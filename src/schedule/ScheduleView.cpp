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
#include <ScheduleEffort.h>
#include <ScheduleView.h>
#include <Table.h>
#include <format.h>
#include <utf8.h>
#include <util.h>

#include <algorithm>
#include <set>
#include <sstream>

namespace schedule {

const std::string noTemplateHint =
    "There is no schedule template yet.  Run 'task schedule init' to create one from an\n"
    "example, and 'task schedule help' to learn how the schedule works.\n";

////////////////////////////////////////////////////////////////////////////////
Color configColor(const std::string& name) {
  if (!Context::getContext().color()) return Color();
  return Color(Context::getContext().config.get(name));
}

////////////////////////////////////////////////////////////////////////////////
std::string problemsNote(const Template& tmpl) {
  return format("The schedule template has {1} {2}.  Run 'task schedule check' to see {3}.\n",
                tmpl.problems.size(), tmpl.problems.size() == 1 ? "problem" : "problems",
                tmpl.problems.size() == 1 ? "it" : "them");
}

////////////////////////////////////////////////////////////////////////////////
void loadValidTemplate(Template& tmpl) {
  if (!tmpl.load()) throw noTemplateHint;
  if (!tmpl.problems.empty()) throw problemsNote(tmpl);
}

////////////////////////////////////////////////////////////////////////////////
bool parseDay(const std::string& word, Datetime& day) {
  // Relative dates, such as farda, are relative to the schedule's now.
  NowGuard guard(schedule::now().toEpoch());

  Datetime parsed;
  std::string::size_type end = 0;
  if (!parsed.parse(word, end, Context::getContext().config.get("dateformat")) ||
      end != word.length())
    return false;

  day = parsed.startOfDay();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
Datetime takeWeek(std::vector<std::string>& args) {
  auto day = schedule::now().startOfDay();
  if (args.empty()) return day;

  auto& last = args.back();
  if (last == "next") {
    for (int i = 0; i < 7; ++i) ++day;
  } else if (last == "prev") {
    for (int i = 0; i < 7; ++i) --day;
  } else if (last.substr(0, 5) == "week:") {
    if (!parseDay(last.substr(5), day))
      throw format("'{1}' is not a date, in '{2}'.", last.substr(5), last);
  } else
    return day;

  args.pop_back();
  return day;
}

////////////////////////////////////////////////////////////////////////////////
std::string describeDay(const Datetime& day) { return day.toString("A d B"); }

////////////////////////////////////////////////////////////////////////////////
std::string describePlace(const Datetime& day, int start, int end) {
  return describeDay(day) + ' ' + formatTime(start) + '-' + formatTime(end);
}

////////////////////////////////////////////////////////////////////////////////
std::string describeEffort(const Effort& effort, long before) {
  if (effort.estimate <= 0) return "";

  auto text = formatHours(effort.estimate) + (effort.defaulted ? " default" : "") + " (" +
              formatHours(effort.planned) + ")";
  if (effort.remaining <= 0) return text + ", over estimate";

  // What is left: after the time tracked, and in a later week, after what
  // the weeks before it plan.
  auto left = effort.remaining - before;
  if (left <= 0) return text + ", planned before";
  if (effort.tracked > 0 || before > 0) text += ", " + formatHours(left) + " left";
  return text;
}

////////////////////////////////////////////////////////////////////////////////
std::string renderHours(const Template& tmpl, const std::vector<Assignment>& assignments,
                        Planner& planner) {
  auto minutes = planner.minutesByCategory(assignments);

  std::string line;
  for (auto& category : tmpl.categories) {
    auto found = minutes.find(category.name);
    if (found == minutes.end() || found->second == 0) continue;
    line += (line.empty() ? "" : ", ") + category.name + ' ' + formatHours(found->second * 60L);
  }
  return line.empty() ? "" : "Planned: " + line + '\n';
}

////////////////////////////////////////////////////////////////////////////////
// Hours to the nearest quarter, for pace and overloads.
static std::string quarterHours(long seconds) {
  return formatHours((seconds + 450) / 900 * 900);
}

// What the plan means for the task's deadline, if anything.
std::string paceOf(const Assignment& assignment, const Planner& planner) {
  if (!assignment.task) return "";
  auto& task = *assignment.task;
  if (planner.overdue(task)) return "overdue";
  if (!planner.eligible(task, *assignment.instance)) return "after due";  // Pinned.

  auto pace = planner.pace(task);
  if (!pace) return "";
  if (pace->laxity < 0) return "short " + quarterHours(-pace->laxity);
  auto behind = pace->quota - planner.planned(task);
  if (behind >= 15 * 60) return "behind " + quarterHours(behind);
  return "on track";
}

////////////////////////////////////////////////////////////////////////////////
std::string renderTable(const std::vector<Assignment>& assignments, bool days, Planner& planner) {
  auto& context = Context::getContext();
  auto current = schedule::now();
  int minutes = current.hour() * 60 + current.minute();

  // Each row's cells, in both layouts.  The compact one leaves out Estimate
  // and Pace, which it lists under the table, shortens day names, and
  // marks fallback work with '*' and a mode's with '~'.
  struct Row {
    bool newDay{false};
    std::string day, shortDay, number, time, slot, id, task, estimate, pace, from, shortFrom;
    Color color;
    bool empty{false};  // Found nothing to do.
  };
  std::vector<Row> rows;
  bool fallbackMark = false, secondaryMark = false, modeMark = false;

  const Instance* previous = nullptr;
  for (auto& assignment : assignments) {
    auto& instance = *assignment.instance;
    auto& slot = *instance.slot;
    Row row;

    switch (assignment.source) {
      case Source::event:
        row.task = instance.event->title;
        row.from = row.shortFrom = "event";
        break;
      case Source::fixed:
        row.from = row.shortFrom = "fixed";
        break;
      case Source::rest:
        // A rest slot is a break by design; a slot whose fallback chain ended
        // in rest found nothing to do.
        if (instance.kind != Kind::rest) {
          row.task = slot.optional ? "" : "nothing to do";
          row.from = "rest (fallback)";
          row.empty = true;
        } else
          row.from = row.shortFrom = "rest";
        break;
      case Source::pinned:
        row.from = row.shortFrom = "assigned";
        break;
      case Source::borrowed:
        row.from = row.shortFrom = "borrowed";
        break;
      case Source::slot:
        row.from = row.shortFrom = assignment.via.empty() ? "filter" : assignment.via;
        if (!assignment.mode.empty()) {
          row.from += " (" + assignment.mode + ")";
          row.shortFrom += "~";
          modeMark = true;
        }
        break;
      case Source::secondary:
        row.from = assignment.via + " (secondary)";
        row.shortFrom = assignment.via + "+";
        secondaryMark = true;
        break;
      case Source::fallback:
        row.from = assignment.via + " (fallback)";
        row.shortFrom = assignment.via + "*";
        fallbackMark = true;
        break;
      case Source::past:
        row.from = row.shortFrom = "past";
        break;
      case Source::none:
        row.task = slot.optional ? "" : "nothing to do";
        row.from = slot.optional ? "optional" : "-";
        row.shortFrom = slot.optional ? "optional" : "";
        row.empty = true;
        break;
    }
    if (instance.skipped) {
      row.from += ", skipped";
      row.shortFrom += " skipped";
    }

    if (assignment.task) {
      auto& task = *assignment.task;
      row.id = std::to_string(task.id);
      row.task = task.get("description");
      row.estimate = describeEffort(planner.effort(task), planner.plannedBefore(task));
      row.pace = paceOf(assignment, planner);
    }

    bool sameDay = previous && previous->day.sameDay(instance.day);
    row.newDay = days && previous && !sameDay;
    if (days && !sameDay) {
      row.day = describeDay(instance.day);
      row.shortDay = instance.day.toString("a d");
    }

    bool past = instance.day < current.startOfDay() ||
                (instance.day.sameDay(current) && instance.end <= minutes);
    bool now = instance.day.sameDay(current) && instance.start <= minutes && minutes < instance.end;
    if (now)
      row.color = configColor("color.schedule.now");
    else if (past)
      row.color = configColor("color.schedule.past");
    else if (!assignment.task && (slot.optional || row.empty))
      row.color = configColor("color.schedule.optional");

    row.number = instance.number ? std::to_string(instance.number) : "";
    row.time = formatTime(instance.start) + '-' + formatTime(instance.end);
    row.slot = instance.id + (instance.moved ? " (moved)" : "");
    rows.push_back(row);
    previous = &instance;
  }

  // The Pace column is only shown when it says something.
  bool pace = std::any_of(rows.begin(), rows.end(), [](auto& r) { return !r.pace.empty(); });

  // The full layout, unless it does not fit (or schedule.layout says).
  auto width = context.getWidth();
  auto layout = context.config.get("schedule.layout");
  bool compact = layout == "compact";
  if (layout != "compact" && layout != "full") {
    std::vector<std::vector<std::string>> columns(9);
    for (auto& r : rows) {
      std::vector<std::string> cells{r.day, r.number, r.time, r.slot, r.id, r.task, r.estimate,
                                     r.pace, r.from};
      for (size_t c = 0; c < cells.size(); ++c) columns[c].push_back(cells[c]);
    }
    std::vector<std::string> headers{"Day", "#", "Time", "Slot", "ID", "Task", "Estimate", "Pace",
                                     "From"};
    int needed = -1;
    for (size_t c = 0; c < columns.size(); ++c) {
      if ((c == 0 && !days) || (c == 7 && !pace)) continue;
      unsigned widest = utf8_width(headers[c]);
      for (auto& cell : columns[c]) widest = std::max(widest, utf8_width(cell));
      needed += static_cast<int>(widest) + 1;
    }
    compact = needed > width;
  }

  Table table;
  table.width(width);
  table.withColor(context.color());
  if (days) table.add("Day");
  table.add("#", false);
  table.add("Time");
  table.add("Slot");
  table.add("ID", false);
  table.add("Task");
  if (!compact) {
    table.add("Estimate");
    if (pace) table.add("Pace");
  }
  table.add("From");
  setHeaderUnderline(table);

  for (auto& r : rows) {
    if (r.newDay) table.addRow();
    auto row = table.addRow();
    int column = 0;
    if (days)
      table.set(row, column++, compact ? r.shortDay : r.day, configColor("color.schedule.header"));
    table.set(row, column++, r.number, r.color);
    table.set(row, column++, r.time, r.color);
    table.set(row, column++, r.slot, r.color);
    table.set(row, column++, r.id, r.color);
    table.set(row, column++, compact && r.empty ? "-" : r.task, r.color);
    if (!compact) {
      table.set(row, column++, r.estimate, r.color);
      if (pace) table.set(row, column++, r.pace, r.color);
    }
    table.set(row, column++, compact ? r.shortFrom : r.from, r.color);
  }

  auto output = table.render();
  if (!compact) return output;

  // Compact: the tasks' estimates and pace, once each, under the table.
  Table tasks;
  tasks.width(width);
  tasks.withColor(context.color());
  tasks.add("ID", false);
  tasks.add("Task");
  tasks.add("Estimate");
  if (pace) tasks.add("Pace");
  setHeaderUnderline(tasks);
  std::set<std::string> listed;
  for (auto& r : rows) {
    if (r.id.empty() || (r.estimate.empty() && r.pace.empty()) || !listed.insert(r.id).second)
      continue;
    auto row = tasks.addRow();
    tasks.set(row, 0, r.id);
    tasks.set(row, 1, r.task);
    tasks.set(row, 2, r.estimate);
    if (pace) tasks.set(row, 3, r.pace);
  }

  std::string legend;
  if (secondaryMark) legend += "+ a secondary category";
  if (fallbackMark) legend += std::string(legend.empty() ? "" : ", ") + "* from the fallback chain";
  if (modeMark) legend += std::string(legend.empty() ? "" : ", ") + "~ given by a mode";
  if (!legend.empty()) output += legend + ".\n";
  if (!listed.empty()) output += '\n' + tasks.render();
  return output;
}

////////////////////////////////////////////////////////////////////////////////
std::string renderOverloads(const Planner& planner) {
  std::stringstream out;
  // The first date, and how many more.
  if (!planner.overloads.empty()) {
    auto& first = planner.overloads.front();
    auto more = planner.overloads.size() - 1;
    out << format("Overloaded before {1}: {2} of work, {3} of slots{4}.\n",
                  describeDay(Datetime(first.due)), quarterHours(first.work),
                  quarterHours(first.capacity),
                  more == 0 ? "" : format(" (and {1} later {2})", more, more == 1 ? "date" : "dates"));
  }
  return out.str();
}

////////////////////////////////////////////////////////////////////////////////
std::string renderNotes(const Week& week, const Planner& planner) {
  std::stringstream out;
  out << renderOverloads(planner);
  for (auto& mode : week.modes) out << "Mode: " << mode << '\n';
  for (auto& note : week.notes) out << "Note: " << note << '\n';
  for (auto& note : planner.notes) out << "Note: " << note << '\n';
  return out.str();
}

}  // namespace schedule

////////////////////////////////////////////////////////////////////////////////
