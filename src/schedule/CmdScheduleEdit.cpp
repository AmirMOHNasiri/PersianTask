////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

// The schedule commands that change it: swap, move, skip, assign, reset,
// event, undo and log.  Each change takes the state lock, reads the state,
// changes it, and records it with the previous state for undo.

#include <cmake.h>
// cmake.h include header must come first

#include <CmdSchedule.h>
#include <Context.h>
#include <Duration.h>
#include <ScheduleEffort.h>
#include <ScheduleHelp.h>
#include <SchedulePlanner.h>
#include <ScheduleState.h>
#include <ScheduleTemplate.h>
#include <ScheduleView.h>
#include <Table.h>
#include <format.h>
#include <shared.h>
#include <util.h>

#include <algorithm>
#include <set>
#include <sstream>

using namespace schedule;

namespace {

// Weeks of changes kept before the current week, for the weekly review.
const int keptWeeks = 8;

std::string usageOf(const std::string& name) {
  auto entry = helpEntry(name);
  return "Usage: task schedule " + entry->name +
         (entry->arguments.empty() ? "" : " " + entry->arguments) + "\nSee 'task schedule help " +
         entry->name + "'.";
}

// A full UUID, such as 8c8a8f4f-2d4d-4f4e-9a52-3a3d6c0a1b2c.
bool looksLikeUuid(const std::string& text) {
  if (text.length() != 36) return false;
  for (size_t i = 0; i < text.length(); ++i) {
    bool dash = i == 8 || i == 13 || i == 18 || i == 23;
    if (dash ? text[i] != '-' : !isxdigit(static_cast<unsigned char>(text[i]))) return false;
  }
  return true;
}

// The change being made: the template, the locked state, and the state before.
struct Change {
  Template tmpl;
  StateLock lock;
  State state;
  State before;

  Change() {
    loadValidTemplate(tmpl);
    state.load();
    before = state;
  }

