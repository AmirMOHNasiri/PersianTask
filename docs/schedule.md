# The weekly schedule

PersianTask can plan your week. You describe a typical week once, as a template of time slots: classes, study blocks, research time. Every slot then pulls a pending task to work on, so that `task` tells you, for each part of today, what to do.

- [Quick start](#quick-start)
- [How a slot gets its task](#how-a-slot-gets-its-task)
- [Commands](#commands)
- [Estimates](#estimates)
- [Borrowing time](#borrowing-time)
- [The weekly review](#the-weekly-review)
- [Changing one week](#changing-one-week)
- [Where the schedule is stored](#where-the-schedule-is-stored)
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

Only pending tasks are scheduled: not waiting tasks, and not tasks blocked by a dependency. Urgency is Taskwarrior's usual urgency, so priorities (`priority:H`), due dates, tags and the rest all count. A task with an [estimate](#estimates) fills slots until its estimated time is planned. Any other task fills at most `unestimated.per-day` slots a day (one, by default), so that a single task does not take over the whole day.

The schedule ignores the active Taskwarrior context (`task context`): slots always pull from all your tasks.

Under the week, `Planned:` gives the hours of work planned for each category: the length of every slot that got a task, after this week's changes (so a swapped slot counts with its new length). A slot counts for the category its task came from: the slot's own category, the fallback step that found the task, or, for an assigned or borrowed task, the first category whose filter the task matches. Fixed slots, events and free slots do not count.

### Relative dates in filters

Filters and urgency are evaluated as of the week being planned. In the current week, `now` (and `today`, `eow`, `now+7d`, and so on) means the current time. In any other week, it means the start of that week. So with the category

```
soon   -   due.after:now due.before:now+7d
```

`task schedule week` fills slots with tasks due in the coming seven days, and `task schedule week next` with tasks due in the seven days after next Shanbe. Urgency, which ranks the tasks, is computed as of the same time.

Viewing the schedule never changes anything: `task schedule`, `task schedule week`, `task schedule check` and plain `task` only read your tasks, the template and the week's changes.

## Commands

### task schedule today

```
task schedule
task schedule today
```

Today's slots in time order, with the task each one gets. The current slot is highlighted, and past slots are dimmed. The `From` column says where the task came from: the slot's category, or a fallback category, marked `(fallback)`.

Below the slots comes the report named by `schedule.report`, by default `important`: tasks due today or overdue, and tasks with high priority (`priority:H`), however far away their due date is.

**Plain `task`** shows the same, when a template exists and `schedule.default` is on. With no template, or with any filter or command, `task` behaves exactly as in Taskwarrior. If the template has mistakes, plain `task` says so and shows the report, but still succeeds, so that shell greetings and scripts calling `task` never fail because of the schedule.

`task tasks` always shows your usual task list: it runs your `default.command` (the `next` report, unless you changed it). To make it run something else, define `alias.tasks` yourself.

### task schedule week

```
task schedule week
task schedule week next
task schedule week prev
task schedule week 1405-08-10
```

A whole week of slots and tasks: this week, the next or previous one, or the week containing a date. Weeks start on the `weekstart` setting, Shanbe by default, and the header gives the week number and its first and last days.

Each slot is numbered within the week. The number can be used in place of the slot's id.

### task schedule swap

```
task schedule swap sat-ds mon-arch
task schedule swap 3 16
task schedule swap sat-ds mon-arch next
```

Swaps the times of two slots, for one week: each slot takes the other's day, start and end. Since they exchange places, a swap never creates an overlap. Fixed and protected slots never move.

### task schedule move

```
task schedule move sat-ds doshanbe 14:00
task schedule move 3 1405-08-05 09:00
```

Moves a slot to another day and start time, for one week. The slot keeps its length. The day is a day name in the week, or a date, which then also chooses the week. A move is refused if the slot would end up outside the day (`schedule.wake` to `schedule.sleep`), or over another slot or an event: to exchange two slots, use `swap`. Fixed and protected slots never move.

### task schedule skip

```
task schedule skip sat-research
task schedule skip wed-floating next
```

Skips a slot's own work, for one week: it goes down its fallback chain instead, or stays free if the chain ends in `rest`.

### task schedule assign

```
task schedule assign wed-floating 12
task schedule assign fri-buffer project:os +homework
```

Pins work to a slot, for one week: a task, by its ID or UUID, or a filter, whose most urgent task the slot then takes. A pinned task comes before any other in that slot. Fixed and rest slots never take work.

### task schedule reset

```
task schedule reset sat-ds
task schedule reset week
```

Undoes this week's changes to one slot, or to the whole week. Resetting one slot of a swap resets the other one too. A reset is refused if the slot would land on another slot that was moved into its place: reset or move that one first.

### task schedule event

```
task schedule event add Dentist 1405-08-03 17:00-18:00
task schedule event list
task schedule event delete e1
```

One-time appointments. An event is like a fixed slot on one day: it shows in the schedule, and no slot can be moved over it. Adding an event over a slot is allowed, and the slots it covers are listed, so that you can skip or move them. Events are numbered `e1`, `e2`, and so on.

### task schedule undo

```
task schedule undo
```

Undoes the last change to the schedule: a swap, move, skip, assign, reset or event. It can be repeated, up to the last 100 changes.

`task undo` undoes changes to tasks, never to the schedule, and `task schedule undo` never changes tasks.

### task schedule log

```
task schedule log
```

Lists the changes to the schedule, most recent first, with when they were made. The first one is what `task schedule undo` undoes next.

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

## Estimates

Give a task an estimate, as a duration:

```
task add Paper draft project:research estimate:4h
task 12 modify estimate:90min
```

An estimate needs a unit: `4h`, `90min`, `1.5h`, `2d`. Taskwarrior refuses a bare number such as `estimate:4`. A very small estimate, under five minutes, is most likely a typo such as `4s` for `4h`, so `task schedule week` and `task schedule check` point it out. (In `task schedule borrow`, on the other hand, a bare number means hours: `borrow 12 2` is two hours.)

The schedule then plans the estimate times `estimate.factor` (a template setting, 1.5 in the example), since work tends to take longer than expected. The `Estimate` column shows both: `4h (6h)`.

The time still to plan is the planned time minus the time already tracked on the task with `task start` and `task stop`. Once time is tracked, the column shows what is left, as in `4h (6h), 2.5h left`, and a task past its planned time shows `over`.

A task with time left takes slots, most urgent first, until that time is covered. Only the part of a slot after the current time counts: earlier slots this week still show their task, but do not use up its time, which is either tracked already or gone. A task with no estimate, or past it, takes at most `unestimated.per-day` slots a day.

### The estimate UDA

PersianTask defines a UDA (user defined attribute) named `estimate` by default, as a duration, so `estimate:4h` works out of the box. It is an ordinary Taskwarrior UDA: it can be shown in reports (`task rc.report.next.columns=id,description,estimate rc.report.next.labels=ID,Description,Estimate next`) and used in filters (`estimate.over:2h`).

Many Taskwarrior users already have their own `estimate` UDA, often a number of hours or points. Your own definition in `.taskrc` always wins over the default, so nothing changes for your tasks; but the schedule then cannot read it as a duration. `task schedule init` and `task schedule check` detect this and explain both ways out:

- make your UDA a duration, if its values are durations already:

  ```
  task config uda.estimate.type duration
  ```

- or keep it as it is, and give the schedule a UDA of its own:

  ```
  task config schedule.estimate.uda effort
  task config uda.effort.type duration
  ```

While the estimate UDA is not usable, the schedule ignores estimates, and `task schedule week` says so.

Estimates are stored exactly as you write them; the factor is only applied when planning, so changing it later needs no change to your tasks.

## Borrowing time

```
task schedule borrow 12 2
task schedule borrow 12 1.5h next
```

When a task needs more time this week than its slots give, `borrow` takes other slots for it. It goes through the template's `borrow` setting in order:

```
borrow = fri-buffer arch wed-floating uptodate research
```

Each name is a slot, or a category, meaning that category's slots in time order. Whole slots are taken until the hours are covered. A slot can lend only if:

- it starts after the current time,
- it is not fixed, rest, protected (such as Friday afternoon), or an event,
- it is not assigned to other work already,
- and its category keeps its floor: a category lends only as long as its planned hours stay at or above its floor. Hours use each slot's length after this week's changes.

Sleep is never borrowed from, since it is not a slot. `borrow` lists the slots it took, and says if it could not cover all the hours and which categories it kept at their floor. The borrowed slots show `borrowed` in the schedule. Borrowing is a change like any other: `task schedule undo` gives the time back, and `task schedule reset` frees a slot.

## The weekly review

```
task schedule review
task schedule review last
```

A look back at a week, and ahead at the next one. It shows, in order:

1. **Hours per category**: planned, tracked, and the floor. Planned hours are those of the [`Planned:` line](#how-a-slot-gets-its-task), using each slot's length after the week's changes. Tracked hours are the time tracked with `task start` and `task stop` during that week, on any task, including tasks finished since; a task counts for the first category, in template order, whose filter it matches, and tracked time matching no category shows as `(no category)`. In a finished week, categories under their floor are marked.
2. **Floors**: the categories below their floor two weeks running. Those are the last two finished weeks: for the current week, which is not over yet, the two weeks before it. If no time at all was tracked in those weeks, floors are checked against planned hours instead, and the review says so.
3. **The buffer**: what each `buffer` slot was used for. A buffer is *full* when all its slots were lent with `borrow` or assigned with `assign`, which is the sign of work overflowing the plan; ordinary fallback work does not count. If it was full two weeks running, the plan is too heavy, and the review suggests fewer slots of the first category the template borrows from after the buffer (`arch` in the example).
4. **Deadlines**: pending tasks due this week and next week, with overdue ones marked.

Planned hours of past weeks are computed from your tasks as they are today, since the schedule keeps changes, not past plans.

## Changing one week

`swap`, `move`, `skip`, `assign` and `reset` change one week only, never the template. They change the current week, unless the command ends with a week:

| Ending | Week |
|---|---|
| (nothing) | This week |
| `next` | Next week |
| `prev` | Last week |
| `week:<date>` | The week containing the date, as in `week:1405-08-10` |

A slot is named by its id, such as `sat-ds`, or by its number in `task schedule week`. The numbers follow the template, so they do not change when slots are moved. Events are `e1`, `e2`, and so on.

### The order of layers

The week you see is built in layers, each of which can change the one before:

1. **The template**: the slots and their categories.
2. **Modes**, such as exam season. *(Coming in a later version.)*
3. **This week's changes**: move, swap, skip and assign. A change made for a week always wins over the template and over modes.
4. **Borrowing**, which gives a task extra time from other slots. It never takes a slot changed by hand.

`skip` and `assign` replace each other on a slot: the last one made counts. A moved slot keeps what it was assigned.

If the template changes so that a change no longer applies (its slot was removed, or its assigned task is done or deleted), the change is ignored, and `task schedule week` and `task schedule check` say so in a note.

## Where the schedule is stored

The schedule is not part of the Taskwarrior database. It is kept in files in the data directory, `~/.task`:

| File | Contents |
|---|---|
| `schedule` | The template, which you edit. |
| `schedule.state.json` | Each week's changes, and the events. |
| `schedule.undo.json` | The last 100 changes, for `task schedule undo` and `task schedule log`. |
| `schedule.lock` | A lock, so that `task` commands running at the same time cannot lose each other's changes. |

This has two consequences:

- **`task undo` does not undo schedule changes.** Use `task schedule undo`.
- **The schedule does not sync with your tasks.** `task sync` copies tasks only, so other devices and other Taskwarrior clients never see the schedule. Copy these files yourself to use the same schedule elsewhere.

Changes older than eight weeks are forgotten.

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
| `estimate.factor` | Planned time for a task is its [estimate](#estimates) times this factor. The default is 1. |
| `borrow` | Slots or categories that [lend time](#borrowing-time) to a task that needs more, in order. |

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
- The **floor** is the minimum number of hours a week the category should get, or `-` for none. [Borrowing](#borrowing-time) never takes a category below its floor.
- The **filter** is the rest of the line, written exactly as on the Taskwarrior command line: `project:os`, `+term`, `project:os +homework`, `due.before:now+7d`, `description.has:"two words"`, and so on. In a filter, `now` means the time of the week being planned (see [Relative dates in filters](#relative-dates-in-filters)).

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
| `filter:"..."` | Pull tasks with this filter instead of the category's. The category is still the slot's category, for planning. Inside the double quotes, quote a phrase with single quotes: `filter:"description.has:'two words'"`. |
| `fallback:"..."` | This slot's own fallback chain, replacing the global one. |
| `optional` | Shown dimmed, and counted as planned time only when it gets a task. |
| `protected` | Never lends its time to another task, and never moves. |

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
| `schedule.estimate.uda` | `estimate` | The UDA holding [estimates](#estimates). It must be a duration. |
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
$ task add Paper draft project:research priority:H estimate:2h
$ task add Tree exercises project:ds
$ task add Term project +term due:1405-09-10
```

Then look at Shanbe:

```
$ task schedule week
Week 31: Shanbe 25 Mehr 1405 - Jome 1 Aban 1405

Day              #  Time         Slot           ID  Task            Estimate  From
Shanbe 25 Mehr   1  07:00-07:30  sat-review      4  Term project              term (fallback)
                 2  07:30-10:30  sat-research    2  Paper draft     2h (3h)   research
                 3  11:00-12:30  sat-ds          3  Tree exercises            ds
                 4  14:00-16:00  sat-os-hw       1  OS homework 3             os
                 5  21:00-21:45  sat-uptodate                                 rest
```

- `sat-research` takes the paper draft. Its estimate of 2h becomes 3h of planned time (`estimate.factor = 1.5`), which this 3-hour slot covers, so the other research slots of the week are free for other research tasks.

- `sat-research`, `sat-ds` and `sat-os-hw` each take their own category's task. `sat-os-hw` only takes OS tasks tagged `+homework`, because of its `filter:` option.
- `sat-review` has no review task to do, so it goes down the fallback chain, `soon term ds arch rest`. The OS homework is due soon, but it already has its slot today, so the next step, `term`, gives the term project.
- `sat-uptodate` has no up-to-date task either, and its fallback chain has nothing left for today (the OS homework, the term project and the tree exercises already have their slots, and there is no arch task), so it reaches `rest` and stays free. Being optional, it is dimmed, and does not count as planned time.

On Yekshanbe, the classes (`sun-db-class`, `sun-net-class`, `sun-os-class`) are fixed: they appear in the schedule, but never get a task.
