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

4. Check it, and look at your week:

   ```
   task schedule check
   task schedule week
   ```

```
$ task schedule week
Week 31: Shanbe 25 Mehr 1405 - Jome 1 Aban 1405

Day              #  Time         Slot           ID  Task            Estimate  From
Shanbe 25 Mehr   1  07:00-07:30  sat-review      4  Term project              term (fallback)
                 2  07:30-10:30  sat-research    2  Paper draft     2h (3h)   research
                 3  11:00-12:30  sat-ds          3  Tree exercises            ds
                 4  14:00-16:00  sat-os-hw       1  OS homework 3             os
```

### Commands

| Command | Meaning |
|---|---|
| `task schedule today` (or plain `task`) | Today's slots, their tasks, and the important tasks |
| `task schedule week [next \| prev \| <date>]` | A week of slots and tasks |
| `task schedule init [example \| blank]` | Creates the template |
| `task schedule check` | Finds mistakes in the template, with line numbers |
| `task schedule swap <slot> <slot>` | Swaps two slots' times, this week |
| `task schedule move <slot> <day> <HH:MM>` | Moves a slot, this week |
| `task schedule skip <slot>` | Skips a slot's own work, this week |
| `task schedule assign <slot> <task \| filter>` | Pins work to a slot, this week |
| `task schedule reset <slot \| week>` | Undoes this week's changes |
| `task schedule borrow <task> <hours>` | Gives a task more time this week, from the slots the template allows |
| `task schedule review [last]` | Weekly review: planned and tracked hours, floors, the buffer, deadlines |
| `task schedule mode start <name> ...` | A mode, such as exam season, that changes the week for a while |
| `task schedule event add <title> <date> <HH:MM-HH:MM>` | A one-time appointment |
| `task schedule undo` / `task schedule log` | Undoes schedule changes, or lists them |
| `task schedule help [<command> \| template]` | Built-in help |
| `task tasks` | Your usual task list (your `default.command`), without the schedule |

For example, to study data structures on Doshanbe evening this week instead of Shanbe morning:

```
task schedule swap sat-ds mon-arch
```

Changes apply to one week, and `task schedule undo` takes them back. Note that `task undo` does not undo schedule changes, and that the schedule is kept in its own files, so it does not sync with your tasks.

Give tasks an estimate, such as `task add Paper project:research estimate:4h`, and the schedule plans enough slots for it, with a safety factor (1.5 in the example); time tracked with `task start` and `task stop` is taken off. When a task needs more, `task schedule borrow 12 2` takes two more hours from the slots the template lets it borrow, never below a category's weekly floor.

Once a week, `task schedule review` compares planned and tracked hours per category, warns about categories below their weekly floor two weeks running and about a buffer that keeps filling up, and lists the deadlines of this week and the next.

For exam season, `task schedule mode start exam os 1405-09-20` gives the week before the exam more OS time, as the template's `exam` mode says, and ends on its own after the exam.

The full reference, with the template format and a worked example, is in [docs/schedule.md](../docs/schedule.md). To keep plain `task` as it was, run `task config schedule.default off`.

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
- `persiantask/`: the tests, and the scripts that build packages and merge upstream updates.
- `.github/workflows/persiantask-*.yml`: runs the tests, and merges upstream every week.
- `pkg/arch/PKGBUILD`: the Arch Linux package, and `pkg/arch/persiantask.asc`, the key that signs the prebuilt packages.

To run the tests: `bash persiantask/build-and-test.sh`. With `PERSIANTASK_UPSTREAM_TESTS=1`, it also runs upstream Taskwarrior's own test suite with `calendar=gregorian`, which takes several more minutes; `persiantask/upstream-known-failures` lists the upstream tests that fail, and why.

## License

PersianTask is released under the [MIT license](../LICENSE), like Taskwarrior, which is developed by [Gothenburg Bit Factory](https://github.com/GothenburgBitFactory/taskwarrior). The original Taskwarrior README is in [README.md](../README.md).