  void commit(const std::string& description) {
    auto cutoff = weekStart(schedule::now());
    for (int i = 0; i < 7 * keptWeeks; ++i) --cutoff;
    state.prune(cutoff.startOfDay().toEpoch());

    schedule::commit(before, state, description);
  }
};

// A slot of the week, which can be changed: not an event.
const Instance& slotArg(const Week& week, const std::string& name) {
  auto instance = week.find(name);
  if (!instance) {
    // An [everyday] slot is changed one day at a time.
    for (auto& other : week.instances)
      if (!other.event && other.slot->everyday == name)
        throw format("'{1}' is an [everyday] slot, with a copy on each day: name one of them, "
                     "such as '{2}'.",
                     name, other.id);
    throw format("There is no slot '{1}' in the week of {2}.  'task schedule week' lists them.",
                 name, describeDay(week.start));
  }
  if (instance->event)
    throw format("'{1}' is an event.  Use 'task schedule event' to change events.", name);
  return *instance;
}

// Fixed and protected slots never move.
void checkMovable(const Instance& instance) {
  if (instance.slot->kind == Kind::fixed)
    throw format("'{1}' is a fixed slot, which can never be moved.", instance.id);
  if (instance.slot->isProtected)
    throw format("'{1}' is protected, so it is not moved.", instance.id);
}

// Slots that can take work: not fixed, rest, or events.
void checkTakesWork(const Instance& instance, const std::string& what) {
  if (instance.kind == Kind::fixed)
    throw format("'{1}' is a fixed slot, which can never be {2}.  If its class is cancelled, "
                 "'task schedule open {1}' lets it take a task this week.",
                 instance.id, what);
  if (instance.kind == Kind::rest)
    throw format("'{1}' is a rest slot, which never takes work.", instance.id);
}

// The slot or event occupying part of the range, other than the excluded ones.
const Instance* overlapping(const Week& week, const Datetime& day, int start, int end,
                            const std::vector<const Instance*>& excluded) {
  for (auto& other : week.instances) {
    if (std::find(excluded.begin(), excluded.end(), &other) != excluded.end()) continue;
    if (other.day.sameDay(day) && other.start < end && start < other.end) return &other;
  }
  return nullptr;
}

std::string overlapError(const Instance& moving, const Instance& other) {
  if (other.event)
    return format("That would overlap event {1}, '{2}', at {3}.", other.id, other.event->title,
                  describePlace(other.day, other.start, other.end));
  if (other.slot->kind == Kind::fixed)
    return format("That would overlap the fixed slot {1}, at {2}.", other.id,
                  describePlace(other.day, other.start, other.end));
  return format("That would overlap {1}, at {2}.  To exchange their times, use 'task schedule "
                "swap {3} {1}'.",
                other.id, describePlace(other.day, other.start, other.end), moving.id);
}

// Undoes the mutual swap link between a slot and its partner, if any, and
// returns the partner's id if its position was also reset.
std::string unlinkSwap(State& state, time_t week, const std::string& id, bool resetPartner) {
  auto& o = state.at(week, id);
  auto partner = o.swappedWith;
  o.swappedWith.clear();
  if (partner.empty()) return "";

  auto& p = state.at(week, partner);
  if (p.swappedWith != id) return "";
  p.swappedWith.clear();
  if (!resetPartner) return "";

  p.moved = false;
  return partner;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// swap <slot> <slot> [week]
int CmdSchedule::swap(std::vector<std::string> args, std::string& output) {
  Change change;
  auto day = takeWeek(args);
  if (args.size() != 2) throw usageOf("swap");

  Week week(change.tmpl, day, &change.state);
  auto& a = slotArg(week, args[0]);
  auto& b = slotArg(week, args[1]);
  if (&a == &b) throw std::string("A slot cannot be swapped with itself.");
  checkMovable(a);
  checkMovable(b);

  auto key = week.start.toEpoch();
  unlinkSwap(change.state, key, a.id, false);
  unlinkSwap(change.state, key, b.id, false);

  auto& oa = change.state.at(key, a.id);
  auto& ob = change.state.at(key, b.id);
  oa.moved = ob.moved = true;
  oa.day = b.day.toEpoch();
  oa.start = b.start;
  oa.end = b.end;
  ob.day = a.day.toEpoch();
  ob.start = a.start;
  ob.end = a.end;
  oa.swappedWith = b.id;
  ob.swappedWith = a.id;

  // A slot back in its template place needs no move.
  for (auto* instance : {&a, &b}) {
    auto& o = change.state.at(key, instance->id);
    auto& slot = *instance->slot;
    Datetime home = week.start;
    while (home.dayOfWeek() != slot.weekday) ++home;
    if (home.startOfDay().toEpoch() == o.day && slot.start == o.start && slot.end == o.end) {
      o.moved = false;
      o.swappedWith.clear();
    }
  }

  auto description = format("Swapped {1} ({2}) and {3} ({4})", a.id,
                            describePlace(a.day, a.start, a.end), b.id,
                            describePlace(b.day, b.start, b.end));
  change.commit(description);
  output = description + ".\n";
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// move <slot> <day | date> <HH:MM> [week]
int CmdSchedule::move(std::vector<std::string> args, std::string& output) {
  Change change;
  bool weekGiven = !args.empty() && (args.back() == "next" || args.back() == "prev" ||
                                     args.back().substr(0, 5) == "week:");
  auto day = takeWeek(args);
  if (args.size() != 3) throw usageOf("move");

  // The day is a day name in the week, or a date, which then gives the week.
  int weekday = parseDayName(args[1]);
  Datetime date;
  bool isDate = weekday == -1 && parseDay(args[1], date);
  if (weekday == -1 && !isDate)
    throw format("'{1}' is not a day, such as doshanbe, or a date.", args[1]);
  if (isDate) {
    if (weekGiven && !weekStart(date).sameDay(weekStart(day)))
      throw format("{1} is not in the week given.", describeDay(date));
    day = date;
  }

  Week week(change.tmpl, day, &change.state);
  auto& instance = slotArg(week, args[0]);
  checkMovable(instance);

  Datetime target = isDate ? date.startOfDay() : week.start;
  if (!isDate)
    while (target.dayOfWeek() != weekday) ++target;
  target = target.startOfDay();

  int start = parseTime(args[2]);
  if (start == -1 || args[2] == "wake" || args[2] == "sleep")
    throw format("'{1}' is not a time, such as 14:00.", args[2]);
  int end = start + (instance.end - instance.start);

  if (start < Template::wake() || end > Template::sleep())
    throw format("{1} would be outside the day, which is {2}-{3} (schedule.wake and "
                 "schedule.sleep).",
                 describePlace(target, start, end), formatTime(Template::wake()),
                 formatTime(Template::sleep()));

  if (auto other = overlapping(week, target, start, end, {&instance}))
    throw overlapError(instance, *other);

  auto key = week.start.toEpoch();
  unlinkSwap(change.state, key, instance.id, false);
  auto& o = change.state.at(key, instance.id);
  o.moved = true;
  o.day = target.toEpoch();
  o.start = start;
  o.end = end;

  auto& slot = *instance.slot;
  if (target.dayOfWeek() == slot.weekday && start == slot.start) o.moved = false;

  auto description = format("Moved {1} to {2}", instance.id, describePlace(target, start, end));
  change.commit(description);
  output = description + ".\n";
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// skip <slot> [week]
int CmdSchedule::skip(std::vector<std::string> args, std::string& output) {
  Change change;
  auto day = takeWeek(args);
  if (args.size() != 1) throw usageOf("skip");

  Week week(change.tmpl, day, &change.state);
  auto& instance = slotArg(week, args[0]);
  checkTakesWork(instance, "skipped");

  auto& o = change.state.at(week.start.toEpoch(), instance.id);
  o.skipped = true;
  o.task.clear();
  o.filter.clear();
  o.borrowed = false;

  auto description = format("Skipped {1} ({2}): it now takes work from its fallback chain",
                            instance.id, describePlace(instance.day, instance.start, instance.end));
  change.commit(description);
  output = description + ".\n";
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// assign <slot> <task id | uuid | filter> [week]
int CmdSchedule::assign(std::vector<std::string> args, std::string& output) {
  Change change;
  auto day = takeWeek(args);
  if (args.size() < 2) throw usageOf("assign");

  Week week(change.tmpl, day, &change.state);
  auto& instance = slotArg(week, args[0]);
  checkTakesWork(instance, "reassigned");

  auto& o = change.state.at(week.start.toEpoch(), instance.id);
  std::string what;

  auto& first = args[1];
  bool isId = args.size() == 2 && !first.empty() &&
              std::all_of(first.begin(), first.end(), ::isdigit);
  bool isUuid = args.size() == 2 && looksLikeUuid(first);

  if (isId || isUuid) {
    Task task;
    bool found = isId ? Context::getContext().tdb2.get(std::stoi(first), task)
                      : Context::getContext().tdb2.get(first, task);
    if (!found) throw format("There is no task {1}.", first);
    if (task.getStatus() != Task::pending && task.getStatus() != Task::waiting)
      throw format("Task {1} is not pending.", first);

    o.task = task.get("uuid");
    o.filter.clear();
    what = format("task {1}, '{2}'", first, task.get("description"));
  } else {
    auto filter = join(" ", std::vector<std::string>(args.begin() + 1, args.end()));
    Planner(change.tmpl, week.reference()).matching(filter);  // Throws if it does not work.
    o.filter = filter;
    o.task.clear();
    what = format("the filter '{1}'", filter);
  }
  o.skipped = false;
  o.borrowed = false;

  auto description = format("Assigned {1} to {2} ({3})", what, instance.id,
                            describePlace(instance.day, instance.start, instance.end));
  change.commit(description);
  output = description + ".\n";
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// borrow <task> <hours> [week]
//
// Gives the task more time this week, taken from the slots named by the
// template's borrow setting, in that order: a slot id, or a category's slots
// in time order.  Only slots that start after now can lend, and never fixed,
// rest, protected or event slots, or slots already assigned.  A category
// lends only as long as its planned hours stay at or above its floor.
int CmdSchedule::borrow(std::vector<std::string> args, std::string& output) {
  Change change;
  // 'force' may take slots below their categories' floors, if it must.
  auto forceArg = std::find(args.begin(), args.end(), "force");
  bool force = forceArg != args.end();
  if (force) args.erase(forceArg);
  auto day = takeWeek(args);
  if (args.size() != 2) throw usageOf("borrow");

  auto& first = args[0];
  Task task;
  bool found = std::all_of(first.begin(), first.end(), ::isdigit) && !first.empty()
                   ? Context::getContext().tdb2.get(std::stoi(first), task)
                   : looksLikeUuid(first) && Context::getContext().tdb2.get(first, task);
  if (!found) throw format("There is no task {1}.", first);
  if (task.getStatus() != Task::pending && task.getStatus() != Task::waiting)
    throw format("Task {1} is not pending.", first);

  auto needed = parseHours(args[1]);
  if (needed <= 0) throw format("'{1}' is not a number of hours, such as 2 or 1.5h.", args[1]);

  if (change.tmpl.borrow.empty())
    throw std::string(
        "The template has no borrow setting, which lists the slots that lend time, such as\n"
        "'borrow = fri-buffer arch research'.  See 'task schedule help template'.");

  WeekPlan plan(change.tmpl, day, &change.state);
  auto& week = plan.week;
  auto& planner = plan.planner;
  auto& assignments = plan.assignments;
  auto minutes = planner.minutesByCategory(assignments);
  auto uuid = task.get("uuid");
  // Never before the current time, even in a past week.
  auto now = std::max(week.reference().toEpoch(), schedule::now().toEpoch());

  std::vector<const Instance*> candidates;
  for (auto& name : change.tmpl.borrow) {
    if (auto slot = change.tmpl.slot(name)) {
      if (auto instance = week.find(slot->id)) candidates.push_back(instance);
    } else
      for (auto& instance : week.instances)
        if (!instance.event &&
            (instance.slot->category == name || instance.slot->everyday == name))
          candidates.push_back(&instance);
  }

  // The slots that may lend, in borrow order, each once.
  struct Candidate {
    const Instance* instance;
    std::string category;  // Of the work it would do.
    bool critical;         // Its task is critical, so 'force' never takes it.
  };
  std::vector<Candidate> eligible;
  for (auto instance : candidates) {
    auto& slot = *instance->slot;
    if (instance->event || instance->kind == Kind::fixed || instance->kind == Kind::rest ||
        slot.isProtected || !instance->pinnedTask.empty() || !instance->pinnedFilter.empty())
      continue;
    if (instance->day.toEpoch() + instance->start * 60 < now) continue;
    // Never after the task is due, unless it is overdue already.
    if (task.has("due") && task.get_date("due") >= now &&
        instance->day.toEpoch() + instance->start * 60 >= task.get_date("due"))
      continue;
    if (std::any_of(eligible.begin(), eligible.end(),
                    [&](auto& c) { return c.instance == instance; }))
      continue;

    auto assignment = std::find_if(assignments.begin(), assignments.end(),
                                   [&](auto& a) { return a.instance == instance; });
    std::string category;
    bool critical = false;
    if (assignment != assignments.end() && assignment->task) {
      if (assignment->task->get("uuid") == uuid) continue;  // Already this task's.
      category = planner.accountingCategory(*assignment);
      auto pace = planner.pace(*assignment->task);
      critical = pace && pace->critical;
    }
    eligible.push_back({instance, category, critical});
  }

  // A category lends only as long as it stays at or above its floor.
  // Modes may change floors for the week.
  auto keepsFloor = [&](const Candidate& c) {
    auto floor = week.floorOf(change.tmpl, c.category);
    return c.category.empty() || floor < 0 ||
           minutes[c.category] - c.instance->minutes() >= floor * 60;
  };

  auto key = week.start.toEpoch();
  long taken = 0;
  std::vector<std::string> lent;
  std::vector<std::string> floors;
  std::set<const Instance*> used;
  std::vector<std::string> broken;  // Categories taken below their floors.

  // First the slots that keep their category's floor; then, with 'force' and
  // only if that was not enough, the others, but never a critical task's.
  auto take = [&](bool belowFloors) {
    auto takes = [&](const Candidate& c) {
      return !used.count(c.instance) &&
             (belowFloors ? !c.critical && !keepsFloor(c) : keepsFloor(c));
    };
    for (size_t i = 0; i < eligible.size() && taken < needed; ++i) {
      auto& candidate = eligible[i];
      auto instance = candidate.instance;
      if (!takes(candidate)) {
        if (!belowFloors && !used.count(instance) && !keepsFloor(candidate) &&
            std::find(floors.begin(), floors.end(), candidate.category) == floors.end())
          floors.push_back(candidate.category);
        continue;
      }

      // A slot that would overshoot the need by more than half its own
      // length is passed over, if a later slot covers it with less overshoot.
      long length = instance->minutes() * 60L;
      long remaining = needed - taken;
      if (length - remaining > length / 2) {
        bool better = false;
        for (size_t j = i + 1; j < eligible.size() && !better; ++j) {
          long other = eligible[j].instance->minutes() * 60L;
          better = takes(eligible[j]) && other >= remaining &&
                   other - remaining < length - remaining;
        }
        if (better) continue;
      }

      if (belowFloors && std::find(broken.begin(), broken.end(), candidate.category) == broken.end())
        broken.push_back(candidate.category);
      if (!candidate.category.empty()) minutes[candidate.category] -= instance->minutes();

      auto& o = change.state.at(key, instance->id);
      o.task = uuid;
      o.filter.clear();
      o.skipped = false;
      o.borrowed = true;
      o.forced = belowFloors;
      used.insert(instance);
      taken += length;
      lent.push_back(format("{1} ({2}{3}){4}", instance->id,
                            describePlace(instance->day, instance->start, instance->end),
                            candidate.category.empty() ? "" : ", from " + candidate.category,
                            length > remaining
                                ? format(": took {1} for {2} needed", formatHours(length),
                                         formatHours(remaining))
                                : ""));
    }
  };
  take(false);
  if (force && taken < needed) take(true);

  // The floors kept, unless broken after all.
  std::vector<std::string> kept;
  for (auto& category : floors)
    if (std::find(broken.begin(), broken.end(), category) == broken.end()) kept.push_back(category);
  floors = kept;

  std::string floorNote;
  if (!floors.empty())
    floorNote = format("  {1} kept at {2} floor.{3}", join(", ", floors),
                       floors.size() == 1 ? "its" : "their",
                       force ? "" : format("  'task schedule borrow {1} {2} force' may go below.",
                                           first, args[1]));

  if (lent.empty())
    throw format("No slot can lend time to task {1} {2}.  The borrow setting lists {3}; only "
                 "slots that start later this week, before the task is due, and are not fixed, "
                 "rest, protected or assigned, can lend.{4}",
                 first,
                 week.start.sameDay(weekStart(schedule::now())) ? "this week" : "that week",
                 join(" ", change.tmpl.borrow), floorNote);

  auto description = format("Borrowed {1} for task {2}, '{3}': {4}", formatHours(taken), first,
                            task.get("description"), join(", ", lent));
  change.commit(description);

  std::stringstream out;
  out << format("Borrowed {1} for task {2}, '{3}':\n", formatHours(taken), first,
                task.get("description"));
  for (auto& l : lent) out << "  " << l << '\n';
  for (auto& category : broken) {
    auto floor = static_cast<long>(week.floorOf(change.tmpl, category) * 3600);
    auto left = std::max(0L, minutes[category] * 60L);
    out << format("Floor broken: {1} has {2} of its {3} floor this week ({4} below).\n", category,
                  formatHours(left), formatHours(floor), formatHours(floor - left));
  }
  if (taken < needed)
    out << format("Only {1} of {2} could be borrowed.{3}\n", formatHours(taken),
                  formatHours(needed), floorNote);
  else if (!floorNote.empty())
    out << floorNote.substr(2) << '\n';
  out << "'task schedule undo' gives the time back.\n";
  output = out.str();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
namespace {

std::string describeWindow(const ActiveMode& mode) {
  auto from = Datetime(mode.from).toString("A d B Y");
  if (!mode.to) return "from " + from + ", until stopped";
  return "from " + from + " to " + Datetime(mode.to - 1).toString("A d B Y");
}

std::string describeMode(const ActiveMode& mode) {
  std::string values;
  for (auto& value : mode.values) values += " " + value;
  return format("m{1}, {2}{3}", mode.id, mode.name, values);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// mode [list] | mode start <name> <values...> | mode stop <m<N> | name>
int CmdSchedule::mode(std::vector<std::string> args, std::string& output) {
  auto sub = args.empty() ? "list" : args[0];
  auto now = schedule::now();

  if (sub == "list" && args.size() <= 1) {
    Template tmpl;
    loadValidTemplate(tmpl);
    State state;
    state.load();

    std::stringstream out;
    if (tmpl.modes.empty())
      out << "The template defines no modes.  See 'task schedule help template'.\n";
    else {
      out << "Modes in the template:\n";
      for (auto& mode : tmpl.modes) {
        out << "  " << mode.name;
        for (auto& param : mode.params) out << " <" << param << ">";
        out << (mode.active.empty() ? "" : ", active " + mode.active) << '\n';
        for (auto& give : mode.gives) out << "      give " << give.first << '\n';
        for (auto& floor : mode.floors) out << "      floor " << floor.first << '\n';
      }
    }

    if (!state.modes.empty()) {
      out << "\nStarted:\n";
      for (auto& mode : state.modes) {
        auto when = mode.to && mode.to <= now.toEpoch() ? "ended"
                    : mode.from > now.toEpoch()          ? "upcoming"
                                                         : "active";
        out << format("  {1}: {2}, {3}\n", describeMode(mode), when, describeWindow(mode));
      }
    }
    out << "\nStart one with 'task schedule mode start <name> <values>'.\n";
    output = out.str();
    return 0;
  }

  if (sub == "start" && args.size() >= 2) {
    Change change;
    auto mode = change.tmpl.mode(args[1]);
    if (!mode) {
      std::vector<std::string> names;
      for (auto& m : change.tmpl.modes) names.push_back(m.name);
      throw format("The template has no mode '{1}'.  Its modes are: {2}.", args[1],
                   names.empty() ? "none" : join(", ", names));
    }

    std::vector<std::string> values(args.begin() + 2, args.end());
    std::string params;
    for (auto& param : mode->params) params += " <" + param + ">";
    if (values.size() != mode->params.size())
      throw format("Usage: task schedule mode start {1}{2}", mode->name, params);

    // A param used by 'active' is a date; one that gets slots or a floor is
    // a category.
    Datetime date;
    for (size_t i = 0; i < mode->params.size(); ++i) {
      auto& param = mode->params[i];
      if (param == mode->activeParam) {
        if (!parseDay(values[i], date))
          throw format("'{1}' is not a date, for <{2}>.", values[i], param);
      }

      bool category = false;
      for (auto& give : mode->giveRules) category = category || give.target == "$" + param;
      for (auto& floor : mode->floorRules) category = category || floor.category == "$" + param;
      if (category && !change.tmpl.category(values[i]))
        throw format("'{1}' is not a category, for <{2}>.", values[i], param);
    }

    ActiveMode active;
    active.name = mode->name;
    active.values = values;
    switch (mode->activeKind) {
      case Mode::Active::before: {
        Datetime from = date.startOfDay();
        for (int i = 0; i < mode->activeDays; ++i) --from;
        active.from = from.startOfDay().toEpoch();
        active.to = date.startOfDay().toEpoch();
        break;
      }
      case Mode::Active::until: {
        Datetime after = date.startOfDay();
        ++after;
        active.from = now.startOfDay().toEpoch();
        active.to = after.startOfDay().toEpoch();
        break;
      }
      case Mode::Active::always:
        active.from = now.startOfDay().toEpoch();
        break;
    }
    if (active.to && active.to <= now.toEpoch())
      throw format("That {1} mode would already be over.", mode->name);

    active.id = 1;
    for (auto& existing : change.state.modes) active.id = std::max(active.id, existing.id + 1);
    change.state.modes.push_back(active);

    auto description =
        format("Started mode {1}, {2}", describeMode(active), describeWindow(active));
    change.commit(description);
    output = description + ".\n";
    return 0;
  }

  if (sub == "stop" && args.size() == 2) {
    Change change;
    auto& ref = args[1];
    auto id = ref[0] == 'm' ? ref.substr(1) : ref;

    // By id, or the latest mode of that name that has not ended.
    auto found = change.state.modes.end();
    for (auto i = change.state.modes.begin(); i != change.state.modes.end(); ++i)
      if (std::to_string(i->id) == id ||
          (i->name == ref && (!i->to || i->to > now.toEpoch())))
        found = i;
    if (found == change.state.modes.end()) throw format("There is no started mode {1}.", ref);
    if (found->to && found->to <= now.toEpoch())
      throw format("Mode {1} has already ended.", describeMode(*found));

    std::string description;
    if (found->from > now.toEpoch()) {
      description = format("Removed mode {1}, which had not started", describeMode(*found));
      change.state.modes.erase(found);
    } else {
      found->to = now.toEpoch();
      description = format("Stopped mode {1}", describeMode(*found));
    }

    change.commit(description);
    output = description + ".\n";
    return 0;
  }

  throw usageOf("mode");
}

////////////////////////////////////////////////////////////////////////////////
// reset <slot | week> [week]
int CmdSchedule::reset(std::vector<std::string> args, std::string& output) {
  Change change;
  auto day = takeWeek(args);
  if (args.size() != 1) throw usageOf("reset");

  Week week(change.tmpl, day, &change.state);
  auto key = week.start.toEpoch();

  std::string description;
  if (args[0] == "week") {
    change.state.weeks.erase(key);
    description = format("Reset the week of {1} to the template", describeDay(week.start));
  } else {
    auto& instance = slotArg(week, args[0]);
    auto partner = unlinkSwap(change.state, key, instance.id, true);
    change.state.weeks[key].erase(instance.id);

    // Back in its template place, it must not overlap a slot moved there.
    Week after(change.tmpl, day, &change.state);
    for (auto& a : after.instances)
      for (auto& b : after.instances)
        if (&a < &b && a.day.sameDay(b.day) && a.start < b.end && b.start < a.end &&
            (a.id == instance.id || b.id == instance.id || a.id == partner || b.id == partner))
          throw format("Resetting {1} would put it over {2}.  Reset or move {2} first, or reset "
                       "the whole week.",
                       instance.id, a.id == instance.id || a.id == partner ? b.id : a.id);

    description = format("Reset {1} to the template", instance.id);
    if (!partner.empty())
      description += format(", and {1}, which it was swapped with", partner);
  }

  change.commit(description);
  output = description + ".\n";
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// event add <title...> <date> <HH:MM-HH:MM> | event delete <id> | event list
int CmdSchedule::event(std::vector<std::string> args, std::string& output) {
  auto sub = args.empty() ? "list" : args[0];

  if (sub == "list" && args.size() <= 1) {
    State state;
    state.load();
    if (state.events.empty()) {
      output = "No events.  Add one with 'task schedule event add'.\n";
      return 0;
    }

    auto events = state.events;
    std::sort(events.begin(), events.end(), [](auto& a, auto& b) {
      return a.day != b.day ? a.day < b.day : a.start < b.start;
    });

    Table table;
    table.width(Context::getContext().getWidth());
    table.withColor(Context::getContext().color());
    table.add("ID");
    table.add("When");
    table.add("Title");
    setHeaderUnderline(table);
    for (auto& e : events) {
      auto row = table.addRow();
      table.set(row, 0, "e" + std::to_string(e.id));
      table.set(row, 1, describePlace(Datetime(e.day), e.start, e.end));
      table.set(row, 2, e.title);
    }
    output = optionalBlankLine() + table.render() + optionalBlankLine();
    return 0;
  }

  if (sub == "add" && args.size() >= 4) {
    Change change;

    auto range = asciiDigits(args.back());
    auto dash = range.find('-');
    int start = dash == std::string::npos ? -1 : parseTime(range.substr(0, dash));
    int end = dash == std::string::npos ? -1 : parseTime(range.substr(dash + 1));
    if (start == -1 || end == -1)
      throw format("'{1}' is not a time range, such as 17:00-18:00.", args.back());
    if (end <= start) throw format("The time range '{1}' ends before it starts.", args.back());

    Datetime day;
    if (!parseDay(args[args.size() - 2], day))
      throw format("'{1}' is not a date, such as 1405-08-03 or farda.", args[args.size() - 2]);

    auto title = join(" ", std::vector<std::string>(args.begin() + 1, args.end() - 2));

    Event e;
    e.id = 1;
    for (auto& existing : change.state.events) e.id = std::max(e.id, existing.id + 1);
    e.title = title;
    e.day = day.toEpoch();
    e.start = start;
    e.end = end;
    change.state.events.push_back(e);

    std::stringstream out;
    auto description = format("Added event e{1}, '{2}', at {3}", e.id, title,
                              describePlace(day, start, end));
    out << description << ".\n";

    // Appointments come first, but say which slots they cover.
    Week week(change.tmpl, day, &change.state);
    auto self = week.find("e" + std::to_string(e.id));
    for (auto& other : week.instances)
      if (&other != self && other.day.sameDay(day) && other.start < end && start < other.end &&
          !other.event)
        out << format("It overlaps {1} ({2}).  Use 'task schedule skip {1}' or 'move' if needed.\n",
                      other.id, describePlace(other.day, other.start, other.end));

    change.commit(description);
    output = out.str();
    return 0;
  }

  if (sub == "delete" && args.size() == 2) {
    Change change;
    auto id = args[1][0] == 'e' ? args[1].substr(1) : args[1];
    auto e = std::find_if(change.state.events.begin(), change.state.events.end(), [&](auto& e) {
      return std::to_string(e.id) == id;
    });
    if (e == change.state.events.end()) throw format("There is no event {1}.", args[1]);

    auto description = format("Deleted event e{1}, '{2}'", e->id, e->title);
    change.state.events.erase(e);
    change.commit(description);
    output = description + ".\n";
    return 0;
  }

  throw usageOf("event");
}

////////////////////////////////////////////////////////////////////////////////
int CmdSchedule::undo(std::string& output) {
  auto description = schedule::undo();
  output = description.empty() ? "There is nothing to undo.\n" : "Undone: " + description + ".\n";
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
int CmdSchedule::log(std::string& output) {
  auto entries = readLog();
  if (entries.empty()) {
    output = "No schedule changes yet.\n";
    return 0;
  }

  std::stringstream out;
  for (auto i = entries.rbegin(); i != entries.rend(); ++i)
    out << Datetime(i->when).toString("Y-M-D H:N") << "  " << i->description << '\n';
  out << "\n'task schedule undo' undoes the most recent change, at the top.\n";
  output = out.str();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// left <task> <hours>
//
// Corrects a wrong estimate: sets it so that the work still to plan, the
// estimate times estimate.factor less the time tracked, is the given hours:
//   estimate = (hours + tracked) / factor
int CmdSchedule::left(const std::vector<std::string>& args, std::string& output) {
  // 'task schedule' opens the task database read-only, since its own changes
  // are kept in its own files.  This one changes a task, so it reopens it for
  // writing, before reading anything from it.
  auto& context = Context::getContext();
  context.tdb2.open_replica(context.data_dir._data, !context.config.getBoolean("exit.on.missing.db"),
                            true);

  Template tmpl;
  loadValidTemplate(tmpl);
  auto problem = estimateUdaProblem();
  if (!problem.empty()) throw problem;

  auto& first = args[0];
  Task task;
  bool found = !first.empty() && std::all_of(first.begin(), first.end(), ::isdigit)
                   ? Context::getContext().tdb2.get(std::stoi(first), task)
                   : looksLikeUuid(first) && Context::getContext().tdb2.get(first, task);
  if (!found) throw format("There is no task {1}.", first);
  if (task.getStatus() != Task::pending && task.getStatus() != Task::waiting)
    throw format("Task {1} is not pending.", first);

  auto hours = parseHours(args[1]);
  if (hours <= 0)
    throw format("'{1}' is not a number of hours, such as 3 or 1.5h.  When the task is finished, "
                 "use 'task {2} done'.",
                 args[1], first);

  auto tracked = schedule::tracked(task);

  // An exact estimate is simply what is left and what is done.
  auto exact = schedule::exactEstimate(task);
  if (exact > 0) {
    auto estimate = (hours + tracked + 30) / 60 * 60;  // Whole minutes.
    task.set(exactUda(), Duration(estimate).formatISO());
    Context::getContext().tdb2.modify(task);
    output = format(
        "Task {1} '{2}': exact estimate {3}, so that {4} {5} left: {3}, less {6} tracked.  It "
        "was {7}; 'task undo' restores it.\n",
        first, task.get("description"), formatHours(estimate), formatHours(hours),
        hours == 3600 ? "is" : "are", tracked > 0 ? formatHours(tracked) : "0h",
        formatHours(exact));
    return 0;
  }

  auto estimate = static_cast<long>((hours + tracked) / tmpl.estimateFactor + 0.5);
  estimate = (estimate + 30) / 60 * 60;  // Whole minutes.
  auto previous = schedule::estimate(task);
  task.set(estimateUda(), Duration(estimate).formatISO());
  Context::getContext().tdb2.modify(task);

  output = format(
      "Task {1} '{2}': estimate {3}, so that {4} {5} left: {3} times {6} (estimate.factor) is {7}, "
      "less {8} tracked.{9}\n",
      first, task.get("description"), formatHours(estimate), formatHours(hours),
      hours == 3600 ? "is" : "are", (std::ostringstream() << tmpl.estimateFactor).str(),
      formatHours(static_cast<long>(estimate * tmpl.estimateFactor + 0.5)),
      tracked > 0 ? formatHours(tracked) : "0h",
      previous > 0 ? format("  It was {1}; 'task undo' restores it.", formatHours(previous))
                   : "");
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
namespace {

std::string describeSource(Source source, const std::string& via) {
  switch (source) {
    case Source::secondary:
      return via + " (secondary)";
    case Source::fallback:
      return via + " (fallback)";
    default:
      return via;
  }
}

const Assignment* assignmentOf(const WeekPlan& plan, const Instance& instance) {
  for (auto& assignment : plan.assignments)
    if (assignment.instance == &instance) return &assignment;
  return nullptr;
}

std::string taskName(const Task& task) {
  return format("task {1}, '{2}'", task.id, task.get("description"));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// peek <slot> [<n>] [week]
//
// The slot's task, and the next n tasks it would take, in the planner's order.
int CmdSchedule::peek(std::vector<std::string> args, std::string& output) {
  auto day = takeWeek(args);
  if (args.empty() || args.size() > 2) throw usageOf("peek");
  int count = 5;
  if (args.size() == 2) {
    if (args[1].empty() || !std::all_of(args[1].begin(), args[1].end(), ::isdigit))
      throw usageOf("peek");
    count = std::stoi(args[1]);
  }

  Template tmpl;
  loadValidTemplate(tmpl);
  State state;
  state.load();
  WeekPlan plan(tmpl, day, &state);
  auto& instance = slotArg(plan.week, args[0]);
  auto current = assignmentOf(plan, instance);

  std::stringstream out;
  out << format("{1} ({2})", instance.id,
                describePlace(instance.day, instance.start, instance.end));
  if (instance.kind == Kind::fixed || instance.kind == Kind::rest)
    out << format(" is a {1} slot.  Were it to take tasks{2}, it would take:\n\n",
                  kindName(instance.kind),
                  instance.kind == Kind::fixed
                      ? format(" ('task schedule open {1}')", instance.id)
                      : std::string());
  else
    out << ":\n\n";

  Table table;
  table.width(Context::getContext().getWidth());
  table.withColor(Context::getContext().color());
  table.add("");
  table.add("ID", false);
  table.add("Task");
  table.add("From");
  table.add("Estimate");
  table.add("Pace");
  table.add("Note");
  setHeaderUnderline(table);

  auto row = [&](const std::string& mark, const Task& task, const std::string& from,
                 const std::string& note) {
    auto r = table.addRow();
    Assignment a;
    a.instance = &instance;
    a.task = task;
    table.set(r, 0, mark);
    table.set(r, 1, std::to_string(task.id));
    table.set(r, 2, task.get("description"));
    table.set(r, 3, from);
    table.set(r, 4, describeEffort(plan.planner.effort(task), plan.planner.plannedBefore(task)));
    table.set(r, 5, note == "due before this slot" ? "" : paceOf(a, plan.planner));
    table.set(r, 6, note);
  };

  std::string currentUuid;
  if (current && current->task) {
    currentUuid = current->task->get("uuid");
    auto from = current->source == Source::pinned     ? std::string("assigned")
                : current->source == Source::borrowed ? std::string("borrowed")
                                                      : describeSource(current->source,
                                                                       current->via);
    row("*", *current->task, from, "in the slot now");
  }
  // Tasks passed over with 'next' this week say so.
  std::vector<std::string> passed;
  if (auto o = state.find(plan.week.start.toEpoch(), instance.id)) passed = o->passed;
  int shown = 0;
  for (auto& candidate : plan.planner.candidates(instance)) {
    if (shown == count) break;
    auto uuid = candidate.task.get("uuid");
    if (uuid == currentUuid) continue;
    auto note = candidate.why;
    if (std::find(passed.begin(), passed.end(), uuid) != passed.end())
      note = "passed over with 'next'" + (note.empty() ? "" : "; " + note);
    row(std::to_string(++shown), candidate.task,
        describeSource(candidate.source, candidate.via), note);
  }

  if (table.rows() == 0)
    out << "No task matches its categories or its fallback chain.\n";
  else
    out << table.render() << "\n'task schedule next " << instance.id
        << "' gives the slot its next candidate, for this week.\n";
  output = out.str();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// next <slot> [week]
//
// Gives the slot its next candidate, for the week: like assign, but going on
// down the list each time it is repeated.
int CmdSchedule::next(std::vector<std::string> args, std::string& output) {
  Change change;
  auto day = takeWeek(args);
  if (args.size() != 1) throw usageOf("next");

  WeekPlan plan(change.tmpl, day, &change.state);
  auto& instance = slotArg(plan.week, args[0]);
  checkTakesWork(instance, "given a task");
  auto current = assignmentOf(plan, instance);

  auto& o = change.state.at(plan.week.start.toEpoch(), instance.id);
  std::string currentUuid = current && current->task ? current->task->get("uuid") : "";
  if (!currentUuid.empty() &&
      std::find(o.passed.begin(), o.passed.end(), currentUuid) == o.passed.end())
    o.passed.push_back(currentUuid);

  // The first candidate not passed over yet, which may be planned before its
  // due time; limits per day do not hold for a task given by hand.
  const Candidate* chosen = nullptr;
  auto candidates = plan.planner.candidates(instance);
  for (auto& candidate : candidates) {
    auto uuid = candidate.task.get("uuid");
    if (std::find(o.passed.begin(), o.passed.end(), uuid) != o.passed.end()) continue;
    if (candidate.why == "due before this slot") continue;
    chosen = &candidate;
    break;
  }
  if (!chosen)
    throw format("No other task for {1}: 'task schedule reset {1}' starts the list over.",
                 instance.id);

  o.task = chosen->task.get("uuid");
  o.filter.clear();
  o.skipped = false;
  o.borrowed = false;

  auto description =
      format("Next for {1} ({2}): {3}, from {4}{5}", instance.id,
             describePlace(instance.day, instance.start, instance.end), taskName(chosen->task),
             describeSource(chosen->source, chosen->via),
             current && current->task ? ", instead of " + taskName(*current->task) : "");
  change.commit(description);
  output = description + ".\n";
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// open <slot> [HH:MM] [week]
//
// A fixed slot takes tasks this week, from the time given: by default, now if
// it is today, or else the whole slot.  For a class that is cancelled or ends
// early.
int CmdSchedule::open(std::vector<std::string> args, std::string& output) {
  Change change;
  auto day = takeWeek(args);
  if (args.empty() || args.size() > 2) throw usageOf("open");

  Week week(change.tmpl, day, &change.state);
  auto& instance = slotArg(week, args[0]);
  if (instance.opened) throw format("'{1}' is open already, this week.", instance.id);
  if (instance.kind != Kind::fixed)
    throw format("'{1}' is a {2} slot, which takes tasks already.", instance.id,
                 kindName(instance.kind));

  int from = instance.start;
  auto current = schedule::now();
  if (args.size() == 2) {
    from = parseTime(args[1]);
    if (from == -1) throw format("'{1}' is not a time, such as 10:00.", args[1]);
    if (from < instance.start || from >= instance.end)
      throw format("{1} is not within {2} ({3}-{4}).", args[1], instance.id,
                   formatTime(instance.start), formatTime(instance.end));
  } else if (instance.day.sameDay(current)) {
    from = std::max(from, current.hour() * 60 + current.minute());
    if (from >= instance.end) throw format("'{1}' is over already, today.", instance.id);
  }

  change.state.at(week.start.toEpoch(), instance.id).opened = from;
  auto description = format("Opened {1} ({2}) for tasks", instance.id,
                            describePlace(instance.day, from, instance.end));
  change.commit(description);
  output = description + ".  'task schedule undo' closes it again.\n";
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// now
//
// What to work on now: the current slot's task, or the best candidate.
int CmdSchedule::now(std::string& output) {
  Template tmpl;
  loadValidTemplate(tmpl);
  State state;
  state.load();

  auto current = schedule::now();
  WeekPlan plan(tmpl, current.startOfDay(), &state);
  int minutes = current.hour() * 60 + current.minute();

  const Assignment* here = nullptr;
  const Assignment* later = nullptr;
  for (auto& assignment : plan.assignments) {
    auto& instance = *assignment.instance;
    if (!instance.day.sameDay(current)) continue;
    if (instance.start <= minutes && minutes < instance.end) here = &assignment;
    if (instance.start > minutes && !later) later = &assignment;
  }

  std::stringstream out;
  if (!here) {
    out << "No slot now.";
    if (later)
      out << format("  Next: {1} at {2}{3}.", later->instance->id,
                    formatTime(later->instance->start),
                    later->task ? ", " + taskName(*later->task) : std::string());
    out << '\n';
    output = out.str();
    return 0;
  }

  auto& instance = *here->instance;
  auto place = format("{1}, {2}-{3}", instance.id, formatTime(instance.start),
                      formatTime(instance.end));
  if (here->task) {
    auto& task = *here->task;
    out << format("Now ({1}): {2}, from {3}.\n", place, taskName(task),
                  here->source == Source::pinned ? "assigned" : describeSource(here->source, here->via));
    if (!task.has("start"))
      out << format("Start it with 'task {1} start'; 'task schedule next {2}' for another.\n",
                    task.id, instance.id);
    output = out.str();
    return 0;
  }

  // A fixed, rest, event or empty slot: the best candidate, were it to work.
  std::string what = instance.event                    ? "an event"
                     : instance.kind == Kind::fixed     ? "a fixed slot"
                     : instance.kind == Kind::rest      ? "a break"
                                                        : "a slot with nothing to do";
  out << format("Now ({1}): {2}.\n", place, what);
  // The first it could take, or else the first.
  std::vector<Candidate> candidates;
  if (!instance.event) candidates = plan.planner.candidates(instance);
  const Candidate* best = candidates.empty() ? nullptr : &candidates.front();
  for (auto& candidate : candidates)
    if (candidate.why.empty()) {
      best = &candidate;
      break;
    }
  if (best) {
    out << format("The best task for it: {1}, from {2}{3}.\n", taskName(best->task),
                  describeSource(best->source, best->via),
                  best->why.empty() ? "" : " (" + best->why + ")");
    if (instance.kind == Kind::fixed && !instance.event)
      out << format("If the class is cancelled: 'task schedule open {1}'; or start it with 'task "
                    "{2} start'.\n",
                    instance.id, best->task.id);
    else
      out << format("Start it with 'task {1} start'.\n", best->task.id);
  }
  output = out.str();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
