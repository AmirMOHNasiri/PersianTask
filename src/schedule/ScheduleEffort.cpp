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
#include <Datetime.h>
#include <Duration.h>
#include <Operation.h>
#include <ScheduleEffort.h>
#include <ScheduleTemplate.h>
#include <format.h>

#include <algorithm>
#include <cstdio>
#include <optional>

namespace schedule {

namespace {

// Seconds of a duration, such as 2h, PT2H or 7200, or -1.
long parseDuration(const std::string& value) {
  double number;
  char extra;
  if (sscanf(value.c_str(), "%lf%c", &number, &extra) == 1) return static_cast<long>(number);

  Duration duration;
  std::string::size_type end = 0;
  if (duration.parse(value, end) && end == value.length()) return duration.toTime_t();
  return -1;
}

}  // namespace

namespace {

// What is wrong with the UDA a setting names, which must be a duration: the
// setting, what it is for, and another name to suggest.
std::string udaProblem(const std::string& setting, const std::string& what,
                       const std::string& example, const std::string& other) {
  auto name = Context::getContext().config.get(setting);
  if (name.empty()) return format("The setting {1} must name a UDA, such as {2}.", setting, example);

  auto type = Context::getContext().config.get("uda." + name + ".type");
  if (type.empty())
    return format(
        "The {1} UDA '{2}' ({3}) is not defined.  Define it with 'task config uda.{2}.type "
        "duration'.",
        what, name, setting);

  if (type != "duration")
    return format(
        "Your configuration defines the UDA '{1}' as a {2}, but the schedule needs {3}s "
        "as durations, such as 2h.  Either make it a duration with 'task config uda.{1}.type "
        "duration', or keep it and give the schedule another UDA: 'task config "
        "{4} {5}' and 'task config uda.{5}.type duration'.",
        name, type, what, setting, other);

  return "";
}

long durationOf(const Task& task, const std::string& name, const std::string& problem) {
  if (!problem.empty() || !task.has(name)) return -1;
  auto seconds = parseDuration(task.get(name));
  return seconds > 0 ? seconds : -1;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
std::string estimateUda() { return Context::getContext().config.get("schedule.estimate.uda"); }

////////////////////////////////////////////////////////////////////////////////
std::string estimateUdaProblem() {
  return udaProblem("schedule.estimate.uda", "estimate", "estimate", "effort");
}

////////////////////////////////////////////////////////////////////////////////
std::string exactUda() { return Context::getContext().config.get("schedule.estimate.exact.uda"); }

////////////////////////////////////////////////////////////////////////////////
std::string exactUdaProblem() {
  return udaProblem("schedule.estimate.exact.uda", "exact estimate", "exact", "exactly");
}

////////////////////////////////////////////////////////////////////////////////
long estimate(const Task& task) { return durationOf(task, estimateUda(), estimateUdaProblem()); }

////////////////////////////////////////////////////////////////////////////////
long exactEstimate(const Task& task) { return durationOf(task, exactUda(), exactUdaProblem()); }

////////////////////////////////////////////////////////////////////////////////
// As 'task info' does: a start is the time in the 'start' value, and a stop
// is when the 'start' value was removed.
std::vector<std::pair<time_t, time_t>> trackedIntervals(const Task& task) {
  auto& replica = Context::getContext().tdb2.replica();
  auto tcoperations = replica->get_task_operations(tc::uuid_from_string(task.get("uuid")));
  auto operations = Operation::operations(tcoperations);
  // Stable: updates in the same second compare equal, and must keep the order
  // they were made in, such as 'task start' and then 'task modify start:...'.
  std::stable_sort(operations.begin(), operations.end());

  std::vector<std::pair<time_t, time_t>> intervals;
  std::optional<time_t> started;
  for (auto& op : operations) {
    if (!op.is_update() || op.get_property() != "start") continue;

    auto value = op.get_value();
    if (value) {
      try {
        started = Datetime(*value).toEpoch();
      } catch (const std::string&) {
        // An invalid start is not counted.
      }
    } else if (started) {
      if (op.get_timestamp() > *started) intervals.emplace_back(*started, op.get_timestamp());
      started.reset();
    }
  }

  if (started && Datetime::currentTime() > *started)
    intervals.emplace_back(*started, Datetime::currentTime());
  return intervals;
}

////////////////////////////////////////////////////////////////////////////////
long tracked(const Task& task) {
  long total = 0;
  for (auto& interval : trackedIntervals(task)) total += interval.second - interval.first;
  return total;
}

////////////////////////////////////////////////////////////////////////////////
std::vector<std::string> estimateWarnings() {
  std::vector<std::string> warnings;
  for (auto& task : Context::getContext().tdb2.pending_tasks()) {
    auto padded = estimate(task);
    auto exact = exactEstimate(task);
    for (auto [seconds, what] : {std::pair{padded, "estimate"}, std::pair{exact, "exact estimate"}})
      if (seconds > 0 && seconds < 5 * 60)
        warnings.push_back(format(
            "Task {1}'s {2}, {3}, looks like it is missing a unit, such as {4}h.", task.id, what,
            formatHours(seconds), seconds < 60 ? seconds : seconds / 60));
    if (padded > 0 && exact > 0)
      warnings.push_back(format("Task {1} has both an estimate, {2}, and an exact one, {3}: the "
                                "exact one is used.",
                                task.id, formatHours(padded), formatHours(exact)));
  }
  return warnings;
}

////////////////////////////////////////////////////////////////////////////////
std::string formatHours(long seconds) {
  if (seconds < 60) return format("{1}s", seconds);
  long minutes = (seconds + 30) / 60;
  if (minutes < 60) return format("{1}min", minutes);

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.2f", minutes / 60.0);
  std::string hours = buffer;
  while (hours.back() == '0') hours.pop_back();
  if (hours.back() == '.') hours.pop_back();
  return hours + "h";
}

////////////////////////////////////////////////////////////////////////////////
long parseHours(const std::string& raw) {
  auto input = asciiDigits(raw);

  double hours;
  char extra;
  if (sscanf(input.c_str(), "%lf%c", &hours, &extra) == 1)
    return hours > 0 ? static_cast<long>(hours * 3600 + 0.5) : -1;

  Duration duration;
  std::string::size_type end = 0;
  if (duration.parse(input, end) && end == input.length() && duration.toTime_t() > 0)
    return duration.toTime_t();
  return -1;
}

}  // namespace schedule

////////////////////////////////////////////////////////////////////////////////
