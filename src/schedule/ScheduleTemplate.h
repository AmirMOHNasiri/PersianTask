////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_SCHEDULETEMPLATE
#define INCLUDED_SCHEDULETEMPLATE

#include <string>
#include <vector>

namespace schedule {

enum class Kind { fixed, study, floating, buffer, rest };

std::string kindName(Kind);

// A mistake in the template, reported by 'task schedule check'.  Line 0 means
// the problem is in the configuration, not in a line of the template.
struct Problem {
  int line;
  std::string message;
};

struct Category {
  std::string name;
  double floor{-1};  // Hours a week, or -1 for none.
  std::string filter;
  bool accounting{true};  // False with 'accounting:off': never counts hours.
  int line{0};
};

// A recurring weekly time slot.
struct Slot {
  std::string id;
  int weekday{0};  // 0 = Sunday, like tm_wday.
  int start{0};    // Minutes since midnight.
  int end{0};
  Kind kind{Kind::study};
  std::string category;  // Empty for '-'.
  std::string filter;    // Overrides the category's filter.
  std::vector<std::string> fallback;
  bool hasFallback{false};  // Overrides the global fallback chain.
  bool optional{false};
  bool isProtected{false};
  int line{0};

  // An [everyday] slot, as written, has weekday -1, and the days it skips
  // (except:jome) in except.  Each day it applies to gets a copy, among the
  // template's slots, with the day's prefix (sat-lunch) and its id here.
  std::vector<int> except;
  std::string everyday;
};

// A day on which an [everyday] slot does not apply, since a slot of the day's
// own overlaps it, and replaces it.
struct Replacement {
  std::string everyday;  // The [everyday] slot's id.
  int weekday{0};
  std::vector<std::string> by;  // The day's slots that overlap it.
};

// A [mode <name>] section.  Modes are parsed and checked here, and applied
// by the planner.
struct Mode {
  std::string name;
  std::vector<std::string> params;
  std::string active;
  std::vector<std::pair<std::string, int>> gives;   // Rule, line.
  std::vector<std::pair<std::string, int>> floors;  // Rule, line.
  int line{0};

  // Parsed by Template::validate, when valid.
  enum class Active { always, before, until };
  Active activeKind{Active::always};
  int activeDays{0};
  std::string activeParam;  // Without the $.

  // give = <selector> [N% | except N] -> <target>
  struct Give {
    std::string selector;  // Category, slot id, or kind:<kind>.
    int percent{100};
    int except{-1};
    std::string target;  // Category, or $param.
  };
  std::vector<Give> giveRules;

  // floor = <category> <hours>
  struct Floor {
    std::string category;  // Category, or $param.
    double hours{0};
  };
  std::vector<Floor> floorRules;
};

// The weekly schedule template: a hand-written text file, documented in
// docs/schedule.md.
class Template {
 public:
  static std::string path();
  static int wake();
  static int sleep();

  // Returns false if the file does not exist.  Mistakes are collected in
  // problems, not thrown.
  bool load();
  void parse(const std::string&);

  const Category* category(const std::string&) const;
  const Mode* mode(const std::string&) const;

  // The category that a task's hours count for, given which categories it
  // matches: the first one, in template order, with a floor, or else the
  // first one without, skipping those with accounting:off.  Empty if none.
  template <typename Matches>
  std::string accountFor(Matches matches) const {
    for (bool floored : {true, false})
      for (auto& c : categories)
        if (c.accounting && (c.floor >= 0) == floored && matches(c)) return c.name;
    return "";
  }
  const Slot* slot(const std::string&) const;
  const Slot* everydaySlot(const std::string&) const;
  std::string filterFor(const Slot&) const;
  const std::vector<std::string>& fallbackFor(const Slot&) const;

  double estimateFactor{1.0};
  int unestimatedPerDay{1};
  bool pacing{true};            // Plan tasks with due dates to keep pace.
  long maxPerDay{-1};           // Seconds of one task a day, or -1 for no limit.
  int overdueCriticalDays{7};   // Days an overdue task stays critical.
  std::vector<std::string> fallback;
  std::vector<std::string> borrow;

  std::vector<Category> categories;
  std::vector<Slot> slots;  // Including each day's copies of [everyday] slots.
  std::vector<Slot> everyday;  // [everyday] slots, as written.
  std::vector<Replacement> replaced;
  std::vector<Mode> modes;
  std::vector<Problem> problems;

 private:
  void parseSetting(const std::string&, int);
  void parseCategory(const std::string&, int);
  void parseSlot(const std::string&, int, int);
  void parseModeLine(Mode&, const std::string&, int);
  void expandEveryday();
  void validate();
  void problem(int, const std::string&);

  int _fallbackLine{0};
  int _borrowLine{0};
};

// Minutes since midnight of HH:MM, 'wake' or 'sleep', or -1 if invalid.
// Persian and Arabic-Indic digits are accepted.
int parseTime(const std::string&);
std::string asciiDigits(const std::string&);
std::string formatTime(int);

// Day of the week (0 = Sunday) of a Finglish or English day name, or -1.
int parseDayName(const std::string&);

// The prefix of a day's copy of an [everyday] slot: sat, sun, ... fri.
std::string dayPrefix(int weekday);

}  // namespace schedule

#endif
////////////////////////////////////////////////////////////////////////////////
