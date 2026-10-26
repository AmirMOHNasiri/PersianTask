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
#include <ScheduleEffort.h>
#include <ScheduleTemplate.h>
#include <format.h>
#include <shared.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>

namespace schedule {

namespace {

const std::vector<std::pair<std::string, Kind>> kinds{{"fixed", Kind::fixed},
                                                      {"study", Kind::study},
                                                      {"floating", Kind::floating},
                                                      {"buffer", Kind::buffer},
                                                      {"rest", Kind::rest}};

const std::vector<std::string> settingNames{"estimate.factor", "unestimated.per-day", "fallback",
                                            "borrow",          "pacing",              "max.per-day",
                                            "overdue.critical-days"};

std::string lower(const std::string& input) {
  std::string output = input;
  std::transform(output.begin(), output.end(), output.begin(), ::tolower);
  return output;
}

std::string trim(const std::string& input) {
  auto first = input.find_first_not_of(" \t\r");
  if (first == std::string::npos) return "";
  auto last = input.find_last_not_of(" \t\r");
  return input.substr(first, last - first + 1);
}

// The line without its comment, which starts at a # outside double quotes.
std::string stripComment(const std::string& line) {
  bool quoted = false;
  for (size_t i = 0; i < line.length(); ++i) {
    if (line[i] == '"')
      quoted = !quoted;
    else if (line[i] == '#' && !quoted)
      return trim(line.substr(0, i));
  }
  return trim(line);
}

// Splits on whitespace, keeping double-quoted text together, without quotes.
std::vector<std::string> tokenize(const std::string& line) {
  std::vector<std::string> tokens;
  std::string token;
  bool quoted = false;
  bool any = false;
  for (auto c : line) {
    if (c == '"') {
      quoted = !quoted;
      any = true;
    } else if ((c == ' ' || c == '\t') && !quoted) {
      if (any) tokens.push_back(token);
      token.clear();
      any = false;
    } else {
      token += c;
      any = true;
    }
  }
  if (any) tokens.push_back(token);
  return tokens;
}

bool isName(const std::string& name) {
  if (name.empty() || !isalnum(static_cast<unsigned char>(name[0]))) return false;
  for (auto c : name)
    if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') return false;
  return true;
}

bool isNumber(const std::string& input, double& value) {
  char extra;
  return sscanf(input.c_str(), "%lf%c", &value, &extra) == 1;
}

size_t distance(const std::string& a, const std::string& b) {
  std::vector<size_t> row(b.length() + 1);
  for (size_t j = 0; j <= b.length(); ++j) row[j] = j;
  for (size_t i = 1; i <= a.length(); ++i) {
    size_t diagonal = row[0];
    row[0] = i;
    for (size_t j = 1; j <= b.length(); ++j) {
      size_t above = row[j];
      row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] != b[j - 1])});
      diagonal = above;
    }
  }
  return row[b.length()];
}

// " (did you mean 'x'?)" if a candidate is close to the name.
std::string suggest(const std::string& name, const std::vector<std::string>& candidates) {
  std::string best;
  size_t bestDistance = 3;
  for (auto& candidate : candidates) {
    auto d = distance(lower(name), lower(candidate));
    if (d < bestDistance) {
      best = candidate;
      bestDistance = d;
    }
  }
  return best.empty() ? "" : " (did you mean '" + best + "'?)";
}

