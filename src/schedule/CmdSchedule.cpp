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
#include <Color.h>
#include <Context.h>
#include <FS.h>
#include <ScheduleExample.h>
#include <ScheduleHelp.h>
#include <SchedulePlanner.h>
#include <ScheduleTemplate.h>
#include <Table.h>
#include <format.h>
#include <shared.h>
#include <util.h>

#include <algorithm>
#include <sstream>

using namespace schedule;

namespace {

const std::string noTemplateHint =
    "There is no schedule template yet.  Run 'task schedule init' to create one from an\n"
    "example, and 'task schedule help' to learn how the schedule works.\n";

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

Color configColor(const std::string& name) {
  if (!Context::getContext().color()) return Color();
  return Color(Context::getContext().config.get(name));
}

std::string problemsNote(const Template& tmpl) {
  return format("The schedule template has {1} {2}.  Run 'task schedule check' to see {3}.\n",
                tmpl.problems.size(), tmpl.problems.size() == 1 ? "problem" : "problems",
                tmpl.problems.size() == 1 ? "it" : "them");
}

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

// Parses a whole word as a date, such as 'farda' or '1405-08-10'.
bool parseDay(const std::string& word, Datetime& day) {
  Datetime parsed;
  std::string::size_type end = 0;
  if (!parsed.parse(word, end, Context::getContext().config.get("dateformat")) ||
      end != word.length())
    return false;

  day = parsed.startOfDay();
  return true;
}

// Adds a row for each assignment.  With days, the first row of each day
// shows the day, and a blank row separates days.
void addRows(Table& table, const std::vector<Assignment>& assignments, bool days) {
  auto current = schedule::now();
  int minutes = current.hour() * 60 + current.minute();
  int column = days ? 1 : 0;

  const Instance* previous = nullptr;
  for (auto& assignment : assignments) {
    auto& instance = *assignment.instance;
    auto& slot = *instance.slot;

    std::string id, description, from;
    switch (assignment.source) {
      case Source::fixed:
        from = "fixed";
        break;
      case Source::rest:
        from = "rest";
        break;
      case Source::slot:
        from = slot.category.empty() ? "filter" : slot.category;
        break;
      case Source::fallback:
        from = assignment.via + " (fallback)";
        break;
      case Source::none:
        description = slot.optional ? "" : "nothing to do";
        from = slot.optional ? "optional" : "-";
        break;
    }

    if (assignment.task) {
      auto task = *assignment.task;
      id = std::to_string(task.id);
      description = task.get("description");
    }

    bool sameDay = previous && previous->day.sameDay(instance.day);
    if (days && previous && !sameDay) table.addRow();

    bool past = instance.day < current.startOfDay() ||
                (instance.day.sameDay(current) && instance.end <= minutes);
    bool now = instance.day.sameDay(current) && instance.start <= minutes && minutes < instance.end;

    Color color;
    if (now)
      color = configColor("color.schedule.now");
    else if (past)
      color = configColor("color.schedule.past");
    else if (slot.optional && !assignment.task)
      color = configColor("color.schedule.optional");

    auto row = table.addRow();
    if (days && !sameDay)
      table.set(row, 0, instance.day.toString("A d B"), configColor("color.schedule.header"));
    table.set(row, column + 0, instance.number, color);
    table.set(row, column + 1, formatTime(instance.start) + '-' + formatTime(instance.end), color);
    table.set(row, column + 2, slot.id, color);
    table.set(row, column + 3, id, color);
    table.set(row, column + 4, description, color);
    table.set(row, column + 5, from, color);

    previous = &instance;
  }
}

Table makeTable(bool days) {
  Table table;
  table.width(Context::getContext().getWidth());
  table.withColor(Context::getContext().color());
  if (days) table.add("Day");
  table.add("#", false);
  table.add("Time");
  table.add("Slot");
  table.add("ID", false);
  table.add("Task");
  table.add("From");
  setHeaderUnderline(table);
  return table;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
CmdSchedule::CmdSchedule() {
  _keyword = "schedule";
  _usage = "task          schedule [<command>]";
  _description = "Weekly schedule: 'task schedule help' for more";
  _read_only = true;
  _displays_id = true;
  _needs_gc = false;
  _needs_recur_update = false;
  _uses_context = false;
  _accepts_filter = false;
  _accepts_modifications = false;
  _accepts_miscellaneous = true;
  _category = Command::Category::report;

  CLI2::defaultCommandHook = defaultCommand;
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

  if (name == "today") {
    if (!args.empty()) throw std::string("Usage: task schedule today");
    return today(output);
  }
  if (name == "week") return week(args, output);
  if (name == "check") {
    if (!args.empty()) throw std::string("Usage: task schedule check");
    return check(output);
  }

  throw format("The schedule command '{1}' is not implemented yet.", name);
}

////////////////////////////////////////////////////////////////////////////////
int CmdSchedule::today(std::string& output) {
  auto& context = Context::getContext();
  int rc = 0;

  Template tmpl;
  if (!tmpl.load()) {
    output = noTemplateHint;
    return 1;
  }

  std::stringstream out;
  out << '\n';
  if (!tmpl.problems.empty()) {
    out << problemsNote(tmpl);
    rc = 1;
  } else {
    auto day = schedule::now().startOfDay();
    Week week(tmpl, day);

    std::vector<Assignment> todays;
    for (auto& assignment : Planner(tmpl).plan(week))
      if (assignment.instance->day.sameDay(day)) todays.push_back(assignment);

    out << configColor("color.schedule.header")
               .colorize(day.toString(context.config.get("dateformat.schedule")))
        << '\n';
    if (todays.empty())
      out << "No slots today.\n";
    else {
      auto table = makeTable(false);
      addRows(table, todays, false);
      out << table.render();
    }
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
  if (args.size() > 1) throw std::string("Usage: task schedule week [next | prev | <date>]");

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

  Week week(tmpl, day);
  auto table = makeTable(true);
  addRows(table, Planner(tmpl).plan(week), true);

  std::stringstream out;
  out << '\n'
      << configColor("color.schedule.header").colorize(week.label()) << "\n\n"
      << table.render();
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
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
int CmdSchedule::check(std::string& output) {
  Template tmpl;
  if (!tmpl.load()) {
    output = noTemplateHint;
    return 1;
  }

  // Filters are checked by running them.
  {
    Planner planner(tmpl);
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
    for (auto& slot : tmpl.slots)
      if (!slot.filter.empty()) tryFilter(slot.filter, slot.line, "slot '" + slot.id + "'");
  }

  std::stringstream out;
  if (tmpl.problems.empty()) {
    std::vector<int> days;
    for (auto& slot : tmpl.slots)
      if (std::find(days.begin(), days.end(), slot.weekday) == days.end())
        days.push_back(slot.weekday);

    out << format("{1}: no problems.  {2} slots on {3} days, {4} categories, {5} modes.\n",
                  Template::path(), tmpl.slots.size(), days.size(), tmpl.categories.size(),
                  tmpl.modes.size());
    output = out.str();
    return 0;
  }

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
