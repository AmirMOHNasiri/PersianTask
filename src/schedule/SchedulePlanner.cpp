////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#include <cmake.h>
// cmake.h include header must come first

#include <Context.h>
#include <Filter.h>
#include <Lexer.h>
#include <SchedulePlanner.h>
#include <format.h>

#include <algorithm>

namespace schedule {

////////////////////////////////////////////////////////////////////////////////
Datetime now() {
  // Test-only: rc.schedule.now fixes the current time, so that the tests do
  // not depend on the day they run.  Not for users, and not documented.
  auto fixed = Context::getContext().config.get("schedule.now");
  if (!fixed.empty()) return Datetime(fixed);

  return Datetime();
}

////////////////////////////////////////////////////////////////////////////////
NowGuard::NowGuard(time_t now) : _saved(Datetime::currentTimeOverride) {
  Datetime::currentTimeOverride = now;
}

////////////////////////////////////////////////////////////////////////////////
NowGuard::~NowGuard() { Datetime::currentTimeOverride = _saved; }

////////////////////////////////////////////////////////////////////////////////
CliGuard::CliGuard() : _saved(Context::getContext().cli2) {}

////////////////////////////////////////////////////////////////////////////////
CliGuard::~CliGuard() { Context::getContext().cli2 = std::move(_saved); }

////////////////////////////////////////////////////////////////////////////////
void CliGuard::replaceArgs(const std::vector<std::string>& args, bool useContext) {
  auto& cli = Context::getContext().cli2;
  std::string binary = cli._original_args.empty() ? "task" : cli._original_args[0].attribute("raw");

  cli._original_args.clear();
  cli._args.clear();
  cli._id_ranges.clear();
  cli._uuid_list.clear();
  cli._command.clear();
  cli._context_added = !useContext;

  // One at a time, as Context::initialize adds them.
  cli.add(binary);
  for (auto& arg : args) cli.add(arg);
  cli.analyze();
}

////////////////////////////////////////////////////////////////////////////////
Week::Week(const Template& tmpl, const Datetime& day) : start(day.startOfWeek()) {
  Datetime cursor = start;
  for (int i = 0; i < 7; ++i, ++cursor) {
    std::vector<const Slot*> slots;
    for (auto& slot : tmpl.slots)
      if (slot.weekday == cursor.dayOfWeek()) slots.push_back(&slot);

    std::stable_sort(slots.begin(), slots.end(),
                     [](auto a, auto b) { return a->start < b->start; });

    for (auto slot : slots)
      instances.push_back({static_cast<int>(instances.size()) + 1, slot, cursor.startOfDay(),
                           slot->start, slot->end});
  }
}

////////////////////////////////////////////////////////////////////////////////
std::string Week::label() const {
  auto dateformat = Context::getContext().config.get("dateformat.schedule");
  Datetime end = start;
  for (int i = 0; i < 6; ++i) ++end;

  return format("Week {1}: {2} - {3}", start.week(), start.toString(dateformat),
                end.toString(dateformat));
}

////////////////////////////////////////////////////////////////////////////////
Datetime Week::reference() const {
  auto current = schedule::now();
  Datetime end = start;
  for (int i = 0; i < 7; ++i) ++end;

  return start <= current && current < end ? current : start;
}

////////////////////////////////////////////////////////////////////////////////
Planner::Planner(const Template& tmpl, const Datetime& reference)
    : _template(tmpl), _reference(reference) {
  for (auto& task : Context::getContext().tdb2.pending_tasks())
    if (task.getStatus() == Task::pending && !task.is_blocked) _pending.push_back(task);
}

////////////////////////////////////////////////////////////////////////////////
// The pending tasks matching a filter, most urgent first.
std::vector<Task> Planner::matching(const std::string& filter) {
  auto cached = _cache.find(filter);
  if (cached != _cache.end()) return cached->second;

  // Filters and urgency are evaluated as of the reference time.
  NowGuard now(_reference.toEpoch());

  std::vector<Task> result;
  {
    CliGuard guard;

    // The filter, followed by a command that accepts one.
    std::vector<std::string> args{"("};
    std::string lexeme;
    Lexer::Type type;
    Lexer lexer(filter);
    while (lexer.token(lexeme, type)) args.push_back(lexeme);
    args.push_back(")");
    args.push_back("count");

    // Slots pull tasks regardless of the active context.
    CliGuard::replaceArgs(args, false);
    Filter().subset(_pending, result);
  }

  // Task::urgency is not const, so the scores are computed before sorting.
  std::vector<std::pair<float, Task>> scored;
  for (auto& task : result) {
    task.recalc_urgency = true;
    scored.emplace_back(task.urgency(), task);
  }
  std::stable_sort(scored.begin(), scored.end(), [](auto& a, auto& b) {
    return a.first != b.first ? a.first > b.first : a.second.id < b.second.id;
  });

  result.clear();
  for (auto& entry : scored) result.push_back(entry.second);

  _cache[filter] = result;
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// The most urgent task matching the filter that still has room on that day.
std::optional<Task> Planner::pick(const std::string& filter, const Datetime& day) {
  for (auto& task : matching(filter)) {
    auto& used = _used[{task.get("uuid"), day.toEpoch()}];
    if (used < _template.unestimatedPerDay) {
      ++used;
      return task;
    }
  }

  return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<Assignment> Planner::plan(const Week& week) {
  _used.clear();

  std::vector<Assignment> assignments;
  for (auto& instance : week.instances) {
    Assignment assignment;
    assignment.instance = &instance;
    if (instance.slot->kind == Kind::fixed) {
      assignment.source = Source::fixed;
      assignment.via = instance.slot->category;
    } else if (instance.slot->kind == Kind::rest)
      assignment.source = Source::rest;
    assignments.push_back(assignment);
  }

  // First every slot takes a task from its own category, so that one slot's
  // fallback never takes the task another slot was meant for.
  for (auto& assignment : assignments) {
    auto& slot = *assignment.instance->slot;
    if (slot.kind == Kind::fixed || slot.kind == Kind::rest) continue;

    auto filter = _template.filterFor(slot);
    if (filter.empty()) continue;

    assignment.task = pick(filter, assignment.instance->day);
    if (assignment.task) {
      assignment.source = Source::slot;
      assignment.via = slot.category;
    }
  }

  // Then the slots still empty go down their fallback chains.
  for (auto& assignment : assignments) {
    auto& slot = *assignment.instance->slot;
    if (slot.kind == Kind::fixed || slot.kind == Kind::rest || assignment.task) continue;

    for (auto& step : _template.fallbackFor(slot)) {
      if (step == "rest") {
        assignment.source = Source::rest;
        break;
      }

      auto category = _template.category(step);
      if (!category) continue;

      assignment.task = pick(category->filter, assignment.instance->day);
      if (assignment.task) {
        assignment.source = Source::fallback;
        assignment.via = step;
        break;
      }
    }
  }

  return assignments;
}

}  // namespace schedule

////////////////////////////////////////////////////////////////////////////////
