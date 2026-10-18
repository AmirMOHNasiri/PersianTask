////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_SCHEDULEHELP
#define INCLUDED_SCHEDULEHELP

#include <string>
#include <vector>

namespace schedule {

// The 'task schedule' commands.  This is the one list of them: the command
// dispatcher only accepts commands found here, and the tests check that the
// documentation mentions each of them.
struct HelpEntry {
  std::string name;
  std::string arguments;
  std::string summary;
  std::string details;
  std::vector<std::string> examples;
};

const std::vector<HelpEntry>& helpEntries();
const HelpEntry* helpEntry(const std::string&);

std::string help();
std::string help(const HelpEntry&);
std::string helpTemplate();

}  // namespace schedule

#endif
////////////////////////////////////////////////////////////////////////////////
