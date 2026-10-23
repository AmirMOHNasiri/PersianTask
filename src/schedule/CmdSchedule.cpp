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

#include <CLI2.h>
#include <CmdSchedule.h>
#include <Context.h>
#include <FS.h>
#include <ScheduleEffort.h>
#include <ScheduleExample.h>
#include <ScheduleHelp.h>
#include <SchedulePlanner.h>
#include <ScheduleStartup.h>
#include <ScheduleState.h>
#include <ScheduleTemplate.h>
#include <ScheduleView.h>
#include <format.h>
#include <shared.h>

#include <algorithm>
#include <sstream>

using namespace schedule;

namespace {

const std::string blankTemplate =
    "# PersianTask weekly schedule.  See 'task schedule help template', and the full\n"
    "# reference in docs/schedule.md.\n"
    "\n"
    "[settings]\n"
    "estimate.factor     = 1.5\n"
    "unestimated.per-day = 1\n"
    "fallback            = soon rest\n"
    "\n"
    "[categories]\n"
    "# name   floor   filter\n"
    "soon     -       due.before:now+7d\n"
    "\n"
    "# Slots: time  id  kind  category  [options]\n"
    "#\n"
    "# [shanbe]\n"
    "# 09:00-10:30   sat-class   fixed   -\n"
    "# 11:00-12:30   sat-study   study   soon\n";

// Plain 'task' shows today's schedule, when there is a template.
std::string defaultCommand(const std::vector<A2>& args) {
  auto& config = Context::getContext().config;

  bool plain = true;
  for (size_t i = 1; i < args.size(); ++i)
    if (!args[i].hasTag("RC") && !args[i].hasTag("CONFIG")) plain = false;

  if (plain && config.getBoolean("schedule") && config.getBoolean("schedule.default") &&
      File(Template::path()).exists())
    return "schedule";

  return config.get("default.command");
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
void schedule::initialize(CLI2& cli) {
  auto& config = Context::getContext().config;

  CLI2::defaultCommandHook = defaultCommand;

#ifdef PERSIANTASK_TESTING
  // Test builds only: keeps a fake current time active for the whole
  // command, to prove that TDB2 refuses to write then.
  if (config.getBoolean("schedule.test.leak-now"))
    Datetime::currentTimeOverride = schedule::now().toEpoch();
#endif

  // 'task tasks' is the usual task list, whatever default.command says.
  if (!config.has("alias.tasks")) {
    auto command = config.get("default.command");
    if (command.empty() || command.substr(0, 8) == "schedule") command = "next";
    cli.alias("tasks", command);
  }
}

////////////////////////////////////////////////////////////////////////////////
CmdSchedule::CmdSchedule() {
  _keyword = "schedule";
  _usage = "task          schedule [<command>]";
  _description = "Weekly schedule: 'task schedule help' for more";
  _read_only = true;  // The schedule's own files are not the task database.
  _displays_id = true;
  _needs_gc = false;
  _needs_recur_update = false;
  _uses_context = false;
  _accepts_filter = false;
  _accepts_modifications = false;
  _accepts_miscellaneous = true;
  _category = Command::Category::report;
}

////////////////////////////////////////////////////////////////////////////////
int CmdSchedule::execute(std::string& output) {
  auto words = Context::getContext().cli2.getWords();
  auto name = words.empty() ? "today" : words[0];
  std::vector<std::string> args(words.begin() + (words.empty() ? 0 : 1), words.end());

  // Only the commands in the help are accepted, so that the help is complete.
  auto entry = helpEntry(name);
  if (!entry) {
    std::vector<std::string> names;
    for (auto& e : helpEntries()) names.push_back(e.name);
    throw format("Unknown schedule command '{1}'.  The commands are: {2}.  Run 'task schedule "
                 "help' for more.",
                 name, join(", ", names));
  }

  if (name == "help") return help(args, output);
  if (name == "init") return init(args, output);

  if (!Context::getContext().config.getBoolean("schedule")) {
    output = "The schedule is turned off.  Turn it on with 'task config schedule on'.\n";
    return 1;
  }

  auto usage = [&]() {
    return std::string("Usage: task schedule ") + entry->name +
           (entry->arguments.empty() ? "" : " " + entry->arguments) +
           "\nSee 'task schedule help " + entry->name + "'.";
  };

  if (name == "today") {
    if (!args.empty()) throw usage();
    return today(output);
  }
  if (name == "week") {
    if (args.size() > 1) throw usage();
    return week(args, output);
  }
  if (name == "check") {
    if (args.size() > 1 || (args.size() == 1 && args[0] != "--expand" && args[0] != "expand"))
      throw usage();
    return check(args, output);
  }
  if (name == "swap") return swap(args, output);
  if (name == "move") return move(args, output);
  if (name == "skip") return skip(args, output);
  if (name == "assign") return assign(args, output);
  if (name == "reset") return reset(args, output);
  if (name == "borrow") return borrow(args, output);
  if (name == "review") return review(args, output);
  if (name == "mode") return mode(args, output);
  if (name == "event") return event(args, output);
  if (name == "undo") {
    if (!args.empty()) throw usage();
    return undo(output);
  }
  if (name == "log") {
    if (!args.empty()) throw usage();
    return log(output);
  }

  throw format("The schedule command '{1}' is not implemented.", name);
}

////////////////////////////////////////////////////////////////////////////////
int CmdSchedule::today(std::string& output) {
  auto& context = Context::getContext();
  int rc = 0;

  // Plain 'task' runs this as the default command.  Shell greetings and
  // scripts call plain 'task', so it never fails because of the template.
  bool plain = false;
  for (auto& arg : context.cli2._args)
    if (arg.hasTag("CMD") && arg.hasTag("DEFAULT")) plain = true;

  Template tmpl;
  if (!tmpl.load()) {
    output = noTemplateHint;
    return 1;
  }

  std::stringstream out;
  out << '\n';
  if (!tmpl.problems.empty()) {
    out << problemsNote(tmpl);
    rc = plain ? 0 : 1;
  } else {
    State state;
    state.load();

    auto day = schedule::now().startOfDay();
    Week week(tmpl, day, &state);
    Planner planner(tmpl, week.reference());

    std::vector<Assignment> todays;
    for (auto& assignment : planner.plan(week))
      if (assignment.instance->day.sameDay(day)) todays.push_back(assignment);

    out << configColor("color.schedule.header")
               .colorize(day.toString(context.config.get("dateformat.schedule")))
        << '\n';
    if (todays.empty())
      out << "No slots today.\n";
    else
      out << renderTable(todays, false, planner);
    out << renderNotes(week, planner);
  }

  // The report below the schedule, run as if it was the command.
  auto report = context.config.get("schedule.report");
  if (!report.empty() && report != "none") {
    auto command = context.commands.find(report);
    if (command == context.commands.end() || command->second->category() != Category::report) {
      out << format("\nThe setting schedule.report must name a report, not '{1}'.\n", report);
    } else {
      std::string tasks;
      {
        CliGuard guard;
        CliGuard::replaceArgs({report}, true);
        command->second->execute(tasks);
      }
      out << '\n' << tasks;
    }
  }

  output = out.str();
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
int CmdSchedule::week(const std::vector<std::string>& args, std::string& output) {
  Template tmpl;
  if (!tmpl.load()) {
    output = noTemplateHint;
    return 1;
  }
  if (!tmpl.problems.empty()) {
    output = problemsNote(tmpl);
    return 1;
  }

  auto day = schedule::now().startOfDay();
  if (args.size() == 1) {
    if (args[0] == "next")
      for (int i = 0; i < 7; ++i) ++day;
    else if (args[0] == "prev")
      for (int i = 0; i < 7; ++i) --day;
    else if (!parseDay(args[0], day))
      throw format("'{1}' is not a date, 'next' or 'prev'.", args[0]);
  }

  State state;
  state.load();
  Week week(tmpl, day, &state);
  Planner planner(tmpl, week.reference());
  auto assignments = planner.plan(week);

  std::stringstream out;
  out << '\n'
      << configColor("color.schedule.header").colorize(week.label()) << "\n\n"
      << renderTable(assignments, true, planner) << '\n'
      << renderHours(tmpl, assignments, planner) << renderNotes(week, planner);

  auto udaProblem = estimateUdaProblem();
  if (!udaProblem.empty()) out << "Note: estimates are ignored.  " << udaProblem << '\n';
  for (auto& warning : estimateWarnings()) out << "Note: " << warning << '\n';
  output = out.str();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
int CmdSchedule::init(const std::vector<std::string>& args, std::string& output) {
  auto which = args.empty() ? "example" : args[0];
  if (args.size() > 1 || (which != "example" && which != "blank"))
    throw std::string("Usage: task schedule init [example | blank]");

  auto path = Template::path();
  if (File(path).exists()) {
    output = format(
        "A schedule template already exists at {1}.\n"
        "Edit it, or remove it to start over.  See 'task schedule help template'.\n",
        path);
    return 1;
  }

  if (!File::write(path, which == "example" ? scheduleExample : blankTemplate))
    throw format("Could not write {1}.", path);

  output = format(
      "Created {1}{2}.\n"
      "\n"
      "Next:\n"
      "  1. Edit it: your categories (which tasks each one pulls) and your slots.\n"
      "  2. Run 'task schedule check' to find mistakes.\n"
      "  3. Run 'task schedule week' to see your week.\n"
      "\n"
      "See 'task schedule help' for the commands, and 'task schedule help template' for\n"
      "the format.\n",
      path, which == "example" ? ", from the example of a university student's week" : "");

  auto udaProblem = estimateUdaProblem();
  if (!udaProblem.empty()) output += "\nNote: " + udaProblem + '\n';
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// The slots of each day, after [everyday] slots are added, for 'check
// --expand': where each one comes from, and which [everyday] slots a day's
// own slot replaces.
static std::string expandedSlots(const Template& tmpl) {
  std::stringstream out;
  for (int i = 0; i < 7; ++i) {
    int day = (Datetime::weekstart + i) % 7;
    out << '\n' << Datetime::dayName(day) << '\n';

    std::vector<const Slot*> slots;
    for (auto& slot : tmpl.slots)
      if (slot.weekday == day) slots.push_back(&slot);
    std::stable_sort(slots.begin(), slots.end(),
                     [](auto a, auto b) { return a->start < b->start; });

    for (auto slot : slots) {
      std::string origin;
      if (!slot->everyday.empty()) origin = "[everyday] " + slot->everyday;
      for (auto& replacement : tmpl.replaced)
        if (replacement.weekday == day &&
            std::find(replacement.by.begin(), replacement.by.end(), slot->id) !=
                replacement.by.end())
          origin += (origin.empty() ? "" : "; ") + std::string("replaces [everyday] ") +
                    replacement.everyday;
      auto line = format("  {1}-{2}  {3} {4} {5} {6}", formatTime(slot->start),
                         formatTime(slot->end), leftJustify(slot->id, 16),
                         leftJustify(kindName(slot->kind), 9),
                         leftJustify(slot->category.empty() ? "-" : slot->category, 10), origin);
      out << line.substr(0, line.find_last_not_of(' ') + 1) << '\n';
    }
    if (slots.empty()) out << "  (no slots)\n";

    for (auto& base : tmpl.everyday)
      if (std::find(base.except.begin(), base.except.end(), day) != base.except.end())
        out << format("  [everyday] {1} does not apply: except:{2}\n", base.id,
                      Datetime::dayName(day));
  }
  return out.str();
}

////////////////////////////////////////////////////////////////////////////////
int CmdSchedule::check(const std::vector<std::string>& args, std::string& output) {
  Template tmpl;
  if (!tmpl.load()) {
    output = noTemplateHint;
    return 1;
  }

  // Filters are checked by running them.
  {
    Planner planner(tmpl, schedule::now());
    auto tryFilter = [&](const std::string& filter, int line, const std::string& what) {
      try {
        planner.matching(filter);
      } catch (const std::string& error) {
        tmpl.problems.push_back(
            {line, format("The filter of {1} does not work: {2}", what, error)});
      }
    };
    for (auto& category : tmpl.categories)
      if (!category.filter.empty())
        tryFilter(category.filter, category.line, "category '" + category.name + "'");
    for (auto list : {&tmpl.slots, &tmpl.everyday})
      for (auto& slot : *list)
        if (!slot.filter.empty() && slot.everyday.empty())
          tryFilter(slot.filter, slot.line, "slot '" + slot.id + "'");
  }

  auto udaProblem = estimateUdaProblem();
  if (!udaProblem.empty()) tmpl.problems.push_back({0, udaProblem});

  std::stringstream out;
  if (!tmpl.problems.empty()) {
    std::stable_sort(tmpl.problems.begin(), tmpl.problems.end(),
                     [](auto& a, auto& b) { return a.line < b.line; });
    for (auto& problem : tmpl.problems) {
      if (problem.line)
        out << Template::path() << ':' << problem.line << ": " << problem.message << '\n';
      else
        out << "Configuration: " << problem.message << '\n';
    }
    out << format("{1} {2}.\n", tmpl.problems.size(),
                  tmpl.problems.size() == 1 ? "problem" : "problems");
    output = out.str();
    return 1;
  }

  std::vector<int> days;
  for (auto& slot : tmpl.slots)
    if (std::find(days.begin(), days.end(), slot.weekday) == days.end())
      days.push_back(slot.weekday);

  out << format("{1}: no problems.  {2} slots on {3} days, {4} categories, {5} modes.\n",
                Template::path(), tmpl.slots.size(), days.size(), tmpl.categories.size(),
                tmpl.modes.size());

  if (!args.empty()) out << expandedSlots(tmpl);

  for (auto& warning : estimateWarnings()) out << "Note: " << warning << '\n';

  // Changes to this week and later weeks that can no longer be honored.
  State state;
  state.load();
  auto thisWeek = weekStart(schedule::now()).toEpoch();
  for (auto& changes : state.weeks) {
    if (changes.first < thisWeek) continue;

    Week week(tmpl, Datetime(changes.first), &state);
    Planner planner(tmpl, week.reference());
    planner.plan(week);

    std::vector<std::string> notes = week.notes;
    notes.insert(notes.end(), planner.notes.begin(), planner.notes.end());
    for (auto& note : notes)
      out << format("Note, week of {1}: {2}\n", describeDay(week.start), note);
  }

  output = out.str();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
int CmdSchedule::help(const std::vector<std::string>& args, std::string& output) {
  if (args.empty()) {
    output = schedule::help();
    return 0;
  }
  if (args.size() == 1 && args[0] == "template") {
    output = helpTemplate();
    return 0;
  }

  auto entry = args.size() == 1 ? helpEntry(args[0]) : nullptr;
  if (!entry)
    throw format("There is no help on '{1}'.  Run 'task schedule help' for the commands.",
                 join(" ", args));

  output = schedule::help(*entry);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
