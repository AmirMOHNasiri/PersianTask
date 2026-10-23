////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_SCHEDULEASK
#define INCLUDED_SCHEDULEASK

#include <Task.h>

namespace schedule {

// Called by 'task add' (before is null) and 'task modify', before the task is
// written: if the task has no estimate and comes into one of the schedule's
// categories, asks for an estimate on the terminal.  Never asks when stdin or
// stdout is not a terminal, or when schedule.estimate.ask is off.
void askEstimate(Task& task, const Task* before);

}  // namespace schedule

#endif
////////////////////////////////////////////////////////////////////////////////
