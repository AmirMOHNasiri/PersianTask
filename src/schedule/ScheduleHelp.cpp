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

#include <FS.h>
#include <ScheduleHelp.h>
#include <ScheduleTemplate.h>
#include <format.h>

#include <sstream>

namespace schedule {

////////////////////////////////////////////////////////////////////////////////
const std::vector<HelpEntry>& helpEntries() {
  static const std::vector<HelpEntry> entries{
      {"today", "",
       "Today's slots, and the task each one gets",
       "Shows today's slots in time order.  Each slot gets the most urgent pending task\n"
       "matching its category, or else a task from the fallback chain.  Below the slots\n"
       "comes the report named by schedule.report (by default 'important': tasks due\n"
       "today or overdue, and high priority tasks).\n"
       "\n"
       "Plain 'task' shows this too, when a template exists and schedule.default is on.",
       {"task schedule", "task schedule today"}},

      {"week", "[next | prev | <date>]",
       "A whole week of slots and tasks",
       "Shows the week containing today, or the next or previous week, or the week\n"
       "containing a date.  Weeks start on the weekstart setting (Shanbe by default).\n"
       "The number next to each slot can be used in place of its id.",
       {"task schedule week", "task schedule week next", "task schedule week 1405-08-10"}},

      {"init", "[example | blank]",
       "Creates a schedule template",
       "Creates the template file (see 'task schedule help template'), either from the\n"
       "shipped example, a university student's week, or as an almost empty file.  An\n"
       "existing template is never overwritten.",
       {"task schedule init", "task schedule init blank"}},

      {"check", "",
       "Finds mistakes in the template",
       "Reads the template and reports every mistake with its line number, such as an\n"
       "unknown category, overlapping slots, or a filter that does not work.  Run it\n"
       "after editing the template.",
       {"task schedule check"}},

      {"help", "[<command> | template]",
       "This help, or help on one command or on the template format",
       "Without an argument, shows a quick reference.  With a command name, shows its\n"
       "usage and examples.  'task schedule help template' explains the template file.",
       {"task schedule help", "task schedule help week", "task schedule help template"}},
  };
  return entries;
}

////////////////////////////////////////////////////////////////////////////////
const HelpEntry* helpEntry(const std::string& name) {
  for (auto& entry : helpEntries())
    if (entry.name == name) return &entry;
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
std::string help() {
  std::stringstream out;
  out << "Weekly schedule\n"
         "\n"
         "Your week is a template of time slots, such as classes and study blocks.  Each\n"
         "slot pulls a pending task from its category, or from a fallback chain when the\n"
         "category has nothing to do.\n"
         "\n"
         "Template: "
      << Template::path()
      << (File(Template::path()).exists() ? "" : "  (not created yet: run 'task schedule init')")
      << "\n"
         "\n"
         "Commands:\n";

  for (auto& entry : helpEntries()) {
    auto usage =
        "task schedule " + entry.name + (entry.arguments.empty() ? "" : " " + entry.arguments);
    out << format("  {1}\n      {2}\n      e.g. {3}\n", usage, entry.summary, entry.examples[0]);
  }

  out << "\n"
         "Slots are named by their id in the template, such as sat-ds, or by the number\n"
         "shown next to them in 'task schedule week'.\n"
         "\n"
         "More: 'task schedule help <command>', 'task schedule help template', and the full\n"
         "reference in docs/schedule.md.\n";
  return out.str();
}

////////////////////////////////////////////////////////////////////////////////
std::string help(const HelpEntry& entry) {
  std::stringstream out;
  out << "Usage: task schedule " << entry.name
      << (entry.arguments.empty() ? "" : " " + entry.arguments) << "\n\n"
      << entry.summary << ".\n\n"
      << entry.details << "\n\n"
      << "Examples:\n";
  for (auto& example : entry.examples) out << "  " << example << '\n';
  return out.str();
}

////////////////////////////////////////////////////////////////////////////////
std::string helpTemplate() {
  return format(
      "The schedule template\n"
      "\n"
      "The template is a text file, at {1}.\n"
      "It has sections in square brackets, and # starts a comment.\n"
      "\n"
      "  [settings]          estimate.factor, unestimated.per-day, fallback, borrow\n"
      "  [categories]        One per line: <name> <floor> <filter>\n"
      "  [shanbe] ... [jome] The slots of each day, one per line:\n"
      "                      <start>-<end> <id> <kind> <category> [options]\n"
      "  [mode <name>]       A temporary change to the week, such as exams\n"
      "\n"
      "Kinds: fixed (a class, never moved or given tasks), study, floating, buffer,\n"
      "and rest (always free).  Options: filter:\"...\" replaces the category's filter,\n"
      "fallback:\"...\" replaces the fallback chain, optional, and protected.  Times are\n"
      "HH:MM, or wake and sleep (the schedule.wake and schedule.sleep settings).\n"
      "\n"
      "A small example:\n"
      "\n"
      "  [settings]\n"
      "  fallback = soon rest\n"
      "\n"
      "  [categories]\n"
      "  os      4    project:os\n"
      "  soon    -    due.before:now+7d\n"
      "\n"
      "  [shanbe]\n"
      "  09:00-10:30  sat-os-class  fixed  os\n"
      "  14:00-16:00  sat-os-hw     study  os  filter:\"project:os +homework\"\n"
      "\n"
      "Run 'task schedule check' after editing.  The full reference, with the\n"
      "fallback and borrowing rules and modes, is in docs/schedule.md.\n",
      Template::path());
}

}  // namespace schedule

////////////////////////////////////////////////////////////////////////////////
