# PersianTask changelog

PersianTask's own changes, release by release. Each release is built on an
upstream Taskwarrior version, which its number starts with: `3.5.0-persiantask.1`
is PersianTask's first release, on Taskwarrior 3.5.0. Upstream's own changes
are in [ChangeLog](ChangeLog).

Numbering: within one upstream version the counter goes up (`3.5.0-persiantask.2`),
and on a new upstream version it starts again at 1 (`3.6.0-persiantask.1`).

## Unreleased

### The weekly schedule

- `task schedule borrow <task> <hours> force` may take slots below their
  categories' floors when the slots that keep floors are not enough, and
  says which floors it broke and by how much.
- Opt-in: with `critical.break-floors = H` in the template, a priority:H task
  that would otherwise miss its due date takes the borrow-order slots it
  needs, below floors if it must. It is off unless set; the example template
  sets it.
- The review lists the floors broken on purpose this week, and by which
  tasks, and warns when one was broken two weeks running.
- A task that does not fit before it is due is told what would help:
  `borrow ... force`, priority:H, or, when even every slot is not enough,
  asking for an extension or reducing scope.

## 3.5.0-persiantask.5 (2026-10-26)

### New commands

- `task schedule now`: what to work on right now. In a break or a class, the
  best task for the moment, and how to take it.
- `task schedule peek <slot>`: a slot's task, then the next tasks it would
  take, with why some are not planned there now.
- `task schedule next <slot>`: gives a slot its next candidate for the week;
  repeat it to go on down the list.
- `task schedule open <slot> [HH:MM]`: lets a fixed slot take a task this
  week, for a class that is cancelled or ends early.

### The template

- A slot can list several categories in order of priority, such as
  `study ds,algo`: when its own category has nothing to do, it takes the next
  one's task, before the fallback chain, and never a task that another slot's
  own category was meant for.
- `task schedule check` warns when a category's filter uses a virtual tag in
  lower case, such as `+pending`, which is an ordinary tag that no task has
  (virtual tags are in capitals: `+PENDING`), and notes the categories that
  slots use and no pending task matches.

### Estimates

- Exact estimates: `exact:1.5h` is planned as it is, never padded by
  `estimate.factor`, for work whose length you know, such as a lecture video.
  It shows as `1.5h exact`. When `task add` asks for an estimate, `=1.5h`
  gives an exact one, and `task schedule left` keeps it exact.

### Documentation

- The README now lists every schedule command with an example, and every
  template section, setting and slot option.

## 3.5.0-persiantask.4 (2026-10-24)

### The weekly schedule

- Time tracked on a task could be counted as zero when `task start` and
  another change to the task, such as `task modify start:...`, happened in
  the same second. It is now always counted.
- A task's share of a week is never below even progress toward its due
  date. Before, a task that other slots could reach only in its last days
  (through `soon`) got a small share early on, showed `on track`, and was
  left for the last week; now it shows `behind` when its slots cannot keep
  up.

## 3.5.0-persiantask.3 (2026-10-23)

### The weekly schedule

- A compact layout for narrow terminals, chosen by width: short day names,
  no wrapped rows, a dimmed `-` for slots with nothing to do, `soon*` for
  work from the fallback chain, and the estimates and pace listed once under
  the table. `task config schedule.layout full` or `compact` picks one for
  good.
- In a later week, such as `task schedule week next`, the `Estimate` column
  says how much of a task is left after the weeks before it, as in
  `6h (9h), 5h left`, so that free slots after a task's last planned hours
  are no longer puzzling.

## 3.5.0-persiantask.2 (2026-10-23)

The schedule now plans by deadlines and the work left, not only by urgency.

### Planning by due dates

- Pacing: a task with an estimate and a due date has its work spread over
  the weeks until it is due, instead of waiting for its last week. A task
  that falls behind gets more slots, first in its own category, then in empty
  slots that its fallback chain reaches. One that cannot be finished in time
  otherwise takes any slot it can reach, except protected slots and those
  whose category would fall below its weekly floor.
- A new `Pace` column says how each such task stands: `on track`,
  `behind 2h`, `short 3h` or `overdue`. It appears only when needed.
- Overload warning: today's schedule, the week and the review say when the
  work due by a date does not fit in the slots before it, as in
  `Overloaded before Doshanbe 27 Mehr: 12h of work, 10h of slots`.
- A task is never planned in a slot that starts after it is due. An overdue
  task can still take any slot; it is treated as urgent for
  `overdue.critical-days` (7 by default), and then planned as usual.
- `task schedule week next` (or any later week) is planned as if the work
  planned before it were done, so nothing is planned twice.
- Slots of this week that have already ended show `past` and take no task,
  so they no longer use up a task's time for the rest of the day.
- New template settings: `pacing` (on by default; `off` plans as before),
  `max.per-day` (the most time one task gets in a day) and
  `overdue.critical-days`.

### Estimates

- Default estimates: `estimate:4h` at the end of a category's line is the
  estimate of its tasks that have none, shown as `4h default`.
- `task add` and `task modify` ask for an estimate, on a terminal, when a task
  comes into one of your categories without one. `task config
  schedule.estimate.ask off` turns it off.
