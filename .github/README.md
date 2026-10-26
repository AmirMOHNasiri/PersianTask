# PersianTask

**Taskwarrior with the Jalali (Persian / Shamsi) calendar, and a weekly schedule**

PersianTask is a version of [Taskwarrior](https://taskwarrior.org) in which all dates, both entered and displayed, use the Jalali calendar, the official calendar of Iran. It also plans your week: a [weekly schedule](#weekly-schedule) of time slots, each of which pulls the right task to work on. Everything else works exactly like Taskwarrior. Upstream Taskwarrior updates are merged every week, once they pass the Jalali calendar tests. What each release brings is in [CHANGELOG.md](../CHANGELOG.md).

```
$ task add meeting due:1405-08-01
$ task add report due:eom
$ task calendar

         Mehr 1405                 Aban 1405                 Azar 1405
     Sh Ye Do Se Ch Pa Jo      Sh Ye Do Se Ch Pa Jo      Sh Ye Do Se Ch Pa Jo
  27              1  2  3   31                    1   36     1  2  3  4  5  6
  28  4  5  6  7  8  9 10   32  2  3  4  5  6  7  8   37  7  8  9 10 11 12 13
  ...
```

## Installation

### Arch Linux (and Manjaro, EndeavourOS, ...)

PersianTask has its own pacman repository, with prebuilt and signed packages.

1. Trust the signing key:

   ```
   curl -O https://raw.githubusercontent.com/AmirMOHNasiri/PersianTask/main/pkg/arch/persiantask.asc
   sudo pacman-key --add persiantask.asc
   sudo pacman-key --lsign-key DCFAB320BDF35972810853F3A52EE6D3F3739E31
   ```

2. Add the repository to the end of `/etc/pacman.conf`:

   ```
   [persiantask]
   Server = https://github.com/AmirMOHNasiri/PersianTask/releases/download/repo
   ```

3. Install:

   ```
   sudo pacman -Syu persiantask
   ```

The package replaces the official `task` package, if it is installed. Updates arrive with `pacman -Syu` (or `yay -Syu`), like any other package.

#### Building from source instead

```
mkdir persiantask && cd persiantask
curl -O https://raw.githubusercontent.com/AmirMOHNasiri/PersianTask/main/pkg/arch/PKGBUILD
makepkg -si
```

This builds the `persiantask-git` package from the latest commit, which takes several minutes. The build runs the Jalali calendar tests, and stops if any fails. To update, run `makepkg -si` again in the same directory.

### Other Linux distributions

Requirements: `git`, `cmake`, `g++`, `libuuid` (`uuid-dev` on Debian and Ubuntu) and [Rust](https://rustup.rs).

```
git clone --recursive https://github.com/AmirMOHNasiri/PersianTask.git
cd PersianTask
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

Remove any Taskwarrior installed from your distribution's repositories first.

## Usage

All Taskwarrior commands work as usual; only the dates are Jalali.

| Input | Meaning |
|---|---|
| `due:1405-08-01` | 1 Aban 1405 |
| `due:۱۴۰۵-۰۸-۰۱` | The same, with Persian digits |
| `due:esfand` | The start of the next Esfand |
| `due:shanbe` | Next Saturday |
| `due:15th` | The next 15th of a Jalali month |
| `due:sow` / `due:eow` | Start / end of this week (Saturday to Friday) |
| `due:soww` / `due:eoww` | Start / end of this work week (Saturday to Wednesday) |
| `due:som` / `due:eom` | Start / end of this Jalali month |
| `due:soq` / `due:eoq` | Start / end of this season (quarter) |
| `due:soy` / `due:eoy` | Start / end of this Jalali year |
| `recur:monthly` | Repeats every Jalali month |
| `recur:weekdays` | Repeats on work days (Saturday to Wednesday) |

Months: Farvardin, Ordibehesht, Khordad, Tir, Mordad, Shahrivar, Mehr, Aban, Azar, Dey, Bahman, Esfand.

Days: Shanbe, Yekshanbe, Doshanbe, Seshanbe, Chaharshanbe, Panjshanbe, Jome. English day names, such as `friday`, are accepted too.

Names may be abbreviated to three or more letters, such as `due:far` or `due:pan`. Since `sha` matches Shanbe, use at least `shah` for Shahrivar.

Nearby days can also be written in Persian, in full: `pariruz`, `diruz`, `emruz`, `farda` and `pasfarda` (two days ago, yesterday, today, tomorrow, and the day after).

The display format is set with `dateformat`. For example, `task config dateformat 'A d B Y'` shows dates as `Panjshanbe 30 Mehr 1405`.

## Weekly schedule

Describe your typical week once, as a template of time slots: classes, study blocks, research time. Each slot pulls a pending task from its category (a Taskwarrior filter, such as `project:os`), or from a fallback chain when that category has nothing to do. Plain `task` then shows today's slots and what to do in each, followed by the most important tasks.

### Quick start

1. Install PersianTask (above).
2. Create a template from the example, a university student's week:

   ```
   task schedule init
   ```

   The very first time you run `task`, it offers to create `~/.taskrc`: answer `yes`.

3. Edit `~/.task/schedule`: your categories, and your slots for each day, `[shanbe]` to `[jome]`. A slot is one line:

   ```
   [shanbe]
   07:30-10:30   sat-research   study   research
   14:00-16:00   sat-os-hw      study   os         filter:"project:os +homework"
   ```

   Breaks that every day has, such as lunch, go once in `[everyday]`; a day's own slot at that time replaces them on that day:

   ```
   [everyday]
   12:30-13:30   lunch          rest    -          except:jome
   ```

4. Check it, and look at your week:

   ```
   task schedule check
   task schedule check --expand   # Each day's slots, with the [everyday] ones
   task schedule week
   ```

```
$ task schedule week
Week 31: Shanbe 25 Mehr 1405 - Jome 1 Aban 1405

Day              #  Time         Slot           ID  Task            Estimate  From
Shanbe 25 Mehr   1  07:00-07:30  sat-review      4  Term project              term (fallback)
                 2  07:30-10:30  sat-research    2  Paper draft     2h (3h)   research
                 3  11:00-12:30  sat-ds          3  Tree exercises            ds
                 4  12:30-13:30  sat-lunch                                    rest
                 5  14:00-16:00  sat-os-hw       1  OS homework 3             os
```

### Commands

Slots are named by their id in the template, such as `sat-ds`, or by the number `task schedule week` shows next to them. Commands that change a slot apply to this week, or to the week given at the end: `next`, `prev` or `week:<date>`.

| Command | What it does | Example |
|---|---|---|
| `task schedule today` (or plain `task`) | Today's slots and their tasks, then the important tasks | `task schedule` |
| `task schedule now` | What to work on right now, or the best task for a break or a cancelled class | `task schedule now` |
| `task schedule week [next \| prev \| <date>]` | A whole week of slots and tasks | `task schedule week next` |
| `task schedule peek <slot> [n]` | A slot's task, and the next tasks it would take, with why some are not planned there | `task schedule peek thu-ds` |
| `task schedule next <slot>` | Gives a slot its next candidate, this week; repeat to go on down the list | `task schedule next thu-ds` |
| `task schedule open <slot> [HH:MM]` | Lets a fixed slot take a task this week, for a cancelled class | `task schedule open sun-db 10:00` |
| `task schedule swap <slot> <slot>` | Swaps two slots' times, this week | `task schedule swap sat-ds mon-arch` |
| `task schedule move <slot> <day> <HH:MM>` | Moves a slot to another day or time, this week | `task schedule move sat-ds doshanbe 14:00` |
| `task schedule skip <slot>` | Skips a slot's own work, this week | `task schedule skip sat-research` |
| `task schedule assign <slot> <task \| filter>` | Pins a task, or a filter, to a slot, this week | `task schedule assign wed-floating 12` |
| `task schedule reset <slot \| week>` | Undoes this week's changes to a slot, or to the week | `task schedule reset sat-ds` |
| `task schedule borrow <task> <hours> [force]` | Gives a task more time this week, from the slots the template allows; `force` may go below floors | `task schedule borrow 12 2` |
| `task schedule left <task> <hours>` | Corrects a wrong estimate: the hours the task still needs | `task schedule left 12 3` |
| `task schedule review [last]` | Weekly review: planned and tracked hours, floors, the buffer, deadlines | `task schedule review last` |
| `task schedule mode start <name> <values...>` | Starts a mode of the template, such as exam season | `task schedule mode start exam os 1405-09-20` |
| `task schedule mode stop <m<N> \| name>` | Stops a mode early | `task schedule mode stop m1` |
| `task schedule mode list` | Lists the modes started, and when they end | `task schedule mode list` |
| `task schedule event add <title> <date> <HH:MM-HH:MM>` | A one-time appointment | `task schedule event add Dentist 1405-08-03 17:00-18:00` |
| `task schedule event delete <id>` | Deletes an event | `task schedule event delete e1` |
| `task schedule event list` | Lists the events | `task schedule event list` |
| `task schedule undo` | Undoes the last change to the schedule | `task schedule undo` |
| `task schedule log` | Lists the changes to the schedule, most recent first | `task schedule log` |
| `task schedule init [example \| blank]` | Creates the template | `task schedule init` |
| `task schedule check [--expand]` | Finds mistakes in the template; `--expand` lists each day's slots as used | `task schedule check --expand` |
| `task schedule help [<command> \| template]` | Built-in help | `task schedule help peek` |
| `task tasks` | Your usual task list (your `default.command`), without the schedule | `task tasks` |

Changes apply to one week, and `task schedule undo` takes them back. Note that `task undo` does not undo schedule changes (but does undo `left`, which changes a task), and that the schedule is kept in its own files, so it does not sync with your tasks.

### The template

`~/.task/schedule` is a text file of sections:

| Section | What it holds |
|---|---|
| `[settings]` | The settings below |
| `[categories]` | One per line: `<name> <floor> <filter> [options]`, such as `os  4  project:os`; the floor is hours a week, or `-` |
| `[shanbe]` to `[jome]` (or `[saturday]` to `[friday]`) | The slots of each day, one per line: `<start>-<end> <id> <kind> <category> [options]` |
| `[everyday]` | Slots of every day, such as lunch; a day's own slot at that time replaces them on that day |
| `[mode <name>]` | A temporary change, such as exam season: `params`, `active`, `give` and `floor` lines |

Template settings, in `[settings]`:

| Setting | Meaning |
|---|---|
| `fallback` | The fallback chain: categories tried, in order, by a slot whose own categories have nothing; `rest` ends it |
| `borrow` | The slots or categories that lend time to `task schedule borrow`, in order |
| `estimate.factor` | Estimates are planned times this factor (exact estimates never are) |
| `unestimated.per-day` | How many slots a day a task with no estimate may fill |
| `pacing` | `on` (default) or `off`: spread tasks with estimates and due dates over the time they have |
| `max.per-day` | The most time one task gets in a day, such as `3h`, except when it is critical |
| `overdue.critical-days` | For how many days an overdue task stays critical (7) |
| `critical.break-floors` | Priorities, such as `H`, of critical tasks that may take borrow-order slots below floors; none unless set (the example sets `H`) |

Slots: the kind is `fixed` (a class, never takes tasks), `study`, `floating`, `buffer` or `rest`. The category can be a list in order of priority, such as `ds,algo`, tried before the fallback chain. Slot options:

| Option | Meaning |
|---|---|
| `filter:"..."` | Pulls tasks with this filter instead of the category's |
| `fallback:"..."` | The slot's own fallback chain, instead of the `fallback` setting |
| `optional` | Dimmed, and counts as planned time only when it gets a task |
| `protected` | Never lends its time, and never moves |
| `except:jome` | In `[everyday]`: leaves out these days |

Category options, at the end of the line: `accounting:off` keeps it out of the hours, and `estimate:4h` is the estimate of its tasks that have none. For a catch-all category of free slots, use a virtual tag in capitals: `free  -  +PENDING`.

### Estimates and tracking

A task can have one of two estimates:

- `estimate:4h`, padded by `estimate.factor`: shown as `4h (6h)` with a factor of 1.5;
- `exact:1.5h`, planned as it is, such as a lecture video: shown as `1.5h exact`. If a task has both, the exact one is used.

`task add` asks for an estimate when a task comes into one of your categories without one (`3h`, or `=3h` for an exact one). **Track your work with `task start` and `task stop`**: the time tracked is what the schedule counts as done, and the rest is planned again. When a task needs more or less than its estimate said, `task schedule left 12 3` sets it so that 3 hours are left.

With a due date as well, the work is [paced](../docs/schedule.md#pacing): spread over the weeks until then, with a `Pace` column (`on track`, `behind 2h`, `short 3h`, `overdue`) and a warning when the work due by a date no longer fits in the slots before it.

### Settings

In `.taskrc`, set with `task config`:

| Setting | Default | Meaning |
|---|---|---|
| `schedule` | `on` | Turns the schedule on or off |
| `schedule.default` | `on` | Plain `task` shows today's schedule, when a template exists |
| `schedule.template` | | The template file; empty for `schedule` in the data directory |
| `schedule.wake`, `schedule.sleep` | `06:00`, `24:00` | The day's start and end, `wake` and `sleep` in templates |
| `schedule.report` | `important` | The report below today's schedule, or `none` |
| `schedule.layout` | `auto` | Tables: `full`, `compact` for narrow terminals, or `auto` by width |
| `schedule.estimate.uda` | `estimate` | The UDA holding estimates |
| `schedule.estimate.exact.uda` | `exact` | The UDA holding exact estimates |
| `schedule.estimate.ask` | `on` | Ask for an estimate in `task add` and `task modify` |
| `dateformat.schedule` | `A d B Y` | The date format of the schedule's headers |
| `color.schedule.header`, `.now`, `.past`, `.optional` | | Colors of day headers, the current slot, past slots, and empty or optional slots |
| `weekstart` | `saturday` | The first day of the week |

The full reference, with the template format, the rules of planning, and a worked example, is in [docs/schedule.md](../docs/schedule.md). To keep plain `task` as it was, run `task config schedule.default off`.

## Calendar settings

Three ordinary Taskwarrior settings, changed with `task config` and shown by `task show`, control dates everywhere: reports, `task info`, `task calendar`, the schedule, and what you type.

| Setting | Values | Default |
|---|---|---|
| `calendar` | `jalali`, or `gregorian`, which behaves exactly as upstream Taskwarrior | `jalali` |
| `dayname.style` | `finglish` (Shanbe, Yekshanbe, ...) or `english` (Saturday, Sunday, ...) | `finglish` |
| `weekstart` | any day | `saturday` |

They are independent: `task config calendar gregorian` with Finglish day names works, and so does Jalali with English ones. Day names are accepted in both styles when typing, whatever `dayname.style` says. `task config calendar gregorian` does not change `weekstart`, but reminds you if it is still `saturday`.

With `calendar=gregorian`, everything is as in upstream Taskwarrior: Gregorian dates and month names, `sow` and `soww` starting on Monday, Saturday and Sunday as the weekend in `task calendar`, and `recur:weekdays` meaning Monday to Friday. With `calendar=jalali`, weeks follow `weekstart`, the weekend is Thursday and Friday, and `recur:weekdays` means Saturday to Wednesday.

### Numeric dates

| Input | `calendar=jalali` | `calendar=gregorian` |
|---|---|---|
| `1405-07-26` | 26 Mehr 1405 (a Jalali date) | refused, with a hint to set `calendar=jalali` |
| `2026-10-18` | 18 October 2026, shown as 26 Mehr 1405 (any year from 1700 is Gregorian) | 18 October 2026 |
| `1800-01-01` | refused: a year between the calendars, with a hint | refused, as upstream: dates start in 1980 |
| `05-07-26` (with a `y` dateformat) | 1405 | 2005 (two-digit years are 14xx or 20xx) |

So with `calendar=jalali` both kinds of date work, since a Jalali year (1359 to 1699) and a Gregorian one (1980 on) never overlap; anything else is refused with an explanation, never guessed.

### Changing the calendar is safe

Tasks are stored as points in time, so switching `calendar` or `dayname.style` changes only how dates are shown and typed, never your data: `task export` gives the same result either way.

Recurring tasks need one more thing, since "every month" means different dates in the two calendars. A recurring task remembers the calendar it was created under, in an attribute called `rcalendar` (shown by `task info` as "Recurrence calendar"), and keeps making its instances by that calendar's months and weekdays, whatever the setting says later.

- Recurring tasks from earlier versions have no `rcalendar`. They are given `jalali`, the only calendar there was when they were made.
- `task <id> modify rcalendar:gregorian`, on the recurring task itself (see `task recurring`), changes it for the instances made from then on.
- Unmodified Taskwarrior and other clients keep `rcalendar` as an unknown attribute, without harm, also when the task is edited there. They make instances by Gregorian months, though, so if you sync with them, let only PersianTask make instances: `task config recurrence off` on the others, as upstream advises for any sync setup.

[docs/calendar.md](../docs/calendar.md) has the details.

## Notes

- **Your data is compatible.** Tasks are stored exactly as in Taskwarrior 3, so existing data works unchanged, and is simply displayed in Jalali.
- **Machine formats stay Gregorian.** `export`, `import`, hooks and holiday files keep using Gregorian dates, for compatibility with other tools, whatever the calendar.
- **Date range.** As in Taskwarrior, dates before 1359 (1980) are not accepted.

## What changed from Taskwarrior

- `src/libshared/src/Datetime.cpp`: Jalali conversion (Borkowski's algorithm, which matches the official Iranian calendar), parsing and display, and the `calendar` and `dayname.style` settings. Until configured, the library behaves exactly as upstream.
- `src/commands/CmdCalendar.cpp`, `src/recur.cpp`, `src/Task.cpp`, `src/Context.cpp`, `src/commands/CmdConfig.cpp`, `src/commands/CmdShow.cpp`, `src/columns/ColTypeDate.cpp`: the calendar view, recurrence, the settings, and weeks starting on Saturday.
- `src/schedule/`: the weekly schedule, and `examples/schedule/student-week`, the example template.
- `src/CLI2.cpp`: a hook so that plain `task` can show the schedule.
- `src/commands/CmdAdd.cpp`, `src/commands/CmdModify.cpp`: one line each, so that adding a task to a schedule category asks for its estimate.
- `persiantask/`: the tests, and the scripts that build packages and merge upstream updates.
- `.github/workflows/persiantask-*.yml`: runs the tests, and merges upstream every week.
- `pkg/arch/PKGBUILD`: the Arch Linux package, and `pkg/arch/persiantask.asc`, the key that signs the prebuilt packages.

To run the tests: `bash persiantask/build-and-test.sh`. With `PERSIANTASK_FAKETIME_TESTS=1`, they run again with the clock a year ahead (this needs libfaketime), and `persiantask/faketime-tests.sh` runs them at any dates and times, or, by default, at a list of risky ones: around midnight, Nowruz, and the end of a Jalali leap year. With `PERSIANTASK_UPSTREAM_TESTS=1`, it also runs upstream Taskwarrior's own test suite with `calendar=gregorian`, which takes several more minutes; `persiantask/upstream-known-failures` lists the upstream tests that fail, and why.

## License

PersianTask is released under the [MIT license](../LICENSE), like Taskwarrior, which is developed by [Gothenburg Bit Factory](https://github.com/GothenburgBitFactory/taskwarrior). The original Taskwarrior README is in [README.md](../README.md).
