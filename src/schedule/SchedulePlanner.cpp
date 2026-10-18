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
Week::Week(const Template& tmpl, const Datetime& day, const State* state)
    : start(day.startOfWeek()) {
  // The template's slots, numbered in the template's order of the week.
  Datetime cursor = start;
  for (int i = 0; i < 7; ++i, ++cursor) {
    std::vector<const Slot*> slots;
    for (auto& slot : tmpl.slots)
      if (slot.weekday == cursor.dayOfWeek()) slots.push_back(&slot);

    std::stable_sort(slots.begin(), slots.end(),
                     [](auto a, auto b) { return a->start < b->start; });

    for (auto slot : slots) {
      Instance instance;
      instance.number = static_cast<int>(instances.size()) + 1;
      instance.id = slot->id;
      instance.slot = slot;
      instance.day = cursor.startOfDay();
      instance.start = slot->start;
      instance.end = slot->end;
      instances.push_back(instance);
    }
  }

  if (state) {
    // This week's changes.
    auto week = state->weeks.find(start.toEpoch());
    if (week != state->weeks.end()) {
      for (auto& change : week->second) {
        if (change.second.empty()) continue;

        auto instance = std::find_if(instances.begin(), instances.end(),
                                     [&](auto& i) { return i.id == change.first; });
        if (instance == instances.end()) {
          notes.push_back(format(
              "A change to slot '{1}' is ignored: it is no longer in the template.", change.first));
          continue;
        }

        auto& o = change.second;
        if (o.moved) {
          instance->moved = true;
          instance->day = Datetime(o.day);
          instance->start = o.start;
          instance->end = o.end;
        }
        instance->skipped = o.skipped;
        instance->pinnedTask = o.task;
        instance->pinnedFilter = o.filter;
      }
    }

    // This week's events, which are like fixed slots.
    for (auto& event : state->events) {
      if (event.day < start.toEpoch() || event.day >= end().toEpoch()) continue;

      Slot slot;
      slot.id = "e" + std::to_string(event.id);
      slot.kind = Kind::fixed;
      slot.start = event.start;
      slot.end = event.end;
      _eventSlots.push_back(slot);

      Instance instance;
      instance.id = slot.id;
      instance.slot = &_eventSlots.back();
      instance.event = &event;
      instance.day = Datetime(event.day);
      instance.start = event.start;
      instance.end = event.end;
      instances.push_back(instance);
    }
  }

  std::stable_sort(instances.begin(), instances.end(), [](auto& a, auto& b) {
    if (!a.day.sameDay(b.day)) return a.day < b.day;
    return a.start < b.start;
  });
}

////////////////////////////////////////////////////////////////////////////////
Datetime Week::end() const {
  Datetime end = start;
  for (int i = 0; i < 7; ++i) ++end;
  return end.startOfDay();
}

////////////////////////////////////////////////////////////////////////////////
const Instance* Week::find(const std::string& name) const {
  bool digits = !name.empty() && name.length() < 5 &&
                std::all_of(name.begin(), name.end(), ::isdigit);
  for (auto& instance : instances)
    if (instance.id == name || (digits && instance.number == std::stoi(name))) return &instance;
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
std::string Week::label() const {
  auto dateformat = Context::getContext().config.get("dateformat.schedule");
  Datetime last = start;
  for (int i = 0; i < 6; ++i) ++last;

  return format("Week {1}: {2} - {3}", start.week(), start.toString(dateformat),
                last.toString(dateformat));
}

////////////////////////////////////////////////////////////////////////////////
Datetime Week::reference() const {
  auto current = schedule::now();
  return start <= current && current < end() ? current : start;
}

////////////////////////////////////////////////////////////////////////////////
Planner::Planner(const Template& tmpl, const Datetime& reference)
    : _template(tmpl), _reference(reference) {
  for (auto& task : Context::getContext().tdb2.pending_tasks()) {
    _all.push_back(task);
    if (task.getStatus() == Task::pending && !task.is_blocked) _pending.push_back(task);
  }
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
  notes.clear();

  auto takesTasks = [](const Instance& instance) {
    return instance.slot->kind != Kind::fixed && instance.slot->kind != Kind::rest;
  };

  std::vector<Assignment> assignments;
  for (auto& instance : week.instances) {
    Assignment assignment;
    assignment.instance = &instance;
    if (instance.event)
      assignment.source = Source::event;
    else if (instance.slot->kind == Kind::fixed) {
      assignment.source = Source::fixed;
      assignment.via = instance.slot->category;
    } else if (instance.slot->kind == Kind::rest)
      assignment.source = Source::rest;
    assignments.push_back(assignment);
  }

  // Pinned work comes first.
  for (auto& assignment : assignments) {
    auto& instance = *assignment.instance;
    if (!takesTasks(instance)) continue;

    if (!instance.pinnedTask.empty()) {
      auto task = std::find_if(_all.begin(), _all.end(),
                               [&](Task& t) { return t.get("uuid") == instance.pinnedTask; });
      if (task == _all.end() ||
          (task->getStatus() != Task::pending && task->getStatus() != Task::waiting)) {
        notes.push_back(format("The task assigned to {1} is ignored: it is no longer pending.",
                               instance.id));
        continue;
      }
      assignment.task = *task;
      assignment.source = Source::pinned;
      ++_used[{instance.pinnedTask, instance.day.toEpoch()}];
    } else if (!instance.pinnedFilter.empty()) {
      assignment.task = pick(instance.pinnedFilter, instance.day);
      if (assignment.task) assignment.source = Source::pinned;
    }
  }

  // Then every slot takes a task from its own category, so that one slot's
  // fallback never takes the task another slot was meant for.  Skipped slots
  // go straight to their fallback chain.
  for (auto& assignment : assignments) {
    auto& instance = *assignment.instance;
    if (!takesTasks(instance) || instance.skipped || assignment.task) continue;

    auto filter = _template.filterFor(*instance.slot);
    if (filter.empty()) continue;

    assignment.task = pick(filter, instance.day);
    if (assignment.task) {
      assignment.source = Source::slot;
      assignment.via = instance.slot->category;
    }
  }

  // Then the slots still empty go down their fallback chains.
  for (auto& assignment : assignments) {
    auto& instance = *assignment.instance;
    if (!takesTasks(instance) || assignment.task) continue;

    for (auto& step : _template.fallbackFor(*instance.slot)) {
      if (step == "rest") {
        assignment.source = Source::rest;
        break;
      }

      auto category = _template.category(step);
      if (!category) continue;

      assignment.task = pick(category->filter, instance.day);
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
