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
#include <Duration.h>
#include <ScheduleAsk.h>
#include <ScheduleEffort.h>
#include <SchedulePlanner.h>
#include <ScheduleTemplate.h>
#include <format.h>
#include <shared.h>
#include <unistd.h>

#include <iostream>

namespace schedule {

namespace {

// The categories of the template that the task matches, in template order.
std::vector<const Category*> categoriesOf(const Template& tmpl, const Task& task) {
  std::vector<const Category*> matched;
  auto now = Datetime().toEpoch();
  for (auto& category : tmpl.categories)
    if (!category.filter.empty() && !filterTasks(category.filter, {task}, now).empty())
      matched.push_back(&category);
  return matched;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
void askEstimate(Task& task, const Task* before) {
  auto& config = Context::getContext().config;
  if (!config.getBoolean("schedule") || !config.getBoolean("schedule.estimate.ask")) return;
  if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) return;
  if (!estimateUdaProblem().empty()) return;

  // Only a pending task with no estimate, which did not just lose one.
  auto uda = estimateUda();
  if (!task.get(uda).empty() || (before && !before->get(uda).empty())) return;
  auto exactName = exactUda();
  if (!exactName.empty() && (!task.get(exactName).empty() ||
                             (before && !before->get(exactName).empty())))
    return;
  auto status = task.getStatus();
  if (status == Task::completed || status == Task::deleted) return;

  Template tmpl;
  if (!tmpl.load() || !tmpl.problems.empty()) return;

  // It must come into a category: when modified, from none.
  auto matched = categoriesOf(tmpl, task);
  if (matched.empty() || (before && !categoriesOf(tmpl, *before).empty())) return;

  // The default it would get: its accounting category's, or the first one.
  long fallback = -1;
  auto counting = tmpl.category(tmpl.accountFor([&](const Category& category) {
    return std::find(matched.begin(), matched.end(), &category) != matched.end();
  }));
  if (counting && counting->estimate > 0)
    fallback = counting->estimate;
  else
    for (auto category : matched)
      if (category->estimate > 0) {
        fallback = category->estimate;
        break;
      }

  auto hint = fallback > 0 ? format("Enter for the category default, {1}", formatHours(fallback))
                           : std::string("Enter for none");
  auto exactOk = exactUdaProblem().empty();
  for (;;) {
    std::cout << format("Estimate for '{1}' ({2}, padded by estimate.factor{3}; {4}): ",
                        task.get("description"), "e.g. 3h",
                        exactOk ? ", or =3h exact, as it is" : "", hint)
              << std::flush;
    std::string answer;
    if (!std::getline(std::cin, answer)) return;
    answer = trim(answer);
    if (answer.empty()) return;

    // '=3h' is an exact estimate, never padded.
    bool exact = exactOk && answer[0] == '=';
    auto seconds = parseHours(exact ? trim(answer.substr(1)) : answer);
    if (seconds > 0) {
      task.set(exact ? exactUda() : uda, Duration(seconds).formatISO());
      return;
    }
    std::cout << "That is not a duration, such as 3h, 1.5h or 90min, or =1.5h for an exact one.\n";
  }
}

}  // namespace schedule

////////////////////////////////////////////////////////////////////////////////
