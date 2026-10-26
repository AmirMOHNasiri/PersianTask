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
#include <memory>
#include <optional>
#include <tuple>
#include <set>
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
  Kind kind{Kind::study};  // The slot's, or study for a fixed slot opened this week.
  std::string id;  // The slot id, or e<N> for event N.
  const Slot* slot{nullptr};
  const Event* event{nullptr};
  Datetime day;  // Midnight.
  int start{0};
  int end{0};

  // A mode can give the slot's time to another category.
  std::string modeCategory;
  std::string mode;

  bool moved{false};
  bool opened{false};  // A fixed slot taking tasks this week ('open').
  bool skipped{false};
  std::string pinnedTask;  // UUID.
  std::string pinnedFilter;
  bool borrowed{false};
  bool forced{false};  // Borrowed below its category's floor.

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

  // The modes active during the week, and the floors they set.
  std::vector<std::string> modes;
  std::map<std::string, double> floors;

  // A category's floor in hours this week, or -1 for none.
  double floorOf(const Template&, const std::string& category) const;

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
  slot,       // The slot's own filter.
  secondary,  // One of the slot's other categories, as in 'ds,algo'.
  critical,   // A critical task in a borrow-order slot (critical.break-floors).
  fallback,   // A category in the fallback chain.
  past,      // A slot of the current week that has ended: nothing is planned in it.
  none       // Nothing matched.
};

// A task's estimate, in seconds.  Planned is the estimate times the
// template's estimate.factor, or an exact estimate as it is, and remaining is
// planned minus tracked time.
// A task with no estimate of its own may have its category's default.
struct Effort {
  long estimate{-1};  // -1 for none.
  bool defaulted{false};  // The estimate is its category's default.
  bool exact{false};      // An exact estimate: planned as it is, without the factor.
  long planned{0};
  long tracked{0};
  long remaining{0};
};

// How a task with an estimate and a due date stands, for one week's plan.
// Times are in seconds.  Capacity is the time of the slots it may work in,
// from the reference time until it is due: its own (whose own filter matches
// it) and all those it can reach (its own, and those whose fallback chain
// matches it).  Laxity is the reachable capacity less the remaining work.
struct Pace {
  bool paced{false};     // Planned to keep pace.
  bool overdue{false};   // At the reference time.
  bool critical{false};  // Laxity at most zero, or overdue for less than the grace period.
  long remaining{0};
  long own{0};
  long reachable{0};
  long laxity{0};
  long quota{0};    // This week's share of the remaining work.
  // The time it could have with 'borrow force' too: the slots it can reach,
  // and the borrow-order slots, floors or not.
  long forced{0};
  float urgency{0};
};

// Work due by a date that the slots before it cannot hold (see overloads()).
struct Overload {
  time_t due{0};
  long work{0};
  long capacity{0};
};

// What planning needs beyond one week, shared by the planners of one run:
// the weeks ahead, and which tasks with estimates and due dates each filter
// matches, as of each week's reference time.
class Horizon {
 public:
  static const int weeks = 26;

  Horizon(const Template&, const State*);
  Horizon(const Horizon&) = delete;
  Horizon& operator=(const Horizon&) = delete;

  const Week& week(const Datetime& start);

  // Pending tasks with a due date, which are paced if they have an estimate,
  // or a category's default estimate.
  const std::vector<Task>& pool() const { return _pool; }

  // Indexes into the pool of the tasks matching the filter.
  const std::vector<int>& matches(const std::string& filter, time_t reference);

 private:
  const Template& _template;
  const State* _state;
  std::vector<Task> _pool;
  std::map<time_t, std::unique_ptr<Week>> _weeks;
  std::map<std::pair<std::string, time_t>, std::vector<int>> _matches;
  std::map<std::string, bool> _independent;  // Of the time, by filter.
};

// A task a slot could take, in the order the planner tries them.
struct Candidate {
  Task task;
  Source source{Source::slot};  // slot (its own category), secondary or fallback.
  std::string via;              // The category.
  std::string why;              // Why it would not be planned there now, if so.
};

struct Assignment {
  const Instance* instance{nullptr};
  std::optional<Task> task;
  Source source{Source::none};
  std::string via;   // The category, for slot and fallback.
  std::string mode;  // The mode that gave the slot to that category, if any.
  bool floorBroken{false};  // It took the slot below the slot category's floor.
};

// Chooses a task for each slot of a week.  First each slot takes the most
// urgent pending task of its own category, in time order, so the earliest slot
// gets the most urgent task.  Then the slots still empty go down their
// fallback chains, in time order.  Filters and urgency are evaluated as of
// the reference time, so that 'now' in a filter means that time.  Reads
// tasks, never writes anything.
class Planner {
 public:
  Planner(const Template&, const Datetime& reference, std::shared_ptr<Horizon> = nullptr);

  std::vector<Assignment> plan(const Week&);
  std::vector<Task> matching(const std::string& filter);

  // The tasks of the pool that match the filter, in the pool's order.
  std::vector<Task> evaluate(const std::string& filter, const std::vector<Task>& pool);

