# The weekly schedule

PersianTask can plan your week. You describe a typical week once, as a template of time slots: classes, study blocks, research time. Every slot then pulls a pending task to work on, so that `task` tells you, for each part of today, what to do.

- [Quick start](#quick-start)
- [How a slot gets its task](#how-a-slot-gets-its-task)
- [Commands](#commands)
- [The template file](#the-template-file)
- [Settings](#settings)
- [A worked example](#a-worked-example)

## Quick start

```
task schedule init        # Creates a template from the example
task schedule check       # Finds mistakes after you edit it
task schedule week        # Shows the week, with a task in each slot
task                      # Shows today's slots, and the important tasks
```

`task schedule init` copies the example template, a university student's week, to `~/.task/schedule`. Edit it to describe your own week: which categories of work you have (each one a Taskwarrior filter, such as `project:os`), and when each slot is. Then `task schedule check` reports any mistakes, with line numbers.

`task schedule help` shows a quick reference, and `task schedule help <command>` explains one command.

## How a slot gets its task

Each slot has a *kind*:

| Kind | Meaning |
|---|---|
| `fixed` | Something you attend, such as a class. It never gets a task, and can never be moved. |
| `study` | Time to work on its category. |
| `floating` | Like study, but meant to be given away first when time is needed elsewhere. |
| `buffer` | Spare time. With no category of its own, it goes straight to the fallback chain. |
| `rest` | Always free. |

Slots get their tasks in two passes over the week, both in time order:

1. Every slot takes the most urgent pending task matching its category (or its own `filter:`). The earliest slot gets the most urgent task.
2. Every slot still empty goes down its fallback chain: the slot's own `fallback:` if it has one, or else the `fallback` setting. Each step is a category, and the slot takes that category's most urgent task. The step `rest` leaves the slot free, and so does reaching the end of the chain.

The first pass comes first so that one slot's fallback never takes the task that another slot's own category was meant for.

Only pending tasks are scheduled: not waiting tasks, and not tasks blocked by a dependency. Urgency is Taskwarrior's usual urgency, so priorities (`priority:H`), due dates, tags and the rest all count. A task fills at most `unestimated.per-day` slots a day (one, by default), so that a single task does not take over the whole day.

The schedule ignores the active Taskwarrior context (`task context`): slots always pull from all your tasks.

Viewing the schedule never changes anything: `task schedule`, `task schedule week`, `task schedule check` and plain `task` only read your tasks and the template.

## Commands

### task schedule today

```
task schedule
task schedule today
```

Today's slots in time order, with the task each one gets. The current slot is highlighted, and past slots are dimmed. The `From` column says where the task came from: the slot's category, or a fallback category, marked `(fallback)`.

Below the slots comes the report named by `schedule.report`, by default `important`: tasks due today or overdue, and tasks with high priority (`priority:H`), however far away their due date is.

**Plain `task`** shows the same, when a template exists and `schedule.default` is on. With no template, or with any filter or command, `task` behaves exactly as in Taskwarrior. `task tasks` always shows the normal task list (the `next` report).

### task schedule week

```
task schedule week
task schedule week next
task schedule week prev
task schedule week 1405-08-10
```

A whole week of slots and tasks: this week, the next or previous one, or the week containing a date. Weeks start on the `weekstart` setting, Shanbe by default, and the header gives the week number and its first and last days.

Each slot is numbered within the week. The number can be used in place of the slot's id.

### task schedule init

```
task schedule init
task schedule init blank
```

Creates the template file, either from the example of a university student's week or as an almost empty file. An existing template is never overwritten.

### task schedule check

```
task schedule check
```

Reads the template and reports every mistake with its line number, for example:

```
/home/you/.task/schedule:15: Slot 'sat-d': 'oss' is not a category. (did you mean 'os'?)
/home/you/.task/schedule:12: Slot 'sat-b' overlaps slot 'sat-a' on line 11.
```

It checks the syntax, unknown names, overlapping slots, times outside the day, and filters that Taskwarrior cannot read. While the template has mistakes, the other commands do not show it, and point to `task schedule check` instead.

### task schedule help

```
task schedule help
task schedule help week
task schedule help template
```

A quick reference, help on one command, or a summary of the template format.

## The template file

The template is a text file, at `~/.task/schedule` unless the `schedule.template` setting says otherwise. It is made of sections, with a name in square brackets. `#` starts a comment, except inside double quotes. Blank lines are ignored.

### [settings]

```
[settings]
estimate.factor     = 1.5
unestimated.per-day = 1
fallback            = soon term ds arch rest
borrow              = fri-buffer arch wed-floating uptodate research
```

| Setting | Meaning |
|---|---|
| `fallback` | The global fallback chain: category names, tried in order. `rest` (which must come last) leaves the slot free. |
| `unestimated.per-day` | How many slots a day a task may fill. The default is 1. |
| `estimate.factor` | Planned time for a task is its estimate times this factor. *Checked now; used once estimates are supported.* |
| `borrow` | Slots or categories that lend time to a task that needs more than its slot, in order. *Checked now; used once borrowing is supported.* |

### [categories]

```
[categories]
# name      floor   filter
os          4       project:os
soon        -       due.before:now+7d
term        -       +term
```

One category per line: a name, a floor, and a filter.

- The **name** is used by slots, fallback chains, borrowing and modes. It uses letters, digits, `-` and `_`.
- The **floor** is the minimum number of hours a week the category should get, or `-` for none. *Checked now; used by the weekly review once it is supported.*
- The **filter** is the rest of the line, written exactly as on the Taskwarrior command line: `project:os`, `+term`, `project:os +homework`, `due.before:now+7d`, and so on.

### Days: [shanbe] to [jome]

```
[shanbe]
# time         id             kind      category  options
07:30-10:30    sat-research   study     research
14:00-16:00    sat-os-hw      study     os        filter:"project:os +homework"
21:00-21:45    sat-uptodate   study     uptodate  optional

[jome]
10:30-12:00    fri-buffer     buffer    -
12:00-sleep    fri-rest       rest      -         protected
```

A section for each day, named in Finglish: `[shanbe]`, `[yekshanbe]`, `[doshanbe]`, `[seshanbe]`, `[chaharshanbe]`, `[panjshanbe]`, `[jome]`. Each line is one slot, made of:

1. **Time**: `HH:MM-HH:MM`. `wake` and `sleep` stand for the `schedule.wake` and `schedule.sleep` settings, as in `12:00-sleep`. Slots must fit between them, and must not overlap on the same day.
2. **Id**: a unique name, such as `sat-research`, used to refer to the slot. It uses letters, digits, `-` and `_`, and is not only digits.
3. **Kind**: `fixed`, `study`, `floating`, `buffer` or `rest` (see [above](#how-a-slot-gets-its-task)).
4. **Category**: a category name, or `-` for none. Study and floating slots need a category or a `filter:` option.
5. **Options**, any of:

| Option | Meaning |
|---|---|
| `filter:"..."` | Pull tasks with this filter instead of the category's. The category is still the slot's category, for planning. |
| `fallback:"..."` | This slot's own fallback chain, replacing the global one. |
| `optional` | Shown dimmed, and counted as planned time only when it gets a task. |
| `protected` | Never lends its time to another task. *Checked now; used once borrowing is supported.* |

Fixed and rest slots never take tasks, so they cannot have `filter:` or `fallback:`.

### [mode name]

*Modes are checked now, and will be applied in a later version.*

A mode is a temporary change to the week, such as exam season, which starts when you ask and ends on its own.

```
[mode exam]
params = course date
active = 7d before $date
give   = arch -> $course
give   = wed-floating -> $course
give   = research 50% -> $course
floor  = arch 0

[mode project]
params = end
active = until $end
give   = arch except 1 -> term
```

| Line | Meaning |
|---|---|
| `params` | Names for the values given when starting the mode, used as `$name`. |
| `active` | When the mode applies: `<N>d before $param` (the N days before a date) or `until $param`. |
| `give` | `<slots> [N% \| except N] -> <category>`: those slots work on another category. `<slots>` is a category name (all its slots), a slot id, or `kind:<kind>`. `50%` gives half of them, and `except 1` all but one. |
| `floor` | `<category> <hours>`: a different floor while the mode is active. |

## Settings

These go in `.taskrc`, and can be set with `task config`:

| Setting | Default | Meaning |
|---|---|---|
| `schedule` | `on` | Turns the schedule on or off. |
| `schedule.default` | `on` | Plain `task` shows today's schedule, when a template exists. |
| `schedule.template` | | The template file. Empty means `schedule` in the data directory. |
| `schedule.wake` | `06:00` | The start of the day, `wake` in templates. |
| `schedule.sleep` | `24:00` | The end of the day, `sleep` in templates. |
| `schedule.report` | `important` | The report below today's schedule, or `none`. |
| `dateformat.schedule` | `A d B Y` | The date format of the schedule's headers. |
| `color.schedule.header` | `bold` | Color of the day headers. |
| `color.schedule.now` | `bold green` | Color of the current slot. |
| `color.schedule.past` | `gray12` | Color of past slots. |
| `color.schedule.optional` | `gray12` | Color of optional slots with no task. |
| `weekstart` | `saturday` | The first day of the week. |

The template is a plain file: it is not part of the Taskwarrior database, so `task sync` does not copy it to your other devices. Copy it yourself, or keep it in a dotfiles repository.

## A worked example

Start from the example:

```
$ task schedule init
Created /home/you/.task/schedule, from the example of a university student's week.
```

Its categories pull tasks by project: `os` is `project:os`, `ds` is `project:ds`, and so on. Add a few tasks:

```
$ task add OS homework 3 project:os +homework due:1405-07-29
$ task add Paper draft project:research priority:H
$ task add Tree exercises project:ds
$ task add Term project +term due:1405-09-10
```

Then look at Shanbe:

```
$ task schedule week
Week 31: Shanbe 25 Mehr 1405 - Jome 1 Aban 1405

Day              #  Time         Slot           ID  Task            From
Shanbe 25 Mehr   1  07:00-07:30  sat-review      4  Term project    term (fallback)
                 2  07:30-10:30  sat-research    2  Paper draft     research
                 3  11:00-12:30  sat-ds          3  Tree exercises  ds
                 4  14:00-16:00  sat-os-hw       1  OS homework 3   os
                 5  21:00-21:45  sat-uptodate                       rest
```

- `sat-research`, `sat-ds` and `sat-os-hw` each take their own category's task. `sat-os-hw` only takes OS tasks tagged `+homework`, because of its `filter:` option.
- `sat-review` has no review task to do, so it goes down the fallback chain, `soon term ds arch rest`. The OS homework is due soon, but it already has its slot today, so the next step, `term`, gives the term project.
- `sat-uptodate` has no up-to-date task either, and its fallback chain has nothing left for today (the OS homework, the term project and the tree exercises already have their slots, and there is no arch task), so it reaches `rest` and stays free. Being optional, it is dimmed, and does not count as planned time.

On Yekshanbe, the classes (`sun-db-class`, `sun-net-class`, `sun-os-class`) are fixed: they appear in the schedule, but never get a task.
