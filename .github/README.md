# PersianTask

**Taskwarrior with the Jalali (Persian / Shamsi) calendar**

PersianTask is a version of [Taskwarrior](https://taskwarrior.org) in which all dates, both entered and displayed, use the Jalali calendar, the official calendar of Iran. Everything else works exactly like Taskwarrior. Upstream Taskwarrior updates are merged every week, once they pass the Jalali calendar tests.

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

```
mkdir persiantask && cd persiantask
curl -O https://raw.githubusercontent.com/AmirMOHNasiri/PersianTask/main/pkg/arch/PKGBUILD
makepkg -si
```

The package replaces the official `task` package, if it is installed. The build runs the Jalali calendar tests, and stops if any fails. To update, run `makepkg -si` again in the same directory.

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

The display format is set with `dateformat`. For example, `task config dateformat 'A d B Y'` shows dates as `Panjshanbe 30 Mehr 1405`.

## Notes

- **Your data is compatible.** Tasks are stored exactly as in Taskwarrior 3, so existing data works unchanged, and is simply displayed in Jalali.
- **Machine formats stay Gregorian.** `export`, `import`, hooks and holiday files keep using Gregorian dates, for compatibility with other tools. Any year of 1700 or more is read as Gregorian, so `due:2026-10-16` works as well.
- **Date range.** As in Taskwarrior, dates before 1359 (1980) are not accepted.
- **Weeks start on Saturday**, and Thursday and Friday are the weekend. The start of the week can be changed with `weekstart`.

## What changed from Taskwarrior

- `src/libshared/src/Datetime.cpp`: Jalali conversion (Borkowski's algorithm, which matches the official Iranian calendar), parsing and display.
- `src/commands/CmdCalendar.cpp`, `src/recur.cpp`, `src/Context.cpp`: the calendar view, recurrence, and weeks starting on Saturday.
- `jalali/`: the Jalali calendar tests, and the script that merges upstream updates.
- `.github/workflows/jalali-*.yml`: runs the tests, and merges upstream every week.
- `pkg/arch/PKGBUILD`: the Arch Linux package.

To run the tests: `bash jalali/build-and-test.sh`

## License

PersianTask is released under the [MIT license](../LICENSE), like Taskwarrior, which is developed by [Gothenburg Bit Factory](https://github.com/GothenburgBitFactory/taskwarrior). The original Taskwarrior README is in [README.md](../README.md).
