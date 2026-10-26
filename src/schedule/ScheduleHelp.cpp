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
       "The number next to each slot can be used in place of its id.  A later week is\n"
       "planned as if the work planned before it were done.  No task is planned after\n"
       "it is due, unless it is overdue already.  Tasks with an estimate and a due date\n"
       "are paced: their work is spread over the time they have, and the Pace column\n"
       "says whether each is on track, behind, short of time, or overdue.  On a narrow\n"
       "terminal the table is compact (see the schedule.layout setting).",
       {"task schedule week", "task schedule week next", "task schedule week 1405-08-10"}},

      {"review", "[prev | next | week:<date>]",
       "The weekly review",
       "For each category: the hours planned, the hours tracked with 'task start' and\n"
       "'task stop', and its floor.  Then the categories below their floor two weeks\n"
       "running, what the buffer slots were used for (and whether the plan is too\n"
       "heavy), and the deadlines this week and next.  'prev' (or 'last') reviews last\n"
       "week.",
       {"task schedule review", "task schedule review last"}},

      {"init", "[example | blank]",
       "Creates a schedule template",
       "Creates the template file (see 'task schedule help template'), either from the\n"
       "shipped example, a university student's week, or as an almost empty file.  An\n"
       "existing template is never overwritten.",
       {"task schedule init", "task schedule init blank"}},

      {"check", "[--expand]",
       "Finds mistakes in the template",
       "Reads the template and reports every mistake with its line number, such as an\n"
       "unknown category, overlapping slots, or a filter that does not work.  Run it\n"
       "after editing the template.  With --expand, also lists each day's slots as the\n"
       "schedule uses them: with the copies of [everyday] slots, and what replaces them.",
       {"task schedule check", "task schedule check --expand"}},

      {"swap", "<slot> <slot> [next | prev | week:<date>]",
       "Swaps the times of two slots, for one week",
       "Each slot takes the other's day and time, this week only (or the week given).\n"
       "Fixed and protected slots never move.  'task schedule reset' undoes it for\n"
       "both slots.",
       {"task schedule swap sat-ds mon-arch", "task schedule swap 3 16",
        "task schedule swap sat-ds mon-arch next"}},

      {"move", "<slot> <day | date> <HH:MM> [next | prev | week:<date>]",
       "Moves a slot to another day or time, for one week",
       "The slot keeps its length.  It must stay within the day (schedule.wake to\n"
       "schedule.sleep) and not overlap another slot or an event: to exchange two\n"
       "slots, use swap.  A date instead of a day name also chooses the week.  Fixed\n"
       "and protected slots never move.",
       {"task schedule move sat-ds doshanbe 14:00", "task schedule move 3 1405-08-05 09:00"}},

      {"skip", "<slot> [next | prev | week:<date>]",
       "Skips a slot's own work, for one week",
       "The slot no longer takes work from its own category: it goes down its fallback\n"
       "chain instead, or stays free if the chain ends in rest.",
       {"task schedule skip sat-research", "task schedule skip wed-floating next"}},

      {"assign", "<slot> <task id | filter> [next | prev | week:<date>]",
       "Pins a task, or a filter, to a slot, for one week",
       "The slot works on that task, or on the most urgent task matching the filter,\n"
       "instead of its own category.  Assigning replaces a skip, and the other way round.",
       {"task schedule assign wed-floating 12",
        "task schedule assign fri-buffer project:os +homework"}},

      {"borrow", "<task> <hours> [next | prev | week:<date>]",
       "Gives a task more time this week, from other slots",
       "Takes whole slots for the task, from the template's borrow setting, in that\n"
       "order, until the hours are covered.  Only slots later than now can lend, never\n"
       "fixed, rest, protected or assigned ones, and a category lends only down to its\n"
       "floor.  Shows which slots were taken; 'task schedule undo' gives them back.",
       {"task schedule borrow 12 2", "task schedule borrow 12 1.5h next"}},

      {"now", "",
       "What to work on right now",
       "The current slot's task.  In a fixed slot, a break, or a slot with nothing to\n"
       "do, the best task for the moment, with how to take it: 'task schedule open'\n"
       "for a cancelled class, or 'task <id> start'.",
       {"task schedule now"}},

      {"peek", "<slot> [<n>] [next | prev | week:<date>]",
       "A slot's task, and the next tasks it would take",
       "Lists the slot's task, then the next n (5 by default) tasks it would take, in\n"
       "the planner's order: its own category, its secondary categories, then its\n"
       "fallback chain, tasks behind pace first in each.  Each says where it comes\n"
       "from, its pace, and why it is not planned there now, if so.  For a fixed slot,\n"
       "what it would take if it were open.",
       {"task schedule peek thu-ds", "task schedule peek 12 10"}},

      {"next", "<slot> [next | prev | week:<date>]",
       "Gives a slot its next candidate, for one week",
       "Replaces the slot's task with the next one 'peek' lists, as 'assign' would.\n"
       "Repeating it goes on down the list.  'task schedule reset <slot>' or 'undo'\n"
       "goes back.",
       {"task schedule next thu-ds"}},

      {"open", "<slot> [HH:MM] [next | prev | week:<date>]",
       "Lets a fixed slot take a task, for one week",
       "For a class that is cancelled or ends early: this week, the fixed slot takes\n"
       "tasks like a study slot of its category (or from the fallback chain), from\n"
       "the time given, by default now if it is today, or else the whole slot.\n"
       "'task schedule undo' closes it again.",
       {"task schedule open sun-db", "task schedule open sun-db 10:00"}},

      {"left", "<task> <hours>",
       "Corrects a task's estimate: the hours it still needs",
       "When a task needs more (or less) than its estimate says, give the hours it\n"
       "still needs.  The estimate becomes (hours + time tracked) / estimate.factor,\n"
       "so that the time left to plan is those hours.  A task past its estimate is\n"
       "marked 'over estimate' until then.  'task undo' undoes it, like any change to\n"
       "a task.",
       {"task schedule left 12 3", "task schedule left 12 90min"}},

      {"mode", "[list] | start <name> <values...> | stop <m<N> | name>",
       "Modes, such as exam season, which change the week for a while",
       "A mode is defined in the template: which slots it gives to which category,\n"
       "and which floors it changes, while it is active.  Starting it gives values to\n"
       "its params, such as the course and the exam date, and it ends on its own.\n"
       "Changes made for one week (swap, skip, assign...) still win over a mode.",
       {"task schedule mode start exam os 1405-09-20", "task schedule mode list",
        "task schedule mode stop m1"}},

      {"reset", "<slot | week> [next | prev | week:<date>]",
       "Undoes this week's changes to a slot, or to the whole week",
       "The slot goes back to its place and work in the template.  If it was swapped,\n"
       "the other slot goes back too.",
       {"task schedule reset sat-ds", "task schedule reset week"}},

      {"event", "add <title> <date> <HH:MM-HH:MM> | delete <id> | list",
       "One-time appointments",
       "An event is like a fixed slot on one day: it shows in the schedule, and slots\n"
       "cannot be moved over it.  Adding one over a slot is allowed, and the overlap\n"
       "is reported.  Events are numbered e1, e2, and so on.",
       {"task schedule event add Dentist 1405-08-03 17:00-18:00",
        "task schedule event delete e1", "task schedule event list"}},

      {"undo", "",
       "Undoes the last change to the schedule",
       "Undoes the last swap, move, skip, assign, reset or event change.  It can be\n"
       "repeated.  Note that 'task undo' undoes changes to tasks, not to the schedule.",
       {"task schedule undo"}},

      {"log", "",
       "Lists the changes to the schedule, most recent first",
       "The changes that 'task schedule undo' can undo, with when they were made.",
       {"task schedule log"}},

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
         "Estimates: 'task add Paper project:research estimate:4h' plans 4h times the\n"
         "template's estimate.factor.  An estimate needs a unit, such as 4h or 90min.  In\n"
         "'task schedule borrow', a plain number means hours.  Track your work with\n"
         "'task start' and 'task stop': the time tracked is what the schedule counts as\n"
         "done, and the rest is planned again.  With a due date too, the work is spread\n"
         "over the weeks until then (pacing).\n"
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
      "  [settings]          estimate.factor, unestimated.per-day, fallback, borrow,\n"
      "                      pacing, max.per-day, overdue.critical-days\n"
      "  [categories]        One per line: <name> <floor> <filter>\n"
      "  [shanbe] ... [jome] The slots of each day, one per line:\n"
      "                      <start>-<end> <id> <kind> <category> [options]\n"
      "                      The category can be a list, ds,algo: algo when ds has\n"
      "                      nothing, before the fallback chain.\n"
      "  [everyday]          Slots of every day, such as lunch, written the same way.\n"
      "                      Each day gets a copy, named sat-lunch, sun-lunch, and so\n"
      "                      on.  A day's own slot that overlaps one replaces it on\n"
      "                      that day.  except:jome leaves out days.\n"
      "  [mode <name>]       A temporary change to the week, such as exams\n"
      "\n"
      "Kinds: fixed (a class, never moved or given tasks), study, floating, buffer,\n"
      "and rest (always free).  Options: filter:\"...\" replaces the category's filter,\n"
      "fallback:\"...\" replaces the fallback chain, optional, and protected.  Times are\n"
      "HH:MM, or wake and sleep (the schedule.wake and schedule.sleep settings).\n"
      "\n"
      "In filters, 'now' is the time of the week being planned: the current time\n"
      "for this week, and the start of the week for other weeks.  Inside\n"
      "filter:\"...\", quote a phrase with single quotes: 'two words'.\n"
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
