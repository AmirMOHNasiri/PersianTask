////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2026, AmirMOHNasiri.
//
// Part of PersianTask, released under the MIT license, like Taskwarrior.
//
// https://www.opensource.org/licenses/mit-license.php
//
////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDED_SCHEDULEVIEW
#define INCLUDED_SCHEDULEVIEW

#include <Color.h>
#include <SchedulePlanner.h>
#include <ScheduleTemplate.h>

#include <string>
#include <vector>

// Helpers shared by the schedule commands.
namespace schedule {

extern const std::string noTemplateHint;

Color configColor(const std::string&);
std::string problemsNote(const Template&);

// Loads the template.  Throws the init hint if there is none, and the
// problems note if it has mistakes.
void loadValidTemplate(Template&);

// Parses a whole word as a date, such as 'farda' or '1405-08-10'.
bool parseDay(const std::string&, Datetime&);

// Removes a trailing week selector from the arguments, 'next', 'prev' or
// 'week:<date>', and returns a day of the selected week: by default, today.
Datetime takeWeek(std::vector<std::string>&);

// A day of the week, such as 'Doshanbe 27 Mehr', for messages.
std::string describeDay(const Datetime&);
std::string describePlace(const Datetime&, int start, int end);

// A table of the assignments, with a Day column if days is true.
std::string renderTable(const std::vector<Assignment>&, bool days, Planner&);

// The planned hours of each category, in template order.
std::string renderHours(const Template&, const std::vector<Assignment>&, Planner&);

// "2h (3h)": the estimate and the planned time, and what is left of it once
// time has been tracked.
// For a later week, before is the time planned in the weeks before it.
std::string describeEffort(const Effort&, long before = 0);

// The Pace column of an assignment: on track, behind, overdue...
std::string paceOf(const Assignment&, const Planner&);

// The notes of a week and its plan, one line each, starting with overloads.
std::string renderNotes(const Week&, const Planner&);
std::string renderOverloads(const Planner&);

}  // namespace schedule

#endif
////////////////////////////////////////////////////////////////////////////////
