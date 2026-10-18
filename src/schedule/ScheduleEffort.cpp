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

////////////////////////////////////////////////////////////////////////////////
std::string estimateUda() { return Context::getContext().config.get("schedule.estimate.uda"); }

////////////////////////////////////////////////////////////////////////////////
std::string estimateUdaProblem() {
  auto name = estimateUda();
  if (name.empty()) return "The setting schedule.estimate.uda must name a UDA, such as estimate.";

  auto type = Context::getContext().config.get("uda." + name + ".type");
  if (type.empty())
    return format(
        "The estimate UDA '{1}' (schedule.estimate.uda) is not defined.  Define it with "
        "'task config uda.{1}.type duration'.",
        name);

  if (type != "duration")
    return format(
        "Your configuration defines the UDA '{1}' as a {2}, but the schedule needs estimates "
        "as durations, such as 2h.  Either make it a duration with 'task config uda.{1}.type "
        "duration', or keep it and give the schedule another UDA: 'task config "
        "schedule.estimate.uda effort' and 'task config uda.effort.type duration'.",
        name, type);

  return "";
}

////////////////////////////////////////////////////////////////////////////////
long estimate(const Task& task) {
  auto name = estimateUda();
  if (!estimateUdaProblem().empty() || !task.has(name)) return -1;

  auto seconds = parseDuration(task.get(name));
  return seconds > 0 ? seconds : -1;
}

////////////////////////////////////////////////////////////////////////////////
// As 'task info' does: a start is the time in the 'start' value, and a stop
// is when the 'start' value was removed.
long tracked(const Task& task) {
  auto& replica = Context::getContext().tdb2.replica();
  auto tcoperations = replica->get_task_operations(tc::uuid_from_string(task.get("uuid")));
  auto operations = Operation::operations(tcoperations);
  std::sort(operations.begin(), operations.end());

  long total = 0;
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
      total += std::max<long>(0, op.get_timestamp() - *started);
      started.reset();
    }
  }

  if (started) total += std::max<long>(0, Datetime::currentTime() - *started);
  return total;
}

////////////////////////////////////////////////////////////////////////////////
std::string formatHours(long seconds) {
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