  // Assignments that could not be honored, such as a pinned task that is no
  // longer pending.
  std::vector<std::string> notes;

  const Effort& effort(const Task&);

  // Minutes of work for each category, using each slot's duration after this
  // week's changes.  Fixed slots, events and free slots do not count.
  std::map<std::string, int> minutesByCategory(const std::vector<Assignment>&);

  // The category a task's hours count for (see Template::accountFor).
  std::string categoryOf(const Task&);

  // The category an assignment's hours count for: the slot's own category
  // for its own work if it has a floor, and otherwise the task's.
  std::string accountingCategory(const Assignment&);

  const Datetime& reference() const { return _reference; }

  // Whether the task may work in the slot: the slot starts before the task
  // is due, or the task is overdue already, at the reference time.
  bool eligible(const Task&, const Instance&) const;
  bool overdue(const Task&) const;

  // Planned time of earlier weeks, by uuid: for a later week, planned as if
  // the work planned in the weeks before it were done.  carried() is what
  // the next week starts from, after plan().
  void carry(const std::map<std::string, long>& planned) { _carried = planned; }
  std::map<std::string, long> carried() const;

  // After plan(): how a task stands, or nullptr if it is not paced.
  const Pace* pace(const Task&) const;
  // Time planned for the task this week, after the reference time.
  long planned(const Task&) const;
  // Time planned for it in the weeks before this one, for a later week.
  long plannedBefore(const Task&) const;

  // After plan(): the tasks the slot could take, in the order the planner
  // tries them: its own category, its secondary ones, its fallback chain;
  // tasks behind pace first in each.  A fixed or rest slot is taken as a
  // study slot of its category, if any.
  std::vector<Candidate> candidates(const Instance&);

  // After plan(): due dates by which the work due exceeds the time of the
  // slots that can hold it, counting each slot once, earliest first.
  std::vector<Overload> overloads;

 private:
  // The ordering repair, after the passes (see plan()).
  void repair(std::vector<Assignment>&, const Week&);
  bool rebuild(std::vector<Assignment>&, bool release);
  bool refill(Assignment&);
  std::optional<std::pair<Source, std::string>> reach(const Task&, const Instance&);
  std::tuple<long, long, int> rank(const Task&);

  // Dependencies, resolved as planned: when the task's blockers are fully
  // planned by the plan so far, or never (-1).
  time_t finishes(const std::string& uuid);
  long portion(const Task&, const Instance&) const;  // Time it counts in the slot.
  bool blockersDone(const Task&, const Instance&);

  long left(const Task&);
  long defaultEstimate(const Task&);
  std::string unavailable(const Task&, const Instance&);
  void computePaces(const Week&);
  bool critical(const std::string& uuid) const;
  bool contains(const std::string& filter, const std::string& uuid);
  std::string homeFilter(const Instance&) const;
  bool lends(const Instance&) const;  // Listed in the borrow setting, and not protected.
  bool breaksFloors(const Task&) const;

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
  std::map<std::string, long> _carried;                  // Planned in earlier weeks.
  std::map<std::string, long> _consumed;                 // Planned by plan().
  std::map<std::pair<std::string, time_t>, long> _usedTime;  // Seconds per task per day.
  std::map<std::string, Pace> _paces;                        // By uuid.
  std::vector<Task> _paced;  // After plan(): paced tasks, least laxity first.
  std::set<std::string> _blocked;  // Pending tasks blocked by others, by uuid.
  std::map<std::string, std::tuple<long, long, int>> _ranks;
  std::vector<Assignment> _assignments;           // After plan().
  const std::vector<Assignment>* _current{nullptr};  // The plan so far.
  std::map<std::string, std::set<std::string>> _matchSets;   // Of matching(), by filter.
  std::shared_ptr<Horizon> _horizon;
};

// A week and its plan.  A week after the current one is planned after the
// weeks between them, in order, as if their planned work were done, so that
// work planned this week is not planned again next week.
class WeekPlan {
 public:
  WeekPlan(const Template&, const Datetime& day, const State*);
  WeekPlan(const WeekPlan&) = delete;
  WeekPlan& operator=(const WeekPlan&) = delete;

  std::shared_ptr<Horizon> horizon;  // Shared by the weeks planned.
  Week week;
  Planner planner;
  std::vector<Assignment> assignments;
};

// The tasks of the pool that match the filter, as of the reference time, in
// the pool's order, whatever the active context.
std::vector<Task> filterTasks(const std::string& filter, const std::vector<Task>& pool,
                              time_t reference);

// The current time.  The setting rc.schedule.now exists only for the tests,
// and is deliberately not documented.
Datetime now();

// Midnight at the start of the week containing the day.  Weeks start on the
// weekstart setting, whatever the calendar, unlike Datetime::startOfWeek,
// which keeps upstream's Sunday weeks with calendar=gregorian.
Datetime weekStart(const Datetime&);

}  // namespace schedule

#endif
////////////////////////////////////////////////////////////////////////////////
