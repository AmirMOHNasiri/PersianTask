////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_SCHEDULEEFFORT
#define INCLUDED_SCHEDULEEFFORT

#include <Task.h>

#include <string>
#include <vector>

// Estimates and tracked time, in seconds.
namespace schedule {

// The UDA holding estimates: schedule.estimate.uda, 'estimate' by default.
std::string estimateUda();

// What is wrong with the estimate UDA's definition, or an empty string.
std::string estimateUdaProblem();

// The task's estimate, as written by the user, or -1 if it has none (or the
// estimate UDA is not usable).
long estimate(const Task&);

// The UDA holding exact estimates, which are planned as they are, without
// estimate.factor: schedule.estimate.exact.uda, 'exact' by default.  Its
// problem, and a task's exact estimate, likewise.
std::string exactUda();
std::string exactUdaProblem();
long exactEstimate(const Task&);

// Time tracked on the task with start and stop, including a start still
// running, from the task's history.
long tracked(const Task&);

// The same, as intervals of time.
std::vector<std::pair<time_t, time_t>> trackedIntervals(const Task&);

// Warnings for pending tasks whose estimate is so small (under five
// minutes) that it is probably missing a unit, such as 4s for 4h.
std::vector<std::string> estimateWarnings();

// "2h", "1.5h", "45min", "4s".
std::string formatHours(long seconds);

// Hours, such as "2", "1.5", "2h" or "90min", or -1 if invalid.
long parseHours(const std::string&);

}  // namespace schedule

#endif
////////////////////////////////////////////////////////////////////////////////
