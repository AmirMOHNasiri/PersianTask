# Calendars

PersianTask shows and reads dates in the Jalali calendar by default, and can
behave exactly as upstream Taskwarrior instead.  This page is the reference;
the [README](../.github/README.md#calendar-settings) has a summary.

## Settings

| Setting | Values | Default |
|---|---|---|
| `calendar` | `jalali`, or `gregorian`, which behaves exactly as upstream Taskwarrior | `jalali` |
| `dayname.style` | `finglish` (Shanbe, Yekshanbe, ...) or `english` (Saturday, Sunday, ...) | `finglish` |
| `weekstart` | any day | `saturday` |

They are ordinary settings: `task config calendar gregorian` changes one,
`task show` lists them, and `rc.calendar:gregorian` on the command line
applies to one command.  They are independent of each other.

`task config calendar gregorian` does not change `weekstart`.  If it is still
`saturday`, it says so, since upstream Taskwarrior's default is Monday:

```
$ task config calendar gregorian
Config file /home/you/.taskrc modified.
weekstart is still saturday; set it with 'task config weekstart monday' if you want.
```

## Numeric dates

| Input | `calendar=jalali` | `calendar=gregorian` |
|---|---|---|
| `1405-07-26` | 26 Mehr 1405 (a Jalali date) | refused, with a hint to set `calendar=jalali` |
| `2026-10-18` | 18 October 2026, shown as 26 Mehr 1405 (any year from 1700 is Gregorian) | 18 October 2026 |
| `1800-01-01` | refused: a year between the calendars, with a hint | refused, as upstream: dates start in 1980 |
| `05-07-26` (with a `y` dateformat) | 1405 | 2005 (two-digit years are 14xx or 20xx) |

A Jalali year (1359 to 1699) and a Gregorian one (1980 on) never overlap, so
nothing is guessed: input is either read one way or refused with an
explanation.  `export`, `import`, hooks and holiday files always use
Gregorian dates, whatever the setting.

## Recurring tasks and `rcalendar`

Tasks are stored as points in time, so changing `calendar` or `dayname.style`
changes only how dates are shown and typed, never your data.

Recurring tasks are the exception that needs care: "every month" means
different dates in the two calendars.  A task due on 1 Mehr (23 September)
repeats on 1 Aban (23 October) and 1 Azar (22 November) with Jalali months,
but on 23 October and 23 November with Gregorian ones.  The same goes for
`quarterly`, `yearly`, `2m` and the like, and for `weekdays`, which means
Saturday to Wednesday in the Jalali calendar and Monday to Friday in the
Gregorian one.

So a recurring task remembers the calendar it was created under, in an
attribute called `rcalendar`, and always makes its instances by that
calendar, whatever the setting says later:

```
$ task add Rent due:1405-07-01 recur:monthly
$ task recurring
$ task <id> info
...
Recurrence calendar  jalali
```

- **New recurring tasks** get the calendar in use when they are created, with
  `task add` or `task import`.  Only the recurring task (the template, with
  status `recurring`) has it; its instances do not.
- **Recurring tasks from earlier versions** of PersianTask have no
  `rcalendar`.  They were made when the Jalali calendar was the only one, so
  they are given `jalali` the next time their instances are made, whatever
  `calendar` says.  The same applies to a recurring task made by another
  Taskwarrior client, which does not know about `rcalendar`.
- **To change it**, modify the recurring task itself:
  `task <id> modify rcalendar:gregorian`, with the id that `task recurring`
  shows.  Instances made from then on follow the new calendar; existing ones
  keep their dates.  Removing it (`rcalendar:`) makes the task count as an
  old one, so it is given `jalali` again.

`rcalendar` is a UDA that PersianTask defines by default (`uda.rcalendar.type`,
`uda.rcalendar.label` and `uda.rcalendar.values`, in `task show`), so
`task info`, `task udas` and `task edit` treat it like any other attribute,
and only `jalali` and `gregorian` are accepted.

### Other Taskwarrior clients

`rcalendar` is stored like any other attribute, so it syncs and exports with
the task.

- **Unmodified Taskwarrior** keeps it as an unknown attribute: `task info` and
  `task udas` list it as a "UDA orphan", and it does no harm.  Editing the
  task there, its description, due date or anything else, keeps `rcalendar`
  as it is.
- **Other clients** that read `task export` keep it as well, as long as they
  keep attributes they do not know.  One that drops it turns the task into an
  "old" one: PersianTask then gives it `jalali` again.
- **Instances** are made by whichever client runs a report.  Upstream
  Taskwarrior makes them by Gregorian months and weekdays, whatever
  `rcalendar` says.  If you sync the same tasks between PersianTask and
  upstream Taskwarrior, let only one of them make instances, as upstream
  advises for any sync setup (see `man task-sync`): run
  `task config recurrence off` on all the others, and keep it on in
  PersianTask.
