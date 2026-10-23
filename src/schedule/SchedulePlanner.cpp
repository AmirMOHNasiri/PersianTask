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
#include <ScheduleEffort.h>
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
Datetime weekStart(const Datetime& day) {
  Datetime start = day.startOfDay();
  while (start.dayOfWeek() != Datetime::weekstart) --start;
  return start.startOfDay();
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
    : start(weekStart(day)) {
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
    // Modes come before this week's changes.
    for (auto& active : state->modes) {
      if (!active.overlaps(start.toEpoch(), end().toEpoch())) continue;

      auto mode = tmpl.mode(active.name);
      if (!mode) {
        notes.push_back(format("Mode '{1}' is ignored: it is no longer in the template.",
                               active.name));
        continue;
      }

      // $name stands for the value given for that param.
      auto bind = [&](const std::string& name) -> std::string {
        if (name.empty() || name[0] != '$') return name;
        for (size_t i = 0; i < mode->params.size() && i < active.values.size(); ++i)
          if (mode->params[i] == name.substr(1)) return active.values[i];
        return "";
      };

      std::string values;
      for (auto& value : active.values) values += (values.empty() ? "" : " ") + value;
      modes.push_back(active.name + (values.empty() ? "" : " " + values));

      for (auto& rule : mode->giveRules) {
        // The slots the rule selects, within the mode's time.  Fixed, rest
        // and protected slots are never given away.
        std::vector<Instance*> selected;
        for (auto& instance : instances) {
          auto& slot = *instance.slot;
          if (slot.kind == Kind::fixed || slot.kind == Kind::rest || slot.isProtected) continue;

          bool matches = rule.selector.substr(0, 5) == "kind:"
                             ? kindName(slot.kind) == rule.selector.substr(5)
                             : slot.category == rule.selector || slot.id == rule.selector ||
                                   slot.everyday == rule.selector;
          if (matches && active.activeAt(instance.day.toEpoch() + instance.start * 60))
            selected.push_back(&instance);
        }

        // 'except N' keeps the first N; 'N%' gives every so often, spread out.
        for (size_t i = 0; i < selected.size(); ++i) {
          bool give = rule.except >= 0 ? i >= static_cast<size_t>(rule.except)
                                       : static_cast<int>((i * rule.percent) % 100) < rule.percent;
          if (give) {
            selected[i]->modeCategory = bind(rule.target);
            selected[i]->mode = active.name;
          }
        }
      }

      for (auto& rule : mode->floorRules) floors[bind(rule.category)] = rule.hours;
    }

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
        instance->borrowed = o.borrowed;
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
double Week::floorOf(const Template& tmpl, const std::string& name) const {
  auto found = floors.find(name);
  if (found != floors.end()) return found->second;
  auto category = tmpl.category(name);
  return category ? category->floor : -1;
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
std::vector<Task> Planner::evaluate(const std::string& filter, const std::vector<Task>& pool) {
  NowGuard now(_reference.toEpoch());

  std::vector<Task> result;
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
  Filter().subset(pool, result);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// The pending tasks matching a filter, most urgent first.
std::vector<Task> Planner::matching(const std::string& filter) {
  auto cached = _cache.find(filter);
  if (cached != _cache.end()) return cached->second;

  // Filters and urgency are evaluated as of the reference time.
  NowGuard now(_reference.toEpoch());
  auto result = evaluate(filter, _pending);

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
const Effort& Planner::effort(const Task& task) {
  auto uuid = task.get("uuid");
  auto found = _efforts.find(uuid);
  if (found != _efforts.end()) return found->second;

  Effort effort;
  effort.estimate = schedule::estimate(task);
  if (effort.estimate > 0) {
    effort.planned = static_cast<long>(effort.estimate * _template.estimateFactor + 0.5);
    effort.tracked = schedule::tracked(task);
    effort.remaining = effort.planned - effort.tracked;
  }
  return _efforts[uuid] = effort;
}

////////////////////////////////////////////////////////////////////////////////
bool Planner::overdue(const Task& task) const {
  return task.has("due") && task.get_date("due") < _reference.toEpoch();
}

////////////////////////////////////////////////////////////////////////////////
bool Planner::eligible(const Task& task, const Instance& instance) const {
  if (!task.has("due") || overdue(task)) return true;
  return instance.day.toEpoch() + instance.start * 60 < task.get_date("due");
}

////////////////////////////////////////////////////////////////////////////////
// The estimated time still to plan, during plan(): the remaining time, less
// what earlier weeks planned, less what this plan gave it so far.
long Planner::left(const Task& task) {
  auto uuid = task.get("uuid");
  if (!_left.count(uuid)) {
    auto carried = _carried.find(uuid);
    _left[uuid] = effort(task).remaining - (carried == _carried.end() ? 0 : carried->second);
  }
  return _left[uuid];
}

////////////////////////////////////////////////////////////////////////////////
std::map<std::string, long> Planner::carried() const {
  auto total = _carried;
  for (auto& entry : _consumed) total[entry.first] += entry.second;
  return total;
}

////////////////////////////////////////////////////////////////////////////////
// A task with remaining estimated time takes slots until that time is
// planned.  Any other task, with no estimate or past its estimate, takes at
// most unestimated.per-day slots a day.  No task takes a slot that starts
// after it is due, unless it is overdue already.
bool Planner::available(const Task& task, const Instance& instance) {
  if (!eligible(task, instance)) return false;

  auto uuid = task.get("uuid");
  if (effort(task).remaining > 0) return left(task) > 0;

  return _used[{uuid, instance.day.toEpoch()}] < _template.unestimatedPerDay;
}

////////////////////////////////////////////////////////////////////////////////
// Only the part of a slot after the reference time plans new work: earlier
// time is either tracked already, or gone.  Nor does the part after the due
// time, for a task not yet overdue.
void Planner::use(const Task& task, const Instance& instance) {
  auto uuid = task.get("uuid");
  ++_used[{uuid, instance.day.toEpoch()}];

  if (effort(task).remaining > 0) {
    auto start = instance.day.toEpoch() + instance.start * 60;
    auto end = instance.day.toEpoch() + instance.end * 60;
    if (task.has("due") && !overdue(task)) end = std::min<time_t>(end, task.get_date("due"));
    auto from = std::max<time_t>(start, _reference.toEpoch());
    if (end > from) {
      left(task);
      _left[uuid] -= end - from;
      _consumed[uuid] += end - from;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// The most urgent task matching the filter that is still available.
std::optional<Task> Planner::pick(const std::string& filter, const Instance& instance) {
  for (auto& task : matching(filter))
    if (available(task, instance)) {
      use(task, instance);
      return task;
    }

  return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
std::string Planner::categoryOf(const Task& task) {
  auto uuid = task.get("uuid");
  return _template.accountFor([&](const Category& category) {
    for (auto& t : matching(category.filter))
      if (t.get("uuid") == uuid) return true;
    return false;
  });
}

////////////////////////////////////////////////////////////////////////////////
std::string Planner::accountingCategory(const Assignment& assignment) {
  if (!assignment.task) return "";

  // A slot's own work counts for its category, if that is a subject with a
  // floor.  Fallback-style categories, such as one for tasks due soon, do not
  // say what the work is about.
  if (assignment.source == Source::slot) {
    auto category = _template.category(assignment.via);
    if (category && category->accounting && category->floor >= 0) return category->name;
  }

  if (assignment.source == Source::slot || assignment.source == Source::fallback ||
      assignment.source == Source::pinned || assignment.source == Source::borrowed)
    return categoryOf(*assignment.task);

  return "";
}

////////////////////////////////////////////////////////////////////////////////
std::map<std::string, int> Planner::minutesByCategory(const std::vector<Assignment>& assignments) {
  std::map<std::string, int> minutes;
  for (auto& assignment : assignments) {
    auto category = accountingCategory(assignment);
    if (!category.empty()) minutes[category] += assignment.instance->minutes();
  }
  return minutes;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<Assignment> Planner::plan(const Week& week) {
  _used.clear();
  _left.clear();
  _consumed.clear();
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
      assignment.source = instance.borrowed ? Source::borrowed : Source::pinned;
      use(*task, instance);
    } else if (!instance.pinnedFilter.empty()) {
      assignment.task = pick(instance.pinnedFilter, instance);
      if (assignment.task) assignment.source = Source::pinned;
    }
  }

  // Then every slot takes a task from its own category, so that one slot's
  // fallback never takes the task another slot was meant for.  Skipped slots
  // go straight to their fallback chain.
  for (auto& assignment : assignments) {
    auto& instance = *assignment.instance;
    if (!takesTasks(instance) || instance.skipped || assignment.task) continue;

    // A mode may have given the slot to another category.
    auto category = instance.slot->category;
    auto filter = _template.filterFor(*instance.slot);
    if (!instance.modeCategory.empty()) {
      auto given = _template.category(instance.modeCategory);
      category = instance.modeCategory;
      filter = given ? given->filter : "";
    }
    if (filter.empty()) continue;

    assignment.task = pick(filter, instance);
    if (assignment.task) {
      assignment.source = Source::slot;
      assignment.via = category;
      assignment.mode = instance.mode;
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

      assignment.task = pick(category->filter, instance);
      if (assignment.task) {
        assignment.source = Source::fallback;
        assignment.via = step;
        break;
      }
    }
  }

  return assignments;
}

////////////////////////////////////////////////////////////////////////////////
WeekPlan::WeekPlan(const Template& tmpl, const Datetime& day, const State* state)
    : week(tmpl, day, state), planner(tmpl, week.reference()) {
  std::map<std::string, long> carried;
  for (auto start = weekStart(schedule::now()); start < week.start;) {
    Week earlier(tmpl, start, state);
    Planner previous(tmpl, earlier.reference());
    previous.carry(carried);
    previous.plan(earlier);
    carried = previous.carried();
    start = earlier.end();
  }
  planner.carry(carried);
  assignments = planner.plan(week);
}

}  // namespace schedule

////////////////////////////////////////////////////////////////////////////////
