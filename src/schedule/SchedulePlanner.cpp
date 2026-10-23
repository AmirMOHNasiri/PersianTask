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
#include <functional>

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
    // Once a day: each is a conversion to local time.
    auto weekday = cursor.dayOfWeek();
    auto midnight = cursor.startOfDay();

    std::vector<const Slot*> slots;
    for (auto& slot : tmpl.slots)
      if (slot.weekday == weekday) slots.push_back(&slot);

    std::stable_sort(slots.begin(), slots.end(),
                     [](auto a, auto b) { return a->start < b->start; });

    for (auto slot : slots) {
      Instance instance;
      instance.number = static_cast<int>(instances.size()) + 1;
      instance.id = slot->id;
      instance.slot = slot;
      instance.day = midnight;
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

  // Days are midnights, so they compare as times, without the conversions to
  // local time that sameDay makes.
  std::stable_sort(instances.begin(), instances.end(), [](auto& a, auto& b) {
    if (a.day.toEpoch() != b.day.toEpoch()) return a.day.toEpoch() < b.day.toEpoch();
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
// The tasks of the pool that match the filter, as of the reference time, in
// the pool's order.
static std::vector<Task> evaluateAt(const std::string& filter, const std::vector<Task>& pool,
                                    time_t reference) {
  NowGuard now(reference);

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
// Whether the filter's result can depend on the current time: it has a date
// or a duration (now+7d, eow, 2026-10-20), a virtual tag about dates, or
// refers to urgency.  When unsure, it says yes, which only costs time.
static bool timeDependent(const std::string& filter) {
  static const std::set<std::string> timeTags{
      "READY", "DUE",  "DUETODAY", "TODAY",   "YESTERDAY", "TOMORROW",
      "OVERDUE", "WEEK", "MONTH",  "QUARTER", "YEAR",      "WAITING"};

  std::function<bool(const std::string&)> dependent = [&](const std::string& text) {
    Lexer lexer(text);
    std::string token;
    Lexer::Type type;
    while (lexer.token(token, type)) {
      if (type == Lexer::Type::date || type == Lexer::Type::duration) return true;
      if (token.find("urgency") != std::string::npos) return true;
      if (type == Lexer::Type::tag && timeTags.count(token.substr(1))) return true;
      if (type == Lexer::Type::pair) {
        std::string name, modifier, separator, value;
        if (!Lexer::decomposePair(token, name, modifier, separator, value) || dependent(value))
          return true;
      }
    }
    return false;
  };
  return dependent(filter);
}

////////////////////////////////////////////////////////////////////////////////
Horizon::Horizon(const Template& tmpl, const State* state) : _template(tmpl), _state(state) {
  for (auto& task : Context::getContext().tdb2.pending_tasks())
    if (task.getStatus() == Task::pending && !task.is_blocked && task.has("due") &&
        schedule::estimate(task) > 0)
      _pool.push_back(task);
}

////////////////////////////////////////////////////////////////////////////////
const Week& Horizon::week(const Datetime& start) {
  auto& week = _weeks[start.toEpoch()];
  if (!week) week = std::make_unique<Week>(_template, start, _state);
  return *week;
}

////////////////////////////////////////////////////////////////////////////////
const std::vector<int>& Horizon::matches(const std::string& filter, time_t reference) {
  // A filter that does not depend on the time is evaluated once.
  auto independent = _independent.find(filter);
  if (independent == _independent.end())
    independent = _independent.emplace(filter, !timeDependent(filter)).first;
  if (independent->second) reference = 0;

  auto key = std::make_pair(filter, reference);
  auto found = _matches.find(key);
  if (found != _matches.end()) return found->second;

  std::vector<int> indexes;
  if (!filter.empty() && !_pool.empty()) {
    std::map<std::string, int> byUuid;
    for (size_t i = 0; i < _pool.size(); ++i) byUuid[_pool[i].get("uuid")] = static_cast<int>(i);
    for (auto& task : evaluateAt(filter, _pool, reference))
      indexes.push_back(byUuid[task.get("uuid")]);
  }
  return _matches[key] = indexes;
}

////////////////////////////////////////////////////////////////////////////////
Planner::Planner(const Template& tmpl, const Datetime& reference,
                 std::shared_ptr<Horizon> horizon)
    : _template(tmpl),
      _reference(reference),
      _horizon(horizon ? horizon : std::make_shared<Horizon>(tmpl, nullptr)) {
  for (auto& task : Context::getContext().tdb2.pending_tasks()) {
    _all.push_back(task);
    if (task.getStatus() == Task::pending && !task.is_blocked) _pending.push_back(task);
  }
}

////////////////////////////////////////////////////////////////////////////////
std::vector<Task> Planner::evaluate(const std::string& filter, const std::vector<Task>& pool) {
  return evaluateAt(filter, pool, _reference.toEpoch());
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

  // No task takes more than max.per-day of a day, unless it is critical.
  // A first slot is always allowed, even a longer one.
  auto uuid = task.get("uuid");
  if (_template.maxPerDay > 0 && !critical(uuid)) {
    auto used = _usedTime[{uuid, instance.day.toEpoch()}];
    if (used > 0 && used + instance.minutes() * 60L > _template.maxPerDay) return false;
  }

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
  _usedTime[{uuid, instance.day.toEpoch()}] += instance.minutes() * 60L;

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
  // An ended slot counts for its own category, if that has a floor, as if
  // its own work was done, so that a week's hours do not drop as it passes.
  if (assignment.source == Source::past) {
    auto category = _template.category(assignment.via);
    return category && category->accounting && category->floor >= 0 ? category->name : "";
  }

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
const Pace* Planner::pace(const Task& task) const {
  auto found = _paces.find(task.get("uuid"));
  return found == _paces.end() ? nullptr : &found->second;
}

////////////////////////////////////////////////////////////////////////////////
long Planner::planned(const Task& task) const {
  auto found = _consumed.find(task.get("uuid"));
  return found == _consumed.end() ? 0 : found->second;
}

////////////////////////////////////////////////////////////////////////////////
bool Planner::critical(const std::string& uuid) const {
  auto found = _paces.find(uuid);
  return found != _paces.end() && found->second.critical;
}

////////////////////////////////////////////////////////////////////////////////
bool Planner::contains(const std::string& filter, const std::string& uuid) {
  if (filter.empty()) return false;
  auto found = _matchSets.find(filter);
  if (found == _matchSets.end()) {
    std::set<std::string> uuids;
    for (auto& task : matching(filter)) uuids.insert(task.get("uuid"));
    found = _matchSets.emplace(filter, uuids).first;
  }
  return found->second.count(uuid) > 0;
}

////////////////////////////////////////////////////////////////////////////////
// The filter of the slot's own work: its own, or that of the category a mode
// gave it to.
std::string Planner::homeFilter(const Instance& instance) const {
  if (!instance.modeCategory.empty()) {
    auto given = _template.category(instance.modeCategory);
    return given ? given->filter : "";
  }
  return _template.filterFor(*instance.slot);
}

////////////////////////////////////////////////////////////////////////////////
// For each task with an estimate and a due date: its remaining work, the
// capacity of the slots it may work in until it is due, in this week and the
// next ones up to the horizon, its laxity, and this week's share of the work.
// Also finds the overloads.  One pass over the slots: each slot adds its
// time to the tasks its filters match.
void Planner::computePaces(const Week& week) {
  if (!_template.pacing) return;

  auto& pool = _horizon->pool();
  auto reference = _reference.toEpoch();
  auto graceDays = static_cast<time_t>(_template.overdueCriticalDays) * 86400;

  struct Capacity {
    long own{0}, reachable{0}, ownThisWeek{0}, reachableThisWeek{0};
  };
  std::vector<Capacity> capacity(pool.size());
  std::vector<long> remaining(pool.size(), 0);
  std::vector<time_t> due(pool.size(), 0);
  std::vector<char> overdue(pool.size(), 0);
  time_t lastDue = 0;
  long overdueWork = 0;

  for (size_t i = 0; i < pool.size(); ++i) {
    auto& task = pool[i];
    if (effort(task).remaining <= 0) continue;
    remaining[i] = left(task);
    if (remaining[i] <= 0) continue;
    due[i] = task.get_date("due");

    if (due[i] < reference) {
      // Overdue: critical for a grace period, and then planned as usual.
      overdue[i] = 1;
      if (reference < due[i] + graceDays) {
        Pace pace;
        pace.paced = pace.overdue = pace.critical = true;
        pace.remaining = remaining[i];
        pace.laxity = -remaining[i];
        pace.quota = remaining[i];
        _paces[task.get("uuid")] = pace;
        overdueWork += remaining[i];
      }
      remaining[i] = 0;
      continue;
    }
    lastDue = std::max(lastDue, due[i]);
  }

  // Slots that some task can use, for the overloads: from when, until when,
  // and the earliest due date among the tasks that can use them.
  struct Usable {
    time_t from, to, due;
  };
  std::vector<Usable> usable;

  std::vector<int> stamp(pool.size(), -1);
  int slotNumber = 0;
  Datetime start = week.start;
  for (int k = 0; k < Horizon::weeks && start.toEpoch() < lastDue; ++k) {
    auto& current = k == 0 ? week : _horizon->week(start);
    auto weekReference = k == 0 ? reference : start.toEpoch();

    for (auto& instance : current.instances) {
      auto& slot = *instance.slot;
      if (instance.event || slot.kind == Kind::fixed || slot.kind == Kind::rest ||
          !instance.pinnedTask.empty() || !instance.pinnedFilter.empty())
        continue;

      auto slotStart = instance.day.toEpoch() + instance.start * 60L;
      auto slotEnd = instance.day.toEpoch() + instance.end * 60L;
      auto from = std::max<time_t>(slotStart, weekReference);
      if (slotEnd <= from) continue;

      ++slotNumber;
      time_t earliest = 0;
      auto add = [&](int i, bool own) {
        if (stamp[i] == slotNumber || remaining[i] <= 0 || slotStart >= due[i]) return;
        stamp[i] = slotNumber;
        long seconds = std::min<time_t>(slotEnd, due[i]) - from;
        if (seconds <= 0) return;
        earliest = earliest ? std::min(earliest, due[i]) : due[i];
        auto& c = capacity[i];
        c.reachable += seconds;
        if (k == 0) c.reachableThisWeek += seconds;
        if (own) {
          c.own += seconds;
          if (k == 0) c.ownThisWeek += seconds;
        }
      };

      if (!instance.skipped)
        for (auto i : _horizon->matches(homeFilter(instance), weekReference)) add(i, true);
      for (auto& step : _template.fallbackFor(slot)) {
        if (step == "rest") break;
        if (auto category = _template.category(step))
          for (auto i : _horizon->matches(category->filter, weekReference)) add(i, false);
      }

      if (earliest) usable.push_back({from, slotEnd, earliest});
    }
    start = current.end();
  }

  NowGuard now(reference);
  std::vector<std::pair<time_t, long>> work;  // Due, remaining.
  for (size_t i = 0; i < pool.size(); ++i) {
    if (remaining[i] <= 0) continue;
    auto& c = capacity[i];
    Pace pace;
    pace.paced = true;
    pace.remaining = remaining[i];
    pace.own = c.own;
    pace.reachable = c.reachable;
    pace.laxity = c.reachable - remaining[i];
    pace.critical = pace.laxity <= 0;

    // Spread over its own slots if they are enough, or else over all it can
    // reach, which grow as the due date nears.
    double share = 1;
    if (c.own >= remaining[i] && c.own > 0)
      share = static_cast<double>(c.ownThisWeek) / c.own;
    else if (c.reachable > 0)
      share = static_cast<double>(c.reachableThisWeek) / c.reachable;
    pace.quota = static_cast<long>(remaining[i] * share + 0.5);

    Task task = pool[i];
    task.recalc_urgency = true;
    pace.urgency = task.urgency();
    _paces[pool[i].get("uuid")] = pace;
    work.emplace_back(due[i], remaining[i]);
  }

  // EDF feasibility: by each due date, the work due by then must fit in the
  // slots before it that the tasks due by then can use, each counted once.
  std::sort(work.begin(), work.end());
  long total = overdueWork;
  for (size_t i = 0; i < work.size(); ++i) {
    total += work[i].second;
    if (i + 1 < work.size() && work[i + 1].first == work[i].first) continue;
    auto by = work[i].first;
    long slots = 0;
    for (auto& u : usable)
      if (u.due <= by) slots += std::max<time_t>(0, std::min(u.to, by) - u.from);
    if (total > slots + 60) overloads.push_back({by, total, slots});
  }
}

////////////////////////////////////////////////////////////////////////////////
// The passes, each over the slots in time order:
//   1. pinned work (assign, borrow);
//   2. critical tasks, least laxity first, in any slot they can reach, even
//      one of another category, as long as it is not protected and that
//      category keeps its floor;
//   3. each slot's own category: first its tasks behind pace, least laxity
//      first, then the most urgent;
//   4. slots still empty: tasks behind pace that their fallback chain reaches,
//      least laxity first;
//   5. slots still empty: their fallback chain, in order, most urgent first.
// Passes 2 and 4, and the order of pass 3, are those of pacing = on.
std::vector<Assignment> Planner::plan(const Week& week) {
  _used.clear();
  _usedTime.clear();
  _left.clear();
  _consumed.clear();
  _paces.clear();
  overloads.clear();
  notes.clear();

  // Slots that have ended before the reference time take no task: what was
  // done in them is tracked time, not plans, and they must not use up a
  // task's slots or time for the day.
  auto reference = _reference.toEpoch();
  auto ended = [&](const Instance& instance) {
    return instance.day.toEpoch() + instance.end * 60L <= reference;
  };
  auto takesTasks = [&](const Instance& instance) {
    return instance.slot->kind != Kind::fixed && instance.slot->kind != Kind::rest &&
           !ended(instance);
  };
  auto categoryOfSlot = [](const Instance& instance) {
    return instance.modeCategory.empty() ? instance.slot->category : instance.modeCategory;
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
    else if (ended(instance)) {
      assignment.source = Source::past;
      assignment.via = categoryOfSlot(instance);
    }
    assignments.push_back(assignment);
  }

  // 1. Pinned work comes first.
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

  computePaces(week);

  // Paced tasks, least laxity first, then the most urgent.
  std::vector<Task> paced;
  for (auto& task : _horizon->pool())
    if (pace(task)) paced.push_back(task);
  std::stable_sort(paced.begin(), paced.end(), [&](const Task& a, const Task& b) {
    auto pa = pace(a), pb = pace(b);
    return pa->laxity != pb->laxity ? pa->laxity < pb->laxity : pa->urgency > pb->urgency;
  });
  auto behind = [&](const Task& task) {
    auto p = pace(task);
    return p && planned(task) < p->quota;
  };

  // The fallback step of the slot that reaches the task, if any.
  auto fallbackStep = [&](const Instance& instance, const std::string& uuid) -> std::string {
    for (auto& step : _template.fallbackFor(*instance.slot)) {
      if (step == "rest") break;
      auto category = _template.category(step);
      if (category && contains(category->filter, uuid)) return step;
    }
    return "";
  };

  // 2. Critical tasks.
  if (_template.pacing) {
    std::map<std::string, long> ownMinutes, given;
    for (auto& assignment : assignments)
      if (!assignment.instance->event && assignment.instance->slot->kind != Kind::fixed &&
          assignment.instance->slot->kind != Kind::rest)
        ownMinutes[categoryOfSlot(*assignment.instance)] += assignment.instance->minutes();

    for (auto& task : paced) {
      if (!pace(task)->critical) continue;
      auto uuid = task.get("uuid");
      for (auto& assignment : assignments) {
        auto& instance = *assignment.instance;
        if (!takesTasks(instance) || instance.event || assignment.task) continue;
        if (!available(task, instance)) continue;

        bool own = !instance.skipped && contains(homeFilter(instance), uuid);
        std::string via = own ? categoryOfSlot(instance) : fallbackStep(instance, uuid);
        if (via.empty()) continue;

        if (!own) {
          // Taking another category's slot: never a protected one, and only
          // while that category keeps its floor this week.
          if (instance.slot->isProtected) continue;
          auto category = categoryOfSlot(instance);
          auto floor = week.floorOf(_template, category);
          if (!category.empty() && floor >= 0 &&
              ownMinutes[category] - given[category] - instance.minutes() < floor * 60)
            continue;
          given[category] += instance.minutes();
        }

        assignment.task = task;
        assignment.source = own ? Source::slot : Source::fallback;
        assignment.via = via;
        if (own) assignment.mode = instance.mode;
        use(task, instance);
      }
    }
  }

  // 3. Every slot takes a task from its own category, so that one slot's
  // fallback never takes the task another slot was meant for.  Skipped slots
  // go straight to their fallback chain.
  for (auto& assignment : assignments) {
    auto& instance = *assignment.instance;
    if (!takesTasks(instance) || instance.skipped || assignment.task) continue;

    auto category = categoryOfSlot(instance);
    auto filter = homeFilter(instance);
    if (filter.empty()) continue;

    // Its tasks behind pace first.
    if (_template.pacing)
      for (auto& task : paced)
        if (behind(task) && contains(filter, task.get("uuid")) && available(task, instance)) {
          assignment.task = task;
          use(task, instance);
          break;
        }

    if (!assignment.task) assignment.task = pick(filter, instance);
    if (assignment.task) {
      assignment.source = Source::slot;
      assignment.via = category;
      assignment.mode = instance.mode;
    }
  }

  // 4. Slots still empty help the tasks behind pace that they can reach.
  if (_template.pacing)
    for (auto& assignment : assignments) {
      auto& instance = *assignment.instance;
      if (!takesTasks(instance) || instance.event || assignment.task) continue;

      for (auto& task : paced) {
        if (!behind(task) || !available(task, instance)) continue;
        auto step = fallbackStep(instance, task.get("uuid"));
        if (step.empty()) continue;
        assignment.task = task;
        assignment.source = Source::fallback;
        assignment.via = step;
        use(task, instance);
        break;
      }
    }

  // 5. Then the slots still empty go down their fallback chains.
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

  // Tasks that cannot be finished in time, whatever the plan.
  for (auto& task : paced) {
    auto p = pace(task);
    if (!p->overdue && p->laxity < 0)
      notes.push_back(format("Task {1} '{2}' cannot be finished before it is due: the slots "
                             "it can use until then are {3} short.",
                             task.id, task.get("description"), formatHours(-p->laxity)));
  }

  return assignments;
}

////////////////////////////////////////////////////////////////////////////////
WeekPlan::WeekPlan(const Template& tmpl, const Datetime& day, const State* state)
    : horizon(std::make_shared<Horizon>(tmpl, state)),
      week(tmpl, day, state),
      planner(tmpl, week.reference(), horizon) {
  std::map<std::string, long> carried;
  for (auto start = weekStart(schedule::now()); start < week.start;) {
    Week earlier(tmpl, start, state);
    Planner previous(tmpl, earlier.reference(), horizon);
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
