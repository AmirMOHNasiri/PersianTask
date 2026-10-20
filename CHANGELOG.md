# PersianTask changelog

PersianTask's own changes, release by release. Each release is built on an
upstream Taskwarrior version, which its number starts with: `3.5.0-persiantask.1`
is PersianTask's first release, on Taskwarrior 3.5.0. Upstream's own changes
are in [ChangeLog](ChangeLog).

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
