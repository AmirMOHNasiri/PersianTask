////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_SCHEDULEPLANNER
#define INCLUDED_SCHEDULEPLANNER

#include <CLI2.h>
#include <Datetime.h>
#include <ScheduleState.h>
#include <ScheduleTemplate.h>
#include <Task.h>

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace schedule {

// Saves the global command line state, and restores it when destroyed, even
// when an exception is thrown.  Filters are evaluated by Taskwarrior's own
// filter engine, which reads that global state.
class CliGuard {
 public:
  CliGuard();
  ~CliGuard();
  CliGuard(const CliGuard&) = delete;
  CliGuard& operator=(const CliGuard&) = delete;

  // Replaces the command line with these arguments, after the binary.  With
  // useContext false, the active context (task context) is not applied.
  static void replaceArgs(const std::vector<std::string>&, bool useContext);

 private:
  CLI2 _saved;
};

// Sets the current time used by filters and urgency (Datetime::currentTime)
// while it exists, and restores the previous one when destroyed.
class NowGuard {
 public:
  explicit NowGuard(time_t);
  ~NowGuard();
  NowGuard(const NowGuard&) = delete;
  NowGuard& operator=(const NowGuard&) = delete;

 private:
  time_t _saved;
};

// A slot on a particular day, after this week's changes.
struct Instance {
  int number{0};  // Position in the template's week, starting at 1.  Events have 0.
  std::string id;  // The slot id, or e<N> for event N.
  const Slot* slot{nullptr};
  const Event* event{nullptr};
  Datetime day;  // Midnight.
  int start{0};
  int end{0};

  bool moved{false};
  bool skipped{false};
  std::string pinnedTask;  // UUID.
  std::string pinnedFilter;
  bool borrowed{false};

  int minutes() const { return end - start; }  // After this week's changes.
};

// The slots of the week containing a day.  Weeks start on the configured
// weekstart (Saturday by default), and days are calendar days.
// The order of layers is: the template, then the active modes (later), then
// this week's changes (move, swap, skip, assign), then borrowing (later).
class Week {
 public:
  Week(const Template&, const Datetime&, const State* = nullptr);
  Week(const Week&) = delete;  // Instances point into the week, and the state.
  Week& operator=(const Week&) = delete;

  std::string label() const;
  Datetime end() const;  // Midnight after the last day.

  // The slot by id, by number, or the event by e<N>, or nullptr.
  const Instance* find(const std::string&) const;

  // The time that filters such as due.before:now+7d are relative to: the
  // current time for this week, and the start of the week for other weeks.
  Datetime reference() const;

  Datetime start;
  std::vector<Instance> instances;  // In time order.

  // Changes in the state that refer to slots no longer in the template.
  std::vector<std::string> notes;

 private:
  std::deque<Slot> _eventSlots;
};

// How a slot got its task.
enum class Source {
  fixed,     // A fixed slot, which never takes a task.
  rest,      // A rest slot, or 'rest' in the fallback chain.
  pinned,    // Assigned with 'task schedule assign'.
  borrowed,  // Lent to a task with 'task schedule borrow'.
  event,     // A one-time event.
  slot,      // The slot's own filter.
  fallback,  // A category in the fallback chain.
  none       // Nothing matched.
};

// A task's estimate, in seconds.  Planned is the estimate times the
// template's estimate.factor, and remaining is planned minus tracked time.
struct Effort {
  long estimate{-1};  // -1 for none.
  long planned{0};
  long tracked{0};
  long remaining{0};
};

struct Assignment {
  const Instance* instance{nullptr};
  std::optional<Task> task;
  Source source{Source::none};
  std::string via;  // The category, for slot and fallback.
};

// Chooses a task for each slot of a week.  First each slot takes the most
// urgent pending task of its own category, in time order, so the earliest slot
// gets the most urgent task.  Then the slots still empty go down their
// fallback chains, in time order.  Filters and urgency are evaluated as of
// the reference time, so that 'now' in a filter means that time.  Reads
// tasks, never writes anything.
class Planner {
 public:
  Planner(const Template&, const Datetime& reference);

  std::vector<Assignment> plan(const Week&);
  std::vector<Task> matching(const std::string& filter);

  // Assignments that could not be honored, such as a pinned task that is no
  // longer pending.
  std::vector<std::string> notes;

  const Effort& effort(const Task&);

  // Minutes of work for each category, using each slot's duration after this
  // week's changes.  Fixed slots, events and free slots do not count.
  std::map<std::string, int> minutesByCategory(const std::vector<Assignment>&);

  // The category of a task: the first category, in template order, whose
  // filter it matches, or an empty string.
  std::string categoryOf(const Task&);

 private:
  std::optional<Task> pick(const std::string& filter, const Instance&);
  void use(const Task&, const Instance&);
  bool available(const Task&, const Instance&);

  const Template& _template;
  Datetime _reference;
  std::vector<Task> _all;      // Pending, waiting and blocked tasks.
  std::vector<Task> _pending;  // Those that can be worked on now.
  std::map<std::string, std::vector<Task>> _cache;
  std::map<std::pair<std::string, time_t>, int> _used;  // Slots per task (uuid) per day.
  std::map<std::string, Effort> _efforts;                // By uuid.
  std::map<std::string, long> _left;                     // Remaining, during plan().
};

// The current time.  The setting rc.schedule.now exists only for the tests,
// and is deliberately not documented.
Datetime now();

}  // namespace schedule

#endif
////////////////////////////////////////////////////////////////////////////////
