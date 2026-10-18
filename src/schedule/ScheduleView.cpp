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
#include <util.h>

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
std::string describeEffort(const Effort& effort) {
  if (effort.estimate <= 0) return "";

  auto text = formatHours(effort.estimate) + " (" + formatHours(effort.planned) + ")";
  if (effort.remaining <= 0) return text + ", over";
  if (effort.tracked > 0) text += ", " + formatHours(effort.remaining) + " left";
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
std::string renderTable(const std::vector<Assignment>& assignments, bool days, Planner& planner) {
  Table table;
  table.width(Context::getContext().getWidth());
  table.withColor(Context::getContext().color());
  if (days) table.add("Day");
  table.add("#", false);
  table.add("Time");
  table.add("Slot");
  table.add("ID", false);
  table.add("Task");
  table.add("Estimate");
  table.add("From");
  setHeaderUnderline(table);

  auto current = schedule::now();
  int minutes = current.hour() * 60 + current.minute();
  int column = days ? 1 : 0;

  const Instance* previous = nullptr;
  for (auto& assignment : assignments) {
    auto& instance = *assignment.instance;
    auto& slot = *instance.slot;

    std::string id, description, estimate, from;
    switch (assignment.source) {
      case Source::event:
        description = instance.event->title;
        from = "event";
        break;
      case Source::fixed:
        from = "fixed";
        break;
      case Source::rest:
        from = "rest";
        break;
      case Source::pinned:
        from = "assigned";
        break;
      case Source::borrowed:
        from = "borrowed";
        break;
      case Source::slot:
        from = assignment.via.empty() ? "filter" : assignment.via;
        if (!assignment.mode.empty()) from += " (" + assignment.mode + ")";
        break;
      case Source::fallback:
        from = assignment.via + " (fallback)";
        break;
      case Source::none:
        description = slot.optional ? "" : "nothing to do";
        from = slot.optional ? "optional" : "-";
        break;
    }
    if (instance.skipped) from += ", skipped";

    if (assignment.task) {
      auto task = *assignment.task;
      id = std::to_string(task.id);
      description = task.get("description");
      estimate = describeEffort(planner.effort(task));
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
      table.set(row, 0, describeDay(instance.day), configColor("color.schedule.header"));
    table.set(row, column + 0, instance.number ? std::to_string(instance.number) : "", color);
    table.set(row, column + 1, formatTime(instance.start) + '-' + formatTime(instance.end), color);
    table.set(row, column + 2, instance.id + (instance.moved ? " (moved)" : ""), color);
    table.set(row, column + 3, id, color);
    table.set(row, column + 4, description, color);
    table.set(row, column + 5, estimate, color);
    table.set(row, column + 6, from, color);

    previous = &instance;
  }

  return table.render();
}

////////////////////////////////////////////////////////////////////////////////
std::string renderNotes(const Week& week, const Planner& planner) {
  std::stringstream out;
  for (auto& mode : week.modes) out << "Mode: " << mode << '\n';
  for (auto& note : week.notes) out << "Note: " << note << '\n';
  for (auto& note : planner.notes) out << "Note: " << note << '\n';
  return out.str();
}

}  // namespace schedule

////////////////////////////////////////////////////////////////////////////////