- A task that has used up its estimate shows `over estimate`, and
  `task schedule left <task> <hours>` sets the estimate so that the given
  hours are left.

### The template

- An `[everyday]` section for slots that every day has, such as lunch. Each
  day gets its own copy (`sat-lunch`, `sun-lunch`, ...), which commands such
  as `move` and `swap` change one day at a time. A day's own slot at that time
  replaces it on that day, and `except:jome` leaves out days. The example
  template uses it for lunch and dinner.
- `task schedule check --expand` lists each day's slots as the schedule uses
  them, and `check` warns when a day's slot only partly overlaps an
  `[everyday]` slot, which then drops out of that whole day.
- A slot that finds nothing to do now shows `nothing to do` and
  `rest (fallback)`, dimmed, so that it is not mistaken for a real break, such
  as lunch, which still shows `rest`.

## 3.5.0-persiantask.1 (2026-10-20)

The first release: Taskwarrior 3.5.0 with the Jalali calendar, and a weekly
schedule.

### The Jalali calendar

- Dates are shown and entered in the Jalali (Shamsi) calendar: `due:1405-08-01`,
  `task calendar` with Jalali months, and Jalali dates in every report and in
  `task info`.
- Finglish month and day names, which can be abbreviated: `due:esfand`,
  `due:shanbe`, `due:far`.
- Persian and Arabic-Indic digits are accepted when entering dates:
  `due:۱۴۰۵-۰۸-۰۱`.
- Persian names for nearby days: `pariruz`, `diruz`, `emruz`, `farda`,
  `pasfarda`.
- Weeks start on Saturday. The weekend is Thursday and Friday, and the work
  week, as in `soww`, `eoww` and `recur:weekdays`, is Saturday to Wednesday.
- `som`, `eom`, `soq`, `eoq`, `soy`, `eoy`, `15th` and `recur:monthly` follow
  Jalali months, seasons and years.
- Gregorian dates are still accepted (`due:2026-10-23`), and machine formats
  stay Gregorian: `export`, `import`, hooks and holiday files work with other
  tools unchanged. Your Taskwarrior data works unchanged too.

### Calendar settings

- `calendar=jalali|gregorian`: with `gregorian`, PersianTask behaves exactly
  as upstream Taskwarrior, which upstream's own test suite checks.
- `dayname.style=finglish|english`: Shanbe or Saturday, in any calendar.
- Both are ordinary settings, set with `task config` and listed by
  `task show`. `task config calendar gregorian` reminds you that `weekstart`
  is still Saturday, without changing it.
- Changing the calendar never changes your data. Recurring tasks remember the
  calendar they were created under, in an `rcalendar` attribute, so their
  months and weekdays stay the same after a switch. Recurring tasks from before
  this release count as Jalali.
- A date that only makes sense in the other calendar, such as `1405-07-26`
  with `calendar=gregorian`, is refused with a hint, never guessed.

See [docs/calendar.md](docs/calendar.md).

### The weekly schedule

Describe your typical week once, as a template of time slots, and each slot
picks the task to work on. See [docs/schedule.md](docs/schedule.md).

- **The template**: categories (Taskwarrior filters, with optional weekly
  floors of hours), and slots for each day, `[shanbe]` to `[jome]` or
  `[saturday]` to `[friday]`. Slots that find no work in their category go
  down a fallback chain. `task schedule init` creates one from an example, a
  university student's week, and `task schedule check` reports mistakes with
  line numbers.
- **Today and the week**: plain `task` shows today's slots and their tasks,
  then the most important tasks; `task schedule week` shows a whole week, and
  `next`, `prev` or a date picks another. Your usual report is still `task
  tasks`, and `schedule.default=off` brings back plain `task` as it was.
- **Changing one week**: `swap`, `move`, `skip`, `assign` and `reset` change a
  week without touching the template, and `event add` adds a one-time
  appointment. `task schedule undo` and `task schedule log` undo and list
  these changes.
- **Estimates**: an `estimate` UDA, such as `estimate:4h`, makes a task take
  enough slots, with a safety factor, minus the time tracked with `task start`
  and `task stop`. Tiny estimates, such as `4s` for `4h`, are pointed out.
- **Borrowing**: `task schedule borrow 12 2` gives a task two more hours this
  week, from the slots the template allows, never taking a category below its
  floor. The planned hours of each category are shown under the week.
- **The weekly review**: `task schedule review` compares planned and tracked
  hours per category, warns about categories below their floor two weeks
  running and about a buffer that keeps filling up, and lists the deadlines of
  this week and the next.
- **Modes**: `task schedule mode start exam os 1405-09-20` switches on a mode
  of the template, such as exam season, which gives slots to other categories
  and raises floors for a while, and can end on its own.
- The schedule follows the calendar settings and `weekstart`.

### Packages

- A pacman repository with prebuilt, signed packages for Arch Linux and
  derivatives, updated with `pacman -Syu`, and a `PKGBUILD` to build from
  source.
- Upstream Taskwarrior is merged every week, once the merge passes
  PersianTask's tests.
