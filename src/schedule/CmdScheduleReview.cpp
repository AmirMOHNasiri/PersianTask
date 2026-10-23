////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

// task schedule review: planned and tracked hours per category, floors,
// the buffer, and deadlines.

#include <cmake.h>
// cmake.h include header must come first

#include <CmdSchedule.h>
#include <Context.h>
#include <ScheduleEffort.h>
#include <ScheduleHelp.h>
#include <SchedulePlanner.h>
#include <ScheduleState.h>
#include <ScheduleTemplate.h>
#include <ScheduleView.h>
#include <Table.h>
#include <format.h>
#include <shared.h>
#include <util.h>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

using namespace schedule;

namespace {

// The numbers of one week.
struct WeekReview {
  Datetime start;
  bool finished{false};
  std::map<std::string, long> planned;  // Seconds, by category.
  std::map<std::string, long> actual;   // Seconds, by category, "" for none.
  long trackedTotal{0};

  // Copied, since the week the slot belongs to does not outlive this.
  struct Buffer {
    std::string id;
    std::string place;
    std::string use;  // Empty if free.
  };
  std::vector<Buffer> buffers;
  bool bufferFull{false};

  std::map<std::string, double> floors;  // Hours, after modes; -1 for none.
};

Datetime weeksBefore(const Datetime& day, int weeks) {
  Datetime result = day;
  for (int i = 0; i < 7 * weeks; ++i) --result;
  return result;
}

Datetime weeksAfter(const Datetime& day, int weeks) {
  Datetime result = day;
  for (int i = 0; i < 7 * weeks; ++i) ++result;
  return result;
}

WeekReview reviewWeek(const Template& tmpl, const State& state, const Datetime& day) {
  auto& context = Context::getContext();

  WeekReview review;
  Week week(tmpl, day, &state);
  review.start = week.start;
  review.finished = week.end() <= schedule::now();
  for (auto& category : tmpl.categories)
    review.floors[category.name] = week.floorOf(tmpl, category.name);

  // Planned: slot lengths after this week's changes.
  Planner planner(tmpl, week.reference(), std::make_shared<Horizon>(tmpl, &state));
  auto assignments = planner.plan(week);
  for (auto& entry : planner.minutesByCategory(assignments))
    review.planned[entry.first] = entry.second * 60L;

  // Actual: time tracked with start and stop within the week, on any task,
  // including those finished since.  Starting and stopping a task modifies
  // it, so tasks unmodified since the week began have nothing to count.
  auto from = week.start.toEpoch();
  auto to = week.end().toEpoch();
  std::vector<Task> worked;
  std::map<std::string, long> seconds;
  for (auto& task : context.tdb2.all_tasks()) {
    if (task.get_date("modified") < from && !task.has("start")) continue;

    long total = 0;
    for (auto& interval : trackedIntervals(task))
      total += std::max<long>(0, std::min(interval.second, to) - std::max(interval.first, from));
    if (total > 0) {
      worked.push_back(task);
      seconds[task.get("uuid")] = total;
    }
  }

  // Each task counts for one category, as Template::accountFor says.
  std::map<std::string, std::set<std::string>> matches;  // Category, uuids.
  for (auto& category : tmpl.categories)
    for (auto& task : planner.evaluate(category.filter, worked))
      matches[category.name].insert(task.get("uuid"));

  for (auto& entry : seconds) {
    auto category = tmpl.accountFor(
        [&](const Category& c) { return matches[c.name].count(entry.first) > 0; });
    review.actual[category] += entry.second;
    review.trackedTotal += entry.second;
  }

  // Buffer slots, and what they were used for.
  for (auto& assignment : assignments) {
    auto& instance = *assignment.instance;
    if (instance.event || instance.slot->kind != Kind::buffer) continue;

    std::string use;
    if (assignment.task) {
      auto description = format("task {1}, '{2}'", assignment.task->id,
                                assignment.task->get("description"));
      if (assignment.source == Source::borrowed)
        use = "lent to " + description;
      else if (assignment.source == Source::pinned)
        use = "assigned to " + description;
      else
        use = "used for " + description;
    }
    review.buffers.push_back(
        {instance.id, describePlace(instance.day, instance.start, instance.end), use});
  }

  // Full: every buffer slot was lent or assigned, which is the sign of work
  // overflowing the plan.  Fallback work in a buffer does not make it full.
  review.bufferFull = !review.buffers.empty();
  for (auto& assignment : assignments)
    if (!assignment.instance->event && assignment.instance->slot->kind == Kind::buffer &&
        assignment.source != Source::borrowed && assignment.source != Source::pinned)
      review.bufferFull = false;

  return review;
}

std::string hours(long seconds) { return seconds > 0 ? formatHours(seconds) : "0h"; }

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// review [prev | last | next | week:<date>]
int CmdSchedule::review(std::vector<std::string> args, std::string& output) {
  if (!args.empty() && args.back() == "last") args.back() = "prev";
  auto day = takeWeek(args);
  if (!args.empty()) throw std::string("Usage: task schedule review [prev | next | week:<date>]");

  Template tmpl;
  loadValidTemplate(tmpl);
  State state;
  state.load();

  auto& context = Context::getContext();
  Week week(tmpl, day, &state);
  auto review = reviewWeek(tmpl, state, day);

  std::stringstream out;
  out << '\n'
      << configColor("color.schedule.header")
             .colorize("Review of " + week.label() + (review.finished ? "" : " (so far)"))
      << "\n\n";

  // Hours.
  Table table;
  table.width(context.getWidth());
  table.withColor(context.color());
  table.add("Category");
  table.add("Planned", false);
  table.add("Tracked", false);
  table.add("Floor", false);
  table.add("");
  setHeaderUnderline(table);

  for (auto& category : tmpl.categories) {
    if (!category.accounting) continue;
    auto planned = review.planned[category.name];
    auto actual = review.actual[category.name];
    auto floor = review.floors[category.name];
    if (!planned && !actual && floor < 0) continue;

    auto row = table.addRow();
    table.set(row, 0, category.name);
    table.set(row, 1, hours(planned));
    table.set(row, 2, hours(actual));
    table.set(row, 3, floor < 0 ? "-" : hours(static_cast<long>(floor * 3600)));
    if (floor >= 0 && review.finished && actual < floor * 3600)
      table.set(row, 4, "below its floor");
  }
  if (review.actual[""] > 0) {
    auto row = table.addRow();
    table.set(row, 0, "(no category)");
    table.set(row, 2, hours(review.actual[""]));
  }
  out << table.render() << '\n';
  if (review.trackedTotal == 0)
    out << "No time was tracked this week.  Tracked hours come from 'task start' and 'task "
           "stop'.\n\n";

  // Floors, over the last two finished weeks.
  auto last = review.finished ? day : weeksBefore(day, 1);
  auto recent = reviewWeek(tmpl, state, last);
  auto earlier = reviewWeek(tmpl, state, weeksBefore(last, 1));
  bool usePlanned = recent.trackedTotal == 0 && earlier.trackedTotal == 0;

  out << "Floors\n";
  if (usePlanned)
    out << "  No time was tracked in the last two finished weeks, so floors are checked\n"
           "  against planned hours.\n";

  bool anyMissed = false;
  for (auto& category : tmpl.categories) {
    if (!category.accounting) continue;

    // Modes may change a floor, so each week has its own.
    long floorA = static_cast<long>(earlier.floors[category.name] * 3600);
    long floorB = static_cast<long>(recent.floors[category.name] * 3600);
    if (floorA <= 0 || floorB <= 0) continue;

    auto a = usePlanned ? earlier.planned[category.name] : earlier.actual[category.name];
    auto b = usePlanned ? recent.planned[category.name] : recent.actual[category.name];
    if (a < floorA && b < floorB) {
      out << format("  {1} was below its floor of {2} two weeks running: {3} in week {4}, and {5} "
                    "in week {6}.\n",
                    category.name, hours(floorB), hours(a), earlier.start.week(), hours(b),
                    recent.start.week());
      anyMissed = true;
    }
  }
  if (!anyMissed)
    out << format("  No category was below its floor two weeks running (weeks {1} and {2}).\n",
                  earlier.start.week(), recent.start.week());
  out << '\n';

  // The buffer.
  out << "Buffer\n";
  if (review.buffers.empty())
    out << "  The template has no buffer slots.\n";
  for (auto& buffer : review.buffers)
    out << format("  {1} ({2}): {3}\n", buffer.id, buffer.place,
                  buffer.use.empty() ? "free" : buffer.use);

  auto previous = reviewWeek(tmpl, state, weeksBefore(day, 1));
  if (review.bufferFull && previous.bufferFull) {
    // Suggest the first category the template borrows from after the buffer.
    std::string lighter;
    for (auto& name : tmpl.borrow) {
      auto slot = tmpl.slot(name);
      if (slot && slot->kind == Kind::buffer) continue;
      lighter = slot ? slot->category : name;
      if (!lighter.empty()) break;
    }
    out << "  The buffer was full two weeks running, so the plan is too heavy.\n";
    if (!lighter.empty())
      out << format("  Consider fewer {1} slots, such as one a week: after the buffer, {1}\n"
                    "  is the first to lend time.\n",
                    lighter);
  }
  out << '\n';

  // Deadlines this week and next.
  out << "Deadlines\n";
  auto nextStart = week.end();
  auto afterNext = weeksAfter(nextStart, 1);
  std::vector<Task> due;
  for (auto& task : context.tdb2.pending_tasks())
    if (task.getStatus() == Task::pending && task.has("due") &&
        task.get_date("due") < afterNext.toEpoch() && task.get_date("due") >= week.start.toEpoch())
      due.push_back(task);
  std::sort(due.begin(), due.end(),
            [](Task& a, Task& b) { return a.get_date("due") < b.get_date("due"); });

  auto now = schedule::now();
  for (auto [title, from, to] : {std::tuple{"This week", week.start, nextStart},
                                 std::tuple{"Next week", nextStart, afterNext}}) {
    out << "  " << title << ":\n";
    bool any = false;
    for (auto& task : due) {
      auto when = Datetime(task.get_date("due"));
      if (when < from || !(when < to)) continue;
      out << format("    {1}  {2}  {3}{4}{5}\n", when.toString("A d B"), task.id,
                    task.get("description"),
                    task.has("project") ? " (" + task.get("project") + ")" : "",
                    when < now ? ", overdue" : "");
      any = true;
    }
    if (!any) out << "    None.\n";
  }

  // Whether the work due fits in the slots before it, from now on.
  WeekPlan current(tmpl, now.startOfDay(), &state);
  auto overloads = renderOverloads(current.planner);
  if (!overloads.empty()) out << "  " << overloads;

  output = out.str();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
