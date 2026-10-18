////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_SCHEDULESTARTUP
#define INCLUDED_SCHEDULESTARTUP

class CLI2;

namespace schedule {

// Called by Context::initialize once the configuration and aliases are
// loaded, before the command line is analyzed:
// - plain 'task' shows today's schedule, through CLI2::defaultCommandHook,
// - 'task tasks' runs the user's default.command, unless alias.tasks is set.
void initialize(CLI2&);

}  // namespace schedule

#endif
////////////////////////////////////////////////////////////////////////////////
