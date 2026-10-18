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

// Estimates and tracked time, in seconds.
namespace schedule {

// The UDA holding estimates: schedule.estimate.uda, 'estimate' by default.
std::string estimateUda();

// What is wrong with the estimate UDA's definition, or an empty string.
std::string estimateUdaProblem();

// The task's estimate, as written by the user, or -1 if it has none (or the
// estimate UDA is not usable).
long estimate(const Task&);

// Time tracked on the task with start and stop, including a start still
// running, from the task's history.
long tracked(const Task&);

// "2h", "1.5h", "45min".
std::string formatHours(long seconds);

// Hours, such as "2", "1.5", "2h" or "90min", or -1 if invalid.
long parseHours(const std::string&);

}  // namespace schedule

#endif
////////////////////////////////////////////////////////////////////////////////
