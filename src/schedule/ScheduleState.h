////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_SCHEDULESTATE
#define INCLUDED_SCHEDULESTATE

#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace schedule {

// A change to one slot in one week.
struct Override {
  // Move or swap: the slot's new place in the week.
  bool moved{false};
  time_t day{0};  // Midnight of the new day.
  int start{0};
  int end{0};
  std::string swappedWith;  // The other slot of a swap, if any.

  // Skip and assign replace each other.
  bool skipped{false};
  std::string task;    // Pinned task, by UUID.
  std::string filter;  // Pinned filter.
  bool borrowed{false};  // The pinned task borrowed this slot.

  // 'next': the tasks passed over, by UUID, so that it goes on down the list.
  std::vector<std::string> passed;

  // 'open': a fixed slot taking tasks this week, from this time (minutes).
  int opened{-1};

  bool empty() const;
};

// A one-time appointment.
struct Event {
  int id{0};
  std::string title;
  time_t day{0};  // Midnight.
  int start{0};
  int end{0};
};

// A mode started with 'task schedule mode start'.
struct ActiveMode {
  int id{0};
  std::string name;
  std::vector<std::string> values;  // In the order of the mode's params.
  time_t from{0};
  time_t to{0};  // 0 until stopped.

  bool activeAt(time_t when) const { return from <= when && (!to || when < to); }
  bool overlaps(time_t start, time_t end) const { return from < end && (!to || start < to); }
};

// A change, as listed by 'task schedule log'.
struct LogEntry {
  time_t when{0};
  std::string description;
  std::string before;  // The whole state before the change, as JSON.
};

// The per-week changes and the events, stored in schedule.state.json in the
// data directory, next to schedule.undo.json, which keeps the previous states
// for 'task schedule undo'.  These files are separate from the task database,
// so that other Taskwarrior clients never see them.
class State {
 public:
  // The week is identified by the midnight that starts it.
  std::map<time_t, std::map<std::string, Override>> weeks;
  std::vector<Event> events;
  std::vector<ActiveMode> modes;  // In the order they were started.

  static std::string statePath();
  static std::string undoPath();

  // Reading needs no lock: files are replaced atomically.
  void load();

  std::string toJSON() const;
  void fromJSON(const std::string&);

  Override& at(time_t week, const std::string& slot);
  const Override* find(time_t week, const std::string& slot) const;
  void prune(time_t);
};

// Holds an exclusive lock on the schedule's files while it exists, so that
// concurrent task commands cannot lose each other's changes.
class StateLock {
 public:
  StateLock();
  ~StateLock();
  StateLock(const StateLock&) = delete;
  StateLock& operator=(const StateLock&) = delete;

 private:
  int _fd{-1};
};

// Records a change: saves the previous state in the undo log, then the new
// state.  Call while holding a StateLock.
void commit(const State& before, const State& after, const std::string& description);

// Restores the state before the last change, and returns its description, or
// an empty string if there is nothing to undo.  Takes the lock itself.
std::string undo();

std::vector<LogEntry> readLog();

// Writes a file atomically: to a temporary file, synced, then renamed.
void writeFile(const std::string&, const std::string&);

}  // namespace schedule

#endif
////////////////////////////////////////////////////////////////////////////////