int configTime(const std::string& name, int fallback) {
  auto value = Context::getContext().config.get(name);
  int minutes;
  char extra;
  int hours;
  if (sscanf(value.c_str(), "%d:%d%c", &hours, &minutes, &extra) != 2 || hours < 0 ||
      hours > 24 || minutes < 0 || minutes > 59 || (hours == 24 && minutes))
    return fallback;
  return hours * 60 + minutes;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
std::string kindName(Kind kind) {
  for (auto& k : kinds)
    if (k.second == kind) return k.first;
  return "";
}

////////////////////////////////////////////////////////////////////////////////
std::string asciiDigits(const std::string& input) {
  std::string output;
  for (size_t i = 0; i < input.length(); ++i) {
    auto lead = static_cast<unsigned char>(input[i]);
    auto trail = i + 1 < input.length() ? static_cast<unsigned char>(input[i + 1]) : 0;
    if (lead == 0xDB && trail >= 0xB0 && trail <= 0xB9) {  // Persian
      output += static_cast<char>('0' + trail - 0xB0);
      ++i;
    } else if (lead == 0xD9 && trail >= 0xA0 && trail <= 0xA9) {  // Arabic-Indic
      output += static_cast<char>('0' + trail - 0xA0);
      ++i;
    } else
      output += input[i];
  }
  return output;
}

////////////////////////////////////////////////////////////////////////////////
int parseTime(const std::string& raw) {
  auto input = asciiDigits(raw);
  if (input == "wake") return Template::wake();
  if (input == "sleep") return Template::sleep();

  int hours, minutes;
  char extra;
  if (sscanf(input.c_str(), "%d:%d%c", &hours, &minutes, &extra) != 2) return -1;
  if (input.find(':') > 2 || input.length() - input.find(':') != 3) return -1;
  if (hours < 0 || minutes < 0 || minutes > 59 || hours > 24 || (hours == 24 && minutes))
    return -1;

  return hours * 60 + minutes;
}

////////////////////////////////////////////////////////////////////////////////
std::string formatTime(int minutes) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes / 60, minutes % 60);
  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
int parseDayName(const std::string& input) {
  static const std::map<std::string, int> days{
      {"shanbe", 6},       {"shanbeh", 6},       {"yekshanbe", 0},    {"yekshanbeh", 0},
      {"doshanbe", 1},     {"doshanbeh", 1},     {"seshanbe", 2},     {"seshanbeh", 2},
      {"sehshanbe", 2},    {"chaharshanbe", 3},  {"chaharshanbeh", 3}, {"charshanbe", 3},
      {"panjshanbe", 4},   {"panjshanbeh", 4},   {"jome", 5},         {"jomeh", 5},
      {"saturday", 6},     {"sunday", 0},        {"monday", 1},       {"tuesday", 2},
      {"wednesday", 3},    {"thursday", 4},      {"friday", 5}};

  auto day = days.find(lower(input));
  return day == days.end() ? -1 : day->second;
}

