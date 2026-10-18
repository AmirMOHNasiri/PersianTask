# PersianTask

**Taskwarrior with the Jalali (Persian / Shamsi) calendar, and a weekly schedule**

PersianTask is a version of [Taskwarrior](https://taskwarrior.org) in which all dates, both entered and displayed, use the Jalali calendar, the official calendar of Iran. It also plans your week: a [weekly schedule](#weekly-schedule) of time slots, each of which pulls the right task to work on. Everything else works exactly like Taskwarrior. Upstream Taskwarrior updates are merged every week, once they pass the Jalali calendar tests.

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

A weekly review and temporary modes such as exam season are coming in the next versions.

The full reference, with the template format and a worked example, is in [docs/schedule.md](../docs/schedule.md). To keep plain `task` as it was, run `task config schedule.default off`.

## Notes

- **Your data is compatible.** Tasks are stored exactly as in Taskwarrior 3, so existing data works unchanged, and is simply displayed in Jalali.
- **Machine formats stay Gregorian.** `export`, `import`, hooks and holiday files keep using Gregorian dates, for compatibility with other tools. Any year of 1700 or more is read as Gregorian, so `due:2026-10-16` works as well.
- **Date range.** As in Taskwarrior, dates before 1359 (1980) are not accepted.
- **Weeks start on Saturday**, and Thursday and Friday are the weekend. The start of the week can be changed with `weekstart`.

## What changed from Taskwarrior

- `src/libshared/src/Datetime.cpp`: Jalali conversion (Borkowski's algorithm, which matches the official Iranian calendar), parsing and display.
- `src/commands/CmdCalendar.cpp`, `src/recur.cpp`, `src/Context.cpp`: the calendar view, recurrence, and weeks starting on Saturday.
- `src/schedule/`: the weekly schedule, and `examples/schedule/student-week`, the example template.
- `src/CLI2.cpp`: a hook so that plain `task` can show the schedule.
- `persiantask/`: the tests, and the scripts that build packages and merge upstream updates.
- `.github/workflows/persiantask-*.yml`: runs the tests, and merges upstream every week.
- `pkg/arch/PKGBUILD`: the Arch Linux package, and `pkg/arch/persiantask.asc`, the key that signs the prebuilt packages.

To run the tests: `bash persiantask/build-and-test.sh`

## License

PersianTask is released under the [MIT license](../LICENSE), like Taskwarrior, which is developed by [Gothenburg Bit Factory](https://github.com/GothenburgBitFactory/taskwarrior). The original Taskwarrior README is in [README.md](../README.md).
