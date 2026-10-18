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
    auto cutoff = schedule::now().startOfWeek();
    for (int i = 0; i < 7 * keptWeeks; ++i) --cutoff;
    state.prune(cutoff.startOfDay().toEpoch());

    schedule::commit(before, state, description);
  }
};

// A slot of the week, which can be changed: not an event.
const Instance& slotArg(const Week& week, const std::string& name) {
  auto instance = week.find(name);
  if (!instance) {
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
  if (instance.slot->kind == Kind::fixed)
    throw format("'{1}' is a fixed slot, which can never be {2}.", instance.id, what);
  if (instance.slot->kind == Kind::rest)
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
    if (weekGiven && !date.startOfWeek().sameDay(day.startOfWeek()))
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

  Week week(change.tmpl, day, &change.state);
  Planner planner(change.tmpl, week.reference());
  auto assignments = planner.plan(week);
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
        if (!instance.event && instance.slot->category == name) candidates.push_back(&instance);
  }

  auto key = week.start.toEpoch();
  long taken = 0;
  std::vector<std::string> lent;
  std::vector<std::string> floors;
  for (auto instance : candidates) {
    if (taken >= needed) break;

    auto& slot = *instance->slot;
    if (instance->event || slot.kind == Kind::fixed || slot.kind == Kind::rest ||
        slot.isProtected || !instance->pinnedTask.empty() || !instance->pinnedFilter.empty())
      continue;
    if (instance->day.toEpoch() + instance->start * 60 < now) continue;
    if (std::find_if(lent.begin(), lent.end(), [&](auto& l) {
          return l.substr(0, instance->id.length() + 1) == instance->id + " ";
        }) != lent.end())
      continue;

    // The category of the work the slot would have done.
    auto assignment = std::find_if(assignments.begin(), assignments.end(),
                                   [&](auto& a) { return a.instance == instance; });
    std::string category;
    if (assignment != assignments.end() && assignment->task) {
      if (assignment->task->get("uuid") == uuid) continue;  // Already this task's.
      category = assignment->source == Source::fallback ? assignment->via : slot.category;
    }

    if (auto c = change.tmpl.category(category)) {
      if (c->floor >= 0 && (minutes[category] - instance->minutes()) < c->floor * 60) {
        if (std::find(floors.begin(), floors.end(), category) == floors.end())
          floors.push_back(category);
        continue;
      }
      minutes[category] -= instance->minutes();
    }

    auto& o = change.state.at(key, instance->id);
    o.task = uuid;
    o.filter.clear();
    o.skipped = false;
    o.borrowed = true;
    taken += instance->minutes() * 60L;
    lent.push_back(format("{1} ({2}{3})", instance->id,
                          describePlace(instance->day, instance->start, instance->end),
                          category.empty() ? "" : ", from " + category));
  }

  std::string floorNote;
  if (!floors.empty())
    floorNote = format("  {1} kept at {2} floor.", join(", ", floors),
                       floors.size() == 1 ? "its" : "their");

  if (lent.empty())
    throw format("No slot can lend time to task {1} {2}.  The borrow setting lists {3}; only "
                 "slots that start later this week, and are not fixed, rest, protected or "
                 "assigned, can lend.{4}",
                 first,
                 week.start.sameDay(schedule::now().startOfWeek()) ? "this week" : "that week",
                 join(" ", change.tmpl.borrow), floorNote);

  auto description = format("Borrowed {1} for task {2}, '{3}': {4}", formatHours(taken), first,
                            task.get("description"), join(", ", lent));
  change.commit(description);

  std::stringstream out;
  out << format("Borrowed {1} for task {2}, '{3}':\n", formatHours(taken), first,
                task.get("description"));
  for (auto& l : lent) out << "  " << l << '\n';
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
