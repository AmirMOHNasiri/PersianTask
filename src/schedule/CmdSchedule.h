////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_CMDSCHEDULE
#define INCLUDED_CMDSCHEDULE

#include <Command.h>

#include <string>
#include <vector>

// task schedule: the weekly schedule.  See docs/schedule.md.
class CmdSchedule : public Command {
 public:
  CmdSchedule();
  int execute(std::string&);

 private:
  int today(std::string&);
  int week(const std::vector<std::string>&, std::string&);
  int init(const std::vector<std::string>&, std::string&);
  int check(const std::vector<std::string>&, std::string&);
  int help(const std::vector<std::string>&, std::string&);

  // Changes, in CmdScheduleEdit.cpp.
  int swap(std::vector<std::string>, std::string&);
  int move(std::vector<std::string>, std::string&);
  int skip(std::vector<std::string>, std::string&);
  int assign(std::vector<std::string>, std::string&);
  int reset(std::vector<std::string>, std::string&);
  int borrow(std::vector<std::string>, std::string&);
  int mode(std::vector<std::string>, std::string&);

  // The weekly review, in CmdScheduleReview.cpp.
  int review(std::vector<std::string>, std::string&);
  int event(std::vector<std::string>, std::string&);
  int undo(std::string&);
  int log(std::string&);

  // Estimates.
  int left(const std::vector<std::string>&, std::string&);

  // A slot's candidates, in CmdScheduleEdit.cpp.
  int peek(std::vector<std::string>, std::string&);
  int next(std::vector<std::string>, std::string&);
  int open(std::vector<std::string>, std::string&);
  int now(std::string&);
};

#endif
////////////////////////////////////////////////////////////////////////////////
