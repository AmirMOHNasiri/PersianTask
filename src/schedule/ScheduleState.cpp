////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#include <cmake.h>
// cmake.h include header must come first

#include <Context.h>
#include <Datetime.h>
#include <FS.h>
#include <JSON.h>
#include <ScheduleState.h>
#include <ScheduleTemplate.h>
#include <fcntl.h>
#include <format.h>
#include <sys/file.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <sstream>

namespace schedule {

namespace {

const size_t maximumLogEntries = 100;

std::string dataPath(const std::string& name) {
  return (std::string)Context::getContext().data_dir + "/" + name;
}

// Days are stored in the Gregorian ISO format, like other machine formats.
std::string isoDate(time_t day) {
  char buffer[16];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", localtime(&day));
  return buffer;
}

time_t parseIsoDate(const std::string& input) {
  struct tm t {};
  char extra;
  if (sscanf(input.c_str(), "%d-%d-%d%c", &t.tm_year, &t.tm_mon, &t.tm_mday, &extra) != 3)
    throw format("The schedule state has an invalid date, '{1}'.", input);
  t.tm_year -= 1900;
  t.tm_mon -= 1;
  t.tm_isdst = -1;
  return mktime(&t);
}

std::string quote(const std::string& text) { return "\"" + json::encode(text) + "\""; }

json::value* member(json::value* value, const std::string& key) {
  if (!value || value->type() != json::j_object) return nullptr;
  auto& data = static_cast<json::object*>(value)->_data;
  auto found = data.find(key);
  return found == data.end() ? nullptr : found->second;
}

std::string text(json::value* value, const std::string& key) {
  auto found = member(value, key);
  if (!found) return "";
  if (found->type() != json::j_string)
    throw format("The schedule state has an invalid '{1}'.", key);
  return json::decode(static_cast<json::string*>(found)->_data);
}

double number(json::value* value, const std::string& key) {
  auto found = member(value, key);
  if (!found || found->type() != json::j_number)
    throw format("The schedule state is missing '{1}'.", key);
  return *static_cast<json::number*>(found);
}

bool flag(json::value* value, const std::string& key) {
  auto found = member(value, key);
  return found && found->type() == json::j_literal &&
         static_cast<json::literal*>(found)->_lvalue == json::literal::truevalue;
}

int minutes(json::value* value, const std::string& key) {
  auto minutes = parseTime(text(value, key));
  if (minutes == -1) throw format("The schedule state has an invalid time in '{1}'.", key);
  return minutes;
}

std::unique_ptr<json::value> parse(const std::string& contents, const std::string& path) {
  std::unique_ptr<json::value> root;
  try {
    root.reset(json::parse(contents));
  } catch (const std::string& error) {
    throw format("{1} is damaged ({2}).  Fix it, or delete it to start over.", path, error);
  }
  if (!root || root->type() != json::j_object)
    throw format("{1} is damaged.  Fix it, or delete it to start over.", path);
  return root;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
bool Override::empty() const { return !moved && !skipped && task.empty() && filter.empty(); }


////////////////////////////////////////////////////////////////////////////////
std::string State::statePath() { return dataPath("schedule.state.json"); }

////////////////////////////////////////////////////////////////////////////////
std::string State::undoPath() { return dataPath("schedule.undo.json"); }

////////////////////////////////////////////////////////////////////////////////
void State::load() {
  weeks.clear();
  events.clear();
  if (!File(statePath()).exists()) return;

  std::string contents;
  if (!File::read(statePath(), contents)) throw format("Could not read {1}.", statePath());
  fromJSON(contents);
}

////////////////////////////////////////////////////////////////////////////////
std::string State::toJSON() const {
  std::stringstream out;
  out << "{\n  \"version\": 1,\n  \"weeks\": {";

  bool firstWeek = true;
  for (auto& week : weeks) {
    bool any = false;
    for (auto& slot : week.second) any = any || !slot.second.empty();
    if (!any) continue;

    out << (firstWeek ? "\n" : ",\n") << "    " << quote(isoDate(week.first)) << ": {";
    firstWeek = false;

    bool firstSlot = true;
    for (auto& slot : week.second) {
      auto& o = slot.second;
      if (o.empty()) continue;

      std::vector<std::string> fields;
      if (o.moved)
        fields.push_back(format("\"move\": {\"day\": {1}, \"start\": {2}, \"end\": {3}}",
                                quote(isoDate(o.day)), quote(formatTime(o.start)),
                                quote(formatTime(o.end))));
      if (!o.swappedWith.empty()) fields.push_back("\"swap\": " + quote(o.swappedWith));
      if (o.skipped) fields.push_back("\"skip\": true");
      if (!o.task.empty()) fields.push_back("\"task\": " + quote(o.task));
      if (o.borrowed) fields.push_back("\"borrowed\": true");
      if (!o.filter.empty()) fields.push_back("\"filter\": " + quote(o.filter));

      std::string joined;
      for (auto& field : fields) joined += (joined.empty() ? "" : ", ") + field;
      out << (firstSlot ? "\n" : ",\n") << "      " << quote(slot.first) << ": {" << joined << "}";
      firstSlot = false;
    }
    out << "\n    }";
  }
  out << (firstWeek ? "" : "\n  ") << "},\n  \"events\": [";

  for (size_t i = 0; i < events.size(); ++i) {
    auto& e = events[i];
    out << (i ? ",\n" : "\n")
        << format("    {\"id\": {1}, \"title\": {2}, \"date\": {3}, \"start\": {4}, \"end\": {5}}",
                  e.id, quote(e.title), quote(isoDate(e.day)), quote(formatTime(e.start)),
                  quote(formatTime(e.end)));
  }
  out << (events.empty() ? "" : "\n  ") << "]\n}\n";
  return out.str();
}

////////////////////////////////////////////////////////////////////////////////
void State::fromJSON(const std::string& contents) {
  weeks.clear();
  events.clear();

  auto root = parse(contents, statePath());

  auto weekList = member(root.get(), "weeks");
  if (weekList && weekList->type() == json::j_object) {
    for (auto& week : static_cast<json::object*>(weekList)->_data) {
      auto start = parseIsoDate(week.first);
      if (week.second->type() != json::j_object) continue;

      for (auto& slot : static_cast<json::object*>(week.second)->_data) {
        Override o;
        if (auto move = member(slot.second, "move")) {
          o.moved = true;
          o.day = parseIsoDate(text(move, "day"));
          o.start = minutes(move, "start");
          o.end = minutes(move, "end");
        }
        o.swappedWith = text(slot.second, "swap");
        o.skipped = flag(slot.second, "skip");
        o.task = text(slot.second, "task");
        o.borrowed = flag(slot.second, "borrowed");
        o.filter = text(slot.second, "filter");
        weeks[start][slot.first] = o;
      }
    }
  }

  auto eventList = member(root.get(), "events");
  if (eventList && eventList->type() == json::j_array) {
    for (auto value : static_cast<json::array*>(eventList)->_data) {
      Event e;
      e.id = static_cast<int>(number(value, "id"));
      e.title = text(value, "title");
      e.day = parseIsoDate(text(value, "date"));
      e.start = minutes(value, "start");
      e.end = minutes(value, "end");
      events.push_back(e);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
Override& State::at(time_t week, const std::string& slot) { return weeks[week][slot]; }

////////////////////////////////////////////////////////////////////////////////
const Override* State::find(time_t week, const std::string& slot) const {
  auto w = weeks.find(week);
  if (w == weeks.end()) return nullptr;
  auto s = w->second.find(slot);
  return s == w->second.end() || s->second.empty() ? nullptr : &s->second;
}

////////////////////////////////////////////////////////////////////////////////
// Forgets weeks that started before the given time, and their events.
void State::prune(time_t before) {
  for (auto i = weeks.begin(); i != weeks.end();)
    i = i->first < before ? weeks.erase(i) : std::next(i);

  for (auto i = events.begin(); i != events.end();) i = i->day < before ? events.erase(i) : i + 1;
}

////////////////////////////////////////////////////////////////////////////////
StateLock::StateLock() {
  auto path = dataPath("schedule.lock");
  _fd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
  if (_fd == -1) throw format("Could not open {1}: {2}", path, strerror(errno));

  while (flock(_fd, LOCK_EX) == -1)
    if (errno != EINTR) {
      close(_fd);
      throw format("Could not lock {1}: {2}", path, strerror(errno));
    }
}

////////////////////////////////////////////////////////////////////////////////
StateLock::~StateLock() {
  flock(_fd, LOCK_UN);
  close(_fd);
}

////////////////////////////////////////////////////////////////////////////////
void writeFile(const std::string& path, const std::string& contents) {
  auto temporary = format("{1}.{2}.tmp", path, getpid());

  int fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd == -1) throw format("Could not write {1}: {2}", temporary, strerror(errno));

  const char* data = contents.data();
  size_t remaining = contents.size();
  while (remaining) {
    auto written = write(fd, data, remaining);
    if (written == -1) {
      if (errno == EINTR) continue;
      auto error = strerror(errno);
      close(fd);
      unlink(temporary.c_str());
      throw format("Could not write {1}: {2}", temporary, error);
    }
    data += written;
    remaining -= written;
  }

  if (fsync(fd) == -1 || close(fd) == -1) {
    auto error = strerror(errno);
    unlink(temporary.c_str());
    throw format("Could not write {1}: {2}", temporary, error);
  }

  if (rename(temporary.c_str(), path.c_str()) == -1) {
    auto error = strerror(errno);
    unlink(temporary.c_str());
    throw format("Could not replace {1}: {2}", path, error);
  }

  // Sync the directory too, so that the rename itself is on disk.
  auto slash = path.rfind('/');
  int directory = open(slash == std::string::npos ? "." : path.substr(0, slash).c_str(),
                       O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (directory != -1) {
    fsync(directory);
    close(directory);
  }
}

////////////////////////////////////////////////////////////////////////////////
std::vector<LogEntry> readLog() {
  std::vector<LogEntry> entries;
  auto path = State::undoPath();
  if (!File(path).exists()) return entries;

  std::string contents;
  if (!File::read(path, contents)) throw format("Could not read {1}.", path);

  auto root = parse(contents, path);
  auto changes = member(root.get(), "changes");
  if (!changes || changes->type() != json::j_array) return entries;

  for (auto value : static_cast<json::array*>(changes)->_data) {
    LogEntry entry;
    entry.when = static_cast<time_t>(number(value, "time"));
    entry.description = text(value, "description");
    auto before = member(value, "before");
    if (!before) throw format("{1} is damaged.  Fix it, or delete it to start over.", path);
    entry.before = before->dump();
    entries.push_back(entry);
  }
  return entries;
}

////////////////////////////////////////////////////////////////////////////////
namespace {

void writeLog(const std::vector<LogEntry>& entries) {
  std::stringstream out;
  out << "{\n  \"version\": 1,\n  \"changes\": [";
  for (size_t i = 0; i < entries.size(); ++i)
    out << (i ? ",\n" : "\n") << "    {\"time\": " << entries[i].when
        << ", \"description\": " << quote(entries[i].description)
        << ", \"before\": " << entries[i].before << "}";
  out << (entries.empty() ? "" : "\n  ") << "]\n}\n";
  writeFile(State::undoPath(), out.str());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
void commit(const State& before, const State& after, const std::string& description) {
  auto entries = readLog();
  entries.push_back({Datetime::currentTime(), description, before.toJSON()});
  if (entries.size() > maximumLogEntries)
    entries.erase(entries.begin(), entries.end() - maximumLogEntries);

  // The log first: if the state is not written, undoing restores the state
  // it already has.
  writeLog(entries);
  writeFile(State::statePath(), after.toJSON());
}

////////////////////////////////////////////////////////////////////////////////
std::string undo() {
  StateLock lock;

  auto entries = readLog();
  if (entries.empty()) return "";

  auto last = entries.back();
  entries.pop_back();

  // Checked before anything is written.
  State restored;
  restored.fromJSON(last.before);

  writeFile(State::statePath(), restored.toJSON());
  writeLog(entries);
  return last.description;
}

}  // namespace schedule

////////////////////////////////////////////////////////////////////////////////