////////////////////////////////////////////////////////////////////////////////
std::string dayPrefix(int weekday) {
  static const char* prefixes[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  return weekday >= 0 && weekday < 7 ? prefixes[weekday] : "";
}

////////////////////////////////////////////////////////////////////////////////
std::string Template::path() {
  auto configured = Context::getContext().config.get("schedule.template");
  if (configured.empty()) return (std::string)Context::getContext().data_dir + "/schedule";
  return Path(configured)._data;
}

////////////////////////////////////////////////////////////////////////////////
int Template::wake() { return configTime("schedule.wake", 6 * 60); }

////////////////////////////////////////////////////////////////////////////////
int Template::sleep() { return configTime("schedule.sleep", 24 * 60); }

////////////////////////////////////////////////////////////////////////////////
bool Template::load() {
  File file(path());
  if (!file.exists()) return false;

  std::string contents;
  if (!File::read(path(), contents)) throw format("Could not read {1}.", path());

  parse(contents);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
void Template::parse(const std::string& contents) {
  std::string section;
  Mode* mode = nullptr;
  int weekday = -1;  // Of a day section.

  std::istringstream lines(contents);
  std::string raw;
  for (int number = 1; std::getline(lines, raw); ++number) {
    auto line = stripComment(raw);
    if (line.empty()) continue;

    if (line[0] == '[') {
      if (line.back() != ']') {
        problem(number, "A section header must end with ']'.");
        section = "";
        continue;
      }

      section = lower(trim(line.substr(1, line.length() - 2)));
      mode = nullptr;
      weekday = parseDayName(section);

      if (section.substr(0, 5) == "mode ") {
        auto name = trim(section.substr(5));
        if (!isName(name)) {
          problem(number, format("'{1}' is not a valid mode name.", name));
          section = "";
          continue;
        }
        for (auto& existing : modes)
          if (existing.name == name)
            problem(number, format("Mode '{1}' is already defined on line {2}.", name,
                                   existing.line));
        Mode defined;
        defined.name = name;
        defined.line = number;
        modes.push_back(defined);
        mode = &modes.back();
        section = "mode";
      } else if (section != "settings" && section != "categories" && section != "everyday" &&
                 weekday == -1) {
        problem(number,
                format("Unknown section '[{1}]'.  Sections are [settings], [categories], day "
                       "names such as [shanbe], [everyday], and [mode <name>].{2}",
                       section,
                       suggest(section, {"settings", "categories", "everyday", "shanbe",
                                         "yekshanbe", "doshanbe", "seshanbe", "chaharshanbe",
                                         "panjshanbe", "jome"})));
        section = "";
      }
      continue;
    }

    if (section == "settings")
      parseSetting(line, number);
    else if (section == "categories")
      parseCategory(line, number);
    else if (section == "mode")
      parseModeLine(*mode, line, number);
    else if (weekday != -1 || section == "everyday")
      parseSlot(line, number, weekday);
    else if (section.empty())
      problem(number, "This line is not in a section, such as [settings] or [shanbe].");
  }

  validate();
}

////////////////////////////////////////////////////////////////////////////////
void Template::parseSetting(const std::string& line, int number) {
  auto equals = line.find('=');
  if (equals == std::string::npos) {
    problem(number, "A setting must look like 'name = value'.");
    return;
  }

  auto name = trim(line.substr(0, equals));
  auto value = trim(line.substr(equals + 1));
  double number_value;

  if (name == "estimate.factor") {
    if (!isNumber(value, number_value) || number_value <= 0)
      problem(number, "estimate.factor must be a positive number, such as 1.5.");
    else
      estimateFactor = number_value;
  } else if (name == "unestimated.per-day") {
    if (!isNumber(value, number_value) || number_value < 1 ||
        number_value != static_cast<int>(number_value))
      problem(number, "unestimated.per-day must be a whole number of at least 1.");
    else
      unestimatedPerDay = static_cast<int>(number_value);
  } else if (name == "pacing") {
    if (value != "on" && value != "off")
      problem(number, "pacing must be on or off.");
    else
      pacing = value == "on";
  } else if (name == "max.per-day") {
    maxPerDay = parseHours(value);
    if (maxPerDay <= 0)
      problem(number, "max.per-day must be a number of hours, such as 3 or 2.5h.");
  } else if (name == "overdue.critical-days") {
    if (!isNumber(value, number_value) || number_value < 0 ||
        number_value != static_cast<int>(number_value))
      problem(number, "overdue.critical-days must be a whole number of days, such as 7.");
    else
      overdueCriticalDays = static_cast<int>(number_value);
  } else if (name == "fallback") {
    fallback = tokenize(value);
    for (auto& step : fallback) step = lower(step);
    _fallbackLine = number;
  } else if (name == "borrow") {
    borrow = tokenize(value);
    _borrowLine = number;
  } else
    problem(number, format("Unknown setting '{1}'.{2}", name, suggest(name, settingNames)));
}

////////////////////////////////////////////////////////////////////////////////
// <name> <floor> <filter...>
void Template::parseCategory(const std::string& line, int number) {
  auto tokens = tokenize(line);
  if (tokens.size() < 3) {
    problem(number, "A category must look like '<name> <floor> <filter>', such as "
                    "'os 4 project:os'.");
    return;
  }

  Category category;
  category.name = lower(tokens[0]);
  category.line = number;

  if (!isName(category.name) || category.name == "rest")
    problem(number, format("'{1}' is not a valid category name.", tokens[0]));

  if (tokens[1] != "-" && (!isNumber(tokens[1], category.floor) || category.floor < 0))
    problem(number, format("The floor '{1}' must be a number of hours, or '-' for none.",
                           tokens[1]));

  // The filter is the rest of the line, as written.
  auto rest = trim(line.substr(line.find(tokens[0]) + tokens[0].length()));
  rest = trim(rest.substr(rest.find(tokens[1]) + tokens[1].length()));
  // Options at the end, after the filter: accounting:off keeps the category
  // out of the hours, and estimate:4h is the estimate of its tasks with none.
  for (;;) {
    auto space = rest.find_last_of(" \t");
    if (space == std::string::npos) break;
    auto option = rest.substr(space + 1);
    if (option == "accounting:off" || option == "accounting:on")
      category.accounting = option == "accounting:on";
    else if (option.substr(0, 9) == "estimate:") {
      category.estimate = parseHours(option.substr(9));
      if (category.estimate <= 0)
        problem(number, format("'{1}' is not an estimate, such as estimate:4h.", option));
    } else
      break;
    rest = trim(rest.substr(0, space));
  }

  if (rest.length() >= 2 && rest.front() == '"' && rest.back() == '"')
    rest = rest.substr(1, rest.length() - 2);
  category.filter = rest;

  for (auto& existing : categories)
    if (existing.name == category.name)
      problem(number, format("Category '{1}' is already defined on line {2}.", category.name,
                             existing.line));

  categories.push_back(category);
}

////////////////////////////////////////////////////////////////////////////////
// <start>-<end> <id> <kind> <category> [options]
void Template::parseSlot(const std::string& line, int number, int weekday) {
  auto tokens = tokenize(line);
  if (tokens.size() < 4) {
    problem(number, "A slot must look like '<start>-<end> <id> <kind> <category>', such as "
                    "'08:00-10:00 sat-os study os'.");
    return;
  }

  Slot slot;
  slot.weekday = weekday;
  slot.line = number;

  auto dash = tokens[0].find('-');
  slot.start = dash == std::string::npos ? -1 : parseTime(tokens[0].substr(0, dash));
  slot.end = dash == std::string::npos ? -1 : parseTime(tokens[0].substr(dash + 1));
  if (slot.start == -1 || slot.end == -1) {
    problem(number, format("'{1}' is not a time range, such as 08:00-10:00 or 12:00-sleep.",
                           tokens[0]));
    return;
  }
  if (slot.end <= slot.start)
    problem(number, format("The time range '{1}' ends before it starts.", tokens[0]));
  else if (slot.start < wake() || slot.end > sleep())
    problem(number, format("'{1}' is outside the day, which is {2}-{3} (schedule.wake and "
                           "schedule.sleep).",
                           tokens[0], formatTime(wake()), formatTime(sleep())));

  slot.id = tokens[1];
  bool digits = std::all_of(slot.id.begin(), slot.id.end(), ::isdigit);
  if (!isName(slot.id) || digits)
    problem(number, format("'{1}' is not a valid slot id.  Use letters, digits, '-' and '_', "
                           "such as 'sat-os'.",
                           slot.id));
  for (auto list : {&slots, &everyday})
    for (auto& existing : *list)
      if (existing.id == slot.id)
        problem(number, format("Slot '{1}' is already defined on line {2}.", slot.id,
                               existing.line));

  auto kind = std::find_if(kinds.begin(), kinds.end(),
                           [&](auto& k) { return k.first == lower(tokens[2]); });
  if (kind == kinds.end()) {
    std::vector<std::string> names;
    for (auto& k : kinds) names.push_back(k.first);
    problem(number, format("Unknown kind '{1}'.  Kinds are fixed, study, floating, buffer and "
                           "rest.{2}",
                           tokens[2], suggest(tokens[2], names)));
  } else
    slot.kind = kind->second;

  // The category, or several in order of priority: 'ds,algo'.
  if (tokens[3] != "-") {
    std::istringstream names(lower(tokens[3]));
    std::string name;
    while (std::getline(names, name, ','))
      if (slot.category.empty())
        slot.category = name;
      else
        slot.secondary.push_back(name);
    if (slot.category.empty() || std::find(slot.secondary.begin(), slot.secondary.end(), "") !=
                                     slot.secondary.end())
      problem(number, format("'{1}' is not a list of categories, such as ds or ds,algo.",
                             tokens[3]));
  }

  for (size_t i = 4; i < tokens.size(); ++i) {
    auto& option = tokens[i];
    if (option == "optional")
      slot.optional = true;
    else if (option == "protected")
      slot.isProtected = true;
    else if (option.substr(0, 7) == "filter:")
      slot.filter = option.substr(7);
    else if (option.substr(0, 9) == "fallback:") {
      slot.fallback = tokenize(option.substr(9));
      for (auto& name : slot.fallback) name = lower(name);
      slot.hasFallback = true;
    } else if (option.substr(0, 7) == "except:") {
      // except:jome or except:panjshanbe,jome
      if (weekday != -1)
        problem(number, "except: only applies to [everyday] slots.");
      std::istringstream names(option.substr(7));
      std::string name;
      while (std::getline(names, name, ',')) {
        auto day = parseDayName(name);
        if (day == -1)
          problem(number, format("In '{1}', '{2}' is not a day name, such as jome or friday.",
                                 option, name));
        else
          slot.except.push_back(day);
      }
    } else
      problem(number, format("Unknown slot option '{1}'.  Options are filter:\"...\", "
                             "fallback:\"...\", optional, protected, and except:<days> in "
                             "[everyday].{2}",
                             option,
                             suggest(option, {"optional", "protected", "filter:", "fallback:",
                                              "except:"})));
  }

  if ((slot.kind == Kind::fixed || slot.kind == Kind::rest) &&
      (!slot.filter.empty() || slot.hasFallback))
    problem(number, format("A {1} slot never takes tasks, so it cannot have a filter or a "
                           "fallback.",
                           kindName(slot.kind)));

  if ((slot.kind == Kind::study || slot.kind == Kind::floating) && slot.category.empty() &&
      slot.filter.empty())
    problem(number, format("A {1} slot needs a category or a filter.", kindName(slot.kind)));

  if (weekday == -1)
    everyday.push_back(slot);
  else
    slots.push_back(slot);
}

////////////////////////////////////////////////////////////////////////////////
// Gives each day a copy of every [everyday] slot, except on the days it
// skips, and on the days where a slot of the day's own overlaps it, which
// replaces it there.  Copies are named after their day: lunch on Shanbe is
// sat-lunch.
void Template::expandEveryday() {
  for (size_t i = 0; i < everyday.size(); ++i) {
    auto& base = everyday[i];

    // Two [everyday] slots must not overlap on a day they share.
    for (size_t j = 0; j < i; ++j) {
      auto& other = everyday[j];
      bool shareDay = false;
      for (int day = 0; day < 7; ++day)
        if (std::find(base.except.begin(), base.except.end(), day) == base.except.end() &&
            std::find(other.except.begin(), other.except.end(), day) == other.except.end())
          shareDay = true;
      if (shareDay && base.start < other.end && other.start < base.end)
        problem(base.line, format("Slot '{1}' overlaps slot '{2}' on line {3}.", base.id,
                                  other.id, other.line));
    }

    for (int day = 0; day < 7; ++day) {
      if (std::find(base.except.begin(), base.except.end(), day) != base.except.end()) continue;

      std::vector<std::string> by;
      std::vector<std::pair<int, int>> covered;
      for (auto& own : slots)
        if (own.everyday.empty() && own.weekday == day && own.start < base.end &&
            base.start < own.end) {
          by.push_back(own.id);
          covered.emplace_back(own.start, own.end);
        }
      if (!by.empty()) {
        // The whole [everyday] slot gives way.  If the day's slots do not
        // cover all of it, some of its time is lost that day: worth a word.
        std::sort(covered.begin(), covered.end());
        int reached = base.start;
        for (auto& range : covered)
          if (range.first <= reached) reached = std::max(reached, range.second);
        bool partly = reached < base.end;
        replaced.push_back({base.id, day, by, partly});
        if (partly)
          warnings.push_back(
              {base.line, format("On {1}, {2} only partly overlaps [everyday] slot '{3}' ({4}-{5}), "
                                 "which is then left out of that whole day.",
                                 Datetime::dayName(day), join(", ", by), base.id,
                                 formatTime(base.start), formatTime(base.end))});
        continue;
      }

      Slot copy = base;
      copy.weekday = day;
      copy.except.clear();
      copy.everyday = base.id;
      copy.id = dayPrefix(day) + "-" + base.id;
      for (auto& existing : slots)
        if (existing.id == copy.id)
          problem(base.line,
                  format("[everyday] slot '{1}' makes '{2}' on {3}, but line {4} already "
                         "defines a slot '{2}'.  Rename one of them.",
                         base.id, copy.id, Datetime::dayName(day), existing.line));
      slots.push_back(copy);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
void Template::parseModeLine(Mode& mode, const std::string& line, int number) {
  auto equals = line.find('=');
  if (equals == std::string::npos) {
    problem(number, "A mode line must look like 'name = value'.");
    return;
  }

  auto name = trim(line.substr(0, equals));
  auto value = trim(line.substr(equals + 1));
  if (name == "params")
    mode.params = tokenize(value);
  else if (name == "active")
    mode.active = value;
  else if (name == "give")
    mode.gives.emplace_back(value, number);
  else if (name == "floor")
    mode.floors.emplace_back(value, number);
  else
    problem(number, format("Unknown mode setting '{1}'.  Use params, active, give or floor.{2}",
                           name, suggest(name, {"params", "active", "give", "floor"})));
}

////////////////////////////////////////////////////////////////////////////////
// Checks what can only be checked once the whole file is read.
void Template::validate() {
  expandEveryday();

  std::vector<std::string> categoryNames;
  for (auto& category : categories) categoryNames.push_back(category.name);
  // Slot ids, including [everyday] ones, which stand for all their copies.
  std::vector<std::string> slotIds;
  for (auto& slot : slots) slotIds.push_back(slot.id);
  for (auto& slot : everyday) slotIds.push_back(slot.id);

  auto isCategory = [&](const std::string& name) {
    return std::find(categoryNames.begin(), categoryNames.end(), name) != categoryNames.end();
  };
  auto isSlot = [&](const std::string& name) {
    return std::find(slotIds.begin(), slotIds.end(), name) != slotIds.end();
  };

  auto checkChain = [&](const std::vector<std::string>& chain, int line,
                        const std::string& what) {
    for (size_t i = 0; i < chain.size(); ++i) {
      if (chain[i] == "rest") {
        if (i + 1 != chain.size())
          problem(line, format("In {1}, 'rest' must come last, since it ends the chain.", what));
      } else if (!isCategory(chain[i]))
        problem(line, format("In {1}, '{2}' is not a category.{3}", what, chain[i],
                             suggest(chain[i], categoryNames)));
    }
  };

  checkChain(fallback, _fallbackLine, "the fallback setting");

  for (auto& name : borrow)
    if (!isCategory(name) && !isSlot(name)) {
      auto names = categoryNames;
      names.insert(names.end(), slotIds.begin(), slotIds.end());
      problem(_borrowLine, format("In the borrow setting, '{1}' is not a slot or a category.{2}",
                                   name, suggest(name, names)));
    }

  for (auto& category : categories)
    if (category.filter.empty())
      problem(category.line, format("Category '{1}' needs a filter.", category.name));

  // Copies of [everyday] slots are checked once, as written.
  std::vector<const Slot*> written;
  for (auto& slot : slots)
    if (slot.everyday.empty()) written.push_back(&slot);
  for (auto& slot : everyday) written.push_back(&slot);
  for (auto slotPointer : written) {
    auto& slot = *slotPointer;
    if (!slot.category.empty() && !isCategory(slot.category))
      problem(slot.line, format("Slot '{1}': '{2}' is not a category.{3}", slot.id,
                                slot.category, suggest(slot.category, categoryNames)));
    for (auto& name : slot.secondary)
      if (!name.empty() && !isCategory(name))
        problem(slot.line, format("Slot '{1}': '{2}' is not a category.{3}", slot.id, name,
                                  suggest(name, categoryNames)));
    if (!slot.secondary.empty() && (slot.kind == Kind::fixed || slot.kind == Kind::rest))
      problem(slot.line, format("A {1} slot never takes tasks, so it cannot have more than one "
                                "category.",
                                kindName(slot.kind)));
    if (slot.hasFallback) checkChain(slot.fallback, slot.line, "the fallback option");
  }

  // Overlapping slots on the same day.  Copies of [everyday] slots never
  // overlap a day's own slots, which replace them.
  for (size_t i = 0; i < slots.size(); ++i)
    for (size_t j = i + 1; j < slots.size(); ++j)
      if (slots[i].everyday.empty() && slots[j].everyday.empty() &&
          slots[i].weekday == slots[j].weekday && slots[i].start < slots[j].end &&
          slots[j].start < slots[i].end)
        problem(slots[j].line, format("Slot '{1}' overlaps slot '{2}' on line {3}.", slots[j].id,
                                      slots[i].id, slots[i].line));

  // Modes refer to categories, slots, kinds and their own params.
  for (auto& mode : modes) {
    auto isTarget = [&](const std::string& name) {
      if (name.size() > 1 && name[0] == '$')
        return std::find(mode.params.begin(), mode.params.end(), name.substr(1)) !=
               mode.params.end();
      return isCategory(name);
    };
    auto isSelector = [&](const std::string& name) {
      if (name.substr(0, 5) == "kind:")
        return std::any_of(kinds.begin(), kinds.end(),
                           [&](auto& k) { return k.first == name.substr(5); });
      return isCategory(name) || isSlot(name);
    };

    if (!mode.active.empty()) {
      auto tokens = tokenize(mode.active);
      bool before = tokens.size() == 3 && tokens[1] == "before" && tokens[0].size() > 1 &&
                    tokens[0].back() == 'd' &&
                    std::all_of(tokens[0].begin(), tokens[0].end() - 1, ::isdigit) &&
                    isTarget(tokens[2]) && tokens[2][0] == '$';
      bool until = tokens.size() == 2 && tokens[0] == "until" && isTarget(tokens[1]) &&
                   tokens[1][0] == '$';
      if (!before && !until)
        problem(mode.line, format("Mode '{1}': active must look like '7d before $date' or "
                                  "'until $end', using one of its params.",
                                  mode.name));
      else if (before) {
        mode.activeKind = Mode::Active::before;
        mode.activeDays = std::stoi(tokens[0].substr(0, tokens[0].size() - 1));
        mode.activeParam = tokens[2].substr(1);
      } else {
        mode.activeKind = Mode::Active::until;
        mode.activeParam = tokens[1].substr(1);
      }
    }

    // give = <selector> [N% | except N] -> <target>
    for (auto& give : mode.gives) {
      auto tokens = tokenize(give.first);
      auto arrow = std::find(tokens.begin(), tokens.end(), "->");
      Mode::Give rule;
      bool valid = arrow != tokens.end() && arrow + 2 == tokens.end();
      if (valid && arrow - tokens.begin() == 2) {
        double percent;
        valid = tokens[1].back() == '%' &&
                isNumber(tokens[1].substr(0, tokens[1].size() - 1), percent) && percent > 0 &&
                percent <= 100 && percent == static_cast<int>(percent);
        rule.percent = valid ? static_cast<int>(percent) : 100;
      } else if (valid && arrow - tokens.begin() == 3) {
        double except;
        valid = tokens[1] == "except" && isNumber(tokens[2], except) && except >= 0 &&
                except == static_cast<int>(except);
        rule.except = valid ? static_cast<int>(except) : -1;
      } else
        valid = valid && arrow - tokens.begin() == 1;
      if (!valid) {
        problem(give.second, "A give rule must look like 'give = <slots> -> <category>', "
                             "optionally with 50% or 'except 1' before the arrow.");
        continue;
      }
      if (!isSelector(tokens[0]))
        problem(give.second, format("'{1}' is not a category, slot or kind:<kind>.", tokens[0]));
      if (!isTarget(tokens.back()))
        problem(give.second,
                format("'{1}' is not a category, or one of the mode's params such as $course.",
                       tokens.back()));
      rule.selector = tokens[0];
      rule.target = tokens.back();
      mode.giveRules.push_back(rule);
    }

    // floor = <category> <hours>
    for (auto& floor : mode.floors) {
      auto tokens = tokenize(floor.first);
      double hours;
      if (tokens.size() != 2 || !isNumber(tokens[1], hours) || hours < 0)
        problem(floor.second, "A floor rule must look like 'floor = <category> <hours>'.");
      else if (!isTarget(tokens[0]))
        problem(floor.second, format("'{1}' is not a category.", tokens[0]));
      else
        mode.floorRules.push_back({tokens[0], hours});
    }
  }

  if (configTime("schedule.wake", -1) == -1)
    problem(0, "The setting schedule.wake must be a time, such as 06:00.");
  if (configTime("schedule.sleep", -1) == -1)
    problem(0, "The setting schedule.sleep must be a time, such as 24:00.");

  std::stable_sort(problems.begin(), problems.end(),
                   [](auto& a, auto& b) { return a.line < b.line; });
}

////////////////////////////////////////////////////////////////////////////////
void Template::problem(int line, const std::string& message) {
  problems.push_back({line, message});
}

////////////////////////////////////////////////////////////////////////////////
const Category* Template::category(const std::string& name) const {
  for (auto& category : categories)
    if (category.name == name) return &category;
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
const Mode* Template::mode(const std::string& name) const {
  for (auto& mode : modes)
    if (mode.name == name) return &mode;
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
const Slot* Template::slot(const std::string& id) const {
  for (auto& slot : slots)
    if (slot.id == id) return &slot;
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
const Slot* Template::everydaySlot(const std::string& id) const {
  for (auto& slot : everyday)
    if (slot.id == id) return &slot;
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
std::string Template::filterFor(const Slot& slot) const {
  if (!slot.filter.empty()) return slot.filter;
  auto c = category(slot.category);
  return c ? c->filter : "";
}

////////////////////////////////////////////////////////////////////////////////
const std::vector<std::string>& Template::fallbackFor(const Slot& slot) const {
  return slot.hasFallback ? slot.fallback : fallback;
}

}  // namespace schedule

////////////////////////////////////////////////////////////////////////////////
