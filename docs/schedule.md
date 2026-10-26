# The weekly schedule

PersianTask can plan your week. You describe a typical week once, as a template of time slots: classes, study blocks, research time. Every slot then pulls a pending task to work on, so that `task` tells you, for each part of today, what to do.

- [Quick start](#quick-start)
- [How a slot gets its task](#how-a-slot-gets-its-task)
- [Commands](#commands)
- [Estimates](#estimates)
- [Pacing](#pacing)
- [Borrowing time](#borrowing-time)
- [The weekly review](#the-weekly-review)
- [Modes](#modes)
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

Slots get their tasks in passes over the week, each in time order:

1. Work pinned with `assign` or `borrow`.
2. [Critical](#pacing) tasks, which cannot be finished in time otherwise.
3. Every slot takes a task matching its own category (or its own `filter:`): first its tasks [behind pace](#pacing), then the most urgent one. The earliest slot gets the most urgent task.
4. Every slot still empty tries its secondary categories, if it has any (`ds,algo`), in order, in the same way.
5. Every slot still empty helps the tasks behind pace that its fallback chain reaches.
6. Every slot still empty goes down its fallback chain: the slot's own `fallback:` if it has one, or else the `fallback` setting. Each step is a category, and the slot takes that category's most urgent task. The step `rest` leaves the slot free, and so does reaching the end of the chain.

Every slot's own category comes before any slot's secondary categories, and those before the fallback chain, so that one slot never takes the task that another slot's own category was meant for. Passes 2 and 5 come from [pacing](#pacing), and without it (`pacing = off`), only passes 1, 3, 4 (by urgency alone) and 6 remain, as in earlier versions. For pacing, a slot whose secondary category matches a task is one the task can reach, like a fallback slot, not one of its own.

**Due dates.** A task is never planned in a slot that starts after it is due, in either pass, nor lent such a slot by `borrow`. A task that is already overdue, at the time the week is planned, is the exception: it may take any slot, and its row says `overdue` in the `Pace` column. That column appears only when some row needs it; it also says `after due` for a task assigned by hand to a slot after its due time. For a task with an estimate, only the part of a slot before the due time counts towards the estimate.

Only pending tasks are scheduled: not waiting tasks, and not tasks blocked by a dependency. Urgency is Taskwarrior's usual urgency, so priorities (`priority:H`), due dates, tags and the rest all count. A task with an [estimate](#estimates) fills slots until its estimated time is planned. Any other task fills at most `unestimated.per-day` slots a day (one, by default), so that a single task does not take over the whole day.

The schedule ignores the active Taskwarrior context (`task context`): slots always pull from all your tasks.

Under the week, `Planned:` gives the hours of work planned for each category: the length of every slot that got a task, after this week's changes (so a swapped slot counts with its new length). Fixed slots, events and free slots do not count.

### Pacing

A task with an estimate and a due date is *paced*: its work is spread over the time it has, instead of waiting until the due date comes near. For each one, at the time the week is planned:

- **Remaining work**: its estimate times `estimate.factor`, less the time tracked on it with `task start` and `task stop`, less what earlier weeks plan (for a later week).
- **Capacity**: the time of the slots it may work in, from now until it is due, this week and the following ones, up to 26 weeks ahead. Its *own* slots are those whose own category (or `filter:`, or the category a [mode](#modes) gives them) matches it; the slots it *can reach* are its own, and those whose fallback chain has a category that matches it, such as `soon` in the example, which matches tasks due within a week. Only the part of a slot before the due time counts.
- **Laxity**: the capacity it can reach, less its remaining work: how much slack it has. The least laxity goes first.
- **This week's share**: at least even progress, its remaining work times this week's part of the time until it is due: with 9h left and 17 days to go, a week's share is 9h × 7/17, about 3.75h. If its own slots can hold all its work and this week has more than its part of them, the share follows its own slots instead. The share never counts on slots it can reach only near the due date, such as through `soon`, so its work is not pushed to the last days.

A paced task that has not got its share yet this week is *behind pace*. In its own category's slots it comes before the category's other tasks, least laxity first; and empty slots whose fallback chain reaches it help it, before the chain's own order. Once it has its share, it is planned by urgency like any other task, so it can still get ahead when there is time.

A task is **critical** when its laxity is zero or less: even all the slots it can reach until its due date are not enough. A critical task is planned first, in every slot it can reach, even one whose own category would take another task, except that it never takes a protected slot, nor a slot whose category would then fall below its weekly floor. An overdue task is critical for `overdue.critical-days` days after its due date (7 by default), and is then planned by urgency like a task with no due date, so that a forgotten task cannot take the slots forever. It is marked `overdue` either way.

`max.per-day`, if set, limits the time a task gets in a day (a first slot is always allowed, however long), except for critical tasks.

The **Pace** column says how each paced task stands:

| Pace | Meaning |
|---|---|
| `on track` | It gets its share this week. |
| `behind 2h` | This week's plan gives it 2h less than its share, since other work has the slots. |
| `short 3h` | Even all the slots it can reach until its due date leave 3h undone; a note under the week says so too. |
| `overdue` | Its due date has passed. |
| `after due` | It is assigned by hand to a slot after its due time. |

Each task's laxity assumes it could have all the slots it reaches, so two tasks can each look fine and still not fit together. So under today's schedule, the week and the review, a warning says when they do not:

```
Overloaded before Doshanbe 27 Mehr: 12h of work, 10h of slots.
```

This is the classic test of earliest-deadline-first scheduling: for each due date, the remaining work of all the tasks due by then must fit in the slots before it that any of them can use, each slot counted once. The first date that fails is shown, with the number of later dates that fail too, as in `(and 2 later dates)`.

Pacing needs nothing new from you: give tasks an estimate and a due date. Tasks without a due date are planned as before, by urgency. To turn pacing off, set `pacing = off` in the template.

### Which category hours count for

Some categories are subjects, such as `os` or `research`; others only help pick tasks, such as `soon` (tasks due soon) or `term` (tasks tagged `+term`). Hours, planned and tracked alike, count for subjects:

1. A slot's own work counts for the slot's category, if that category has a floor.
2. Otherwise (fallback work, assigned and borrowed tasks, slots whose category has no floor, and all tracked time), the hours count for the task: for the first category, in template order, **with a floor** whose filter the task matches.
3. If no category with a floor matches, for the first matching category without one.
4. If none matches, the review shows the time as `(no category)`.

So listing `soon` before `os` in the template does not book OS homework under `soon`. To keep a category out of the hours entirely, add `accounting:off` at the end of its line:

```
soon   -   due.before:now+7d   accounting:off
```

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

Today's slots in time order, with the task each one gets. The current slot is highlighted, and past slots are dimmed. The `From` column says where the task came from: the slot's category, or a fallback category, marked `(fallback)`. A slot without a task says why:

| From | Meaning |
|---|---|
| `rest` | A rest slot: a break by design, such as lunch. |
| `rest (fallback)` | A slot that found nothing to do: its fallback chain ended in `rest`. Its task column says `nothing to do`, and it is dimmed, like an optional slot. |
| `fixed` | A fixed slot, such as a class, which never takes tasks. |
| `optional` | An optional slot that found nothing to do. |
| `past` | A slot of this week that has already ended. Nothing is planned in it: the work done then is the time you tracked. |

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

On a narrow terminal, where the full table would not fit, the week (and today's schedule) is shown compact: day names are short (`Sha 2`), the `Estimate` and `Pace` columns move to a list of the tasks under the table, a slot with nothing to do shows a dimmed `-`, and in the `From` column `soon*` means work from the fallback chain and `os~` work a mode gave the slot, as a line under the table recalls. `task config schedule.layout full` (or `compact`) chooses one layout whatever the width; the default, `auto`, chooses by width.

A later week is planned as if the work planned before it were done: the weeks from this one up to it are planned in order, and the estimated time each of them plans is not planned again. So if this week plans 12 of a task's 16 hours, next week plans the other 4, and its `Estimate` column says `4h left`; once those are planned, the task takes no more slots that week, and slots with nothing else to do stay free. A note under such a week says so. Plans are computed every time you look, from your tasks as they are then, so changing a task (its due date, its estimate, finishing it) changes the plan at once.

### task schedule now

```
task schedule now
```

What to work on right now: the current slot's task, with how to start it. In a fixed slot, a break, or a slot with nothing to do, the best task for the moment, and how to take it: `task schedule open` if a class is cancelled, or `task <id> start`. Between slots, it names the next one.

### task schedule peek

```
task schedule peek thu-ds
task schedule peek thu-ds 10
```

A slot's task, marked `*`, then the next tasks it would take (5, or the number given), in the planner's order: its own category, its secondary categories, then its fallback chain, with the tasks [behind pace](#pacing) first in each. Each row says where the task would come from, its estimate and pace, and why it is not planned there now, if so: due before the slot, its estimate already planned, its slots for that day used, or `max.per-day` reached. For a fixed slot, it shows what the slot would take if it were [open](#task-schedule-open).

### task schedule next

```
task schedule next thu-ds
```

Gives the slot its next candidate, as listed by `peek`, for this week (or the week given), recorded like `assign`. Repeating it goes on down the list, past the tasks already passed over; at the end of the list, `task schedule reset thu-ds` starts it over. `task schedule undo` goes back one.

### task schedule open

```
task schedule open sun-db
task schedule open sun-db 10:00
```

For a class that is cancelled or ends early: this week, the fixed slot takes tasks like a study slot of its category (or from its fallback chain, if it has none), from the time given, by default now if the slot is today, or else the whole slot. `task schedule undo` or `reset` closes it again.

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
task schedule check --expand
```

Reads the template and reports every mistake with its line number, for example:

```
/home/you/.task/schedule:15: Slot 'sat-d': 'oss' is not a category. (did you mean 'os'?)
/home/you/.task/schedule:12: Slot 'sat-b' overlaps slot 'sat-a' on line 11.
```

It checks the syntax, unknown names, overlapping slots, times outside the day, and filters that Taskwarrior cannot read. While the template has mistakes, the other commands do not show it, and point to `task schedule check` instead.

With `--expand` (or `expand`), a valid template is also listed day by day, as the schedule uses it, with each day's copies of the [`[everyday]`](#everyday) slots, the slots that replace them, and the days they skip:

```
Doshanbe
  07:30-10:30  mon-algo-class   fixed     algo
  11:00-16:00  mon-research     study     research   replaces [everyday] lunch
  18:00-19:30  mon-os-slides    study     os
  19:30-20:30  mon-dinner       rest      -          [everyday] dinner
  20:30-22:00  mon-arch         study     arch
...
Jome
  08:00-10:00  fri-arch         study     arch
  10:30-12:00  fri-buffer       buffer    -
  12:00-24:00  fri-rest         rest      -          replaces [everyday] dinner
  [everyday] lunch does not apply: except:Jome
```

### task schedule left

```
task schedule left 12 3
task schedule left 12 90min
```

Corrects a task's estimate, by giving the hours it still needs: the estimate becomes (hours + time tracked) / `estimate.factor`. See [a wrong estimate](#estimates).

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

The time still to plan is the planned time minus the time already tracked on the task with `task start` and `task stop`. Once time is tracked, the column shows what is left, as in `4h (6h), 2.5h left`. So track your work with `task start` and `task stop`: the time tracked is what the schedule counts as done, and whatever is not tracked is planned again.

**A wrong estimate.** A task still pending once its tracked time reaches its planned time shows `over estimate`, and a note under today's schedule, the week and the review offers to correct it:

```
task schedule left 12 3
```

says that task 12 still needs 3 hours. The estimate becomes (3h + time tracked) / `estimate.factor`, so that the time left to plan is exactly 3h: with 2h tracked and a factor of 1.5, that is 3h20min, planned as 5h, less the 2h tracked. `task undo` undoes it, like any change to a task.

**Default estimates.** A category can give its tasks an estimate when they have none, with `estimate:4h` at the end of its line (see [[categories]](#categories)). The `Estimate` column then shows `4h default (6h)`. The default is never stored in the task; if a task matches several categories, it is the default of the category its [hours count for](#which-category-hours-count-for), or else of the first one in the template that has one.

**Asked when adding.** When `task add` creates a task that matches one of the schedule's categories, without an estimate, or `task modify` brings a task without one into a category, PersianTask asks for it:

```
$ task add OS homework 4 project:os +homework due:1405-08-12
Estimate for 'OS homework 4' (e.g. 3h, Enter to use the category default, 4h):
```

Enter leaves the task without an estimate (using the category's default, if any), and a plain number means hours. It only asks on a terminal, never when `task` is run by a script or with its input or output redirected, and never when the template is missing or has mistakes. `task config schedule.estimate.ask off` turns the question off.

A task with time left takes slots, most urgent first, until that time is covered. Slots of this week that have already ended take no task, and show `past`: the work done in them is the time you tracked, so they never use up a task's time, its slots for the day, or `max.per-day`. A slot under way counts only from the current time. (For a category's hours, an ended slot still counts for its own category, if that has a floor, so that the week's hours do not drop as it passes.) A task with no estimate, or past it, takes at most `unestimated.per-day` slots a day.

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

- it starts after the current time, and before the task is due (unless the task is overdue already),
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

1. **Hours per category**: planned, tracked, and the floor. Planned hours are those of the [`Planned:` line](#how-a-slot-gets-its-task), using each slot's length after the week's changes. Tracked hours are the time tracked with `task start` and `task stop` during that week, on any task, including tasks finished since, counted for categories as [above](#which-category-hours-count-for). In a finished week, categories under their floor are marked. Floors are those of the week, after any [mode](#modes).
2. **Floors**: the categories below their floor two weeks running. Those are the last two finished weeks: for the current week, which is not over yet, the two weeks before it. If no time at all was tracked in those weeks, floors are checked against planned hours instead, and the review says so.
3. **The buffer**: what each `buffer` slot was used for. A buffer is *full* when all its slots were lent with `borrow` or assigned with `assign`, which is the sign of work overflowing the plan; ordinary fallback work does not count. If it was full two weeks running, the plan is too heavy, and the review suggests fewer slots of the first category the template borrows from after the buffer (`arch` in the example).
4. **Deadlines**: pending tasks due this week and next week, with overdue ones marked.

Planned hours of past weeks are computed from your tasks as they are today, since the schedule keeps changes, not past plans.

## Modes

```
task schedule mode start exam os 1405-09-20
task schedule mode start project 1405-10-15
task schedule mode list
task schedule mode stop m1
```

A mode changes the week for a while: exam season gives architecture and part of research to the course being examined, and a project phase gives spare time to the project. Modes are defined in the template (see [\[mode name\]](#mode-name)); the example has two:

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
give   = wed-floating -> term
give   = fri-buffer -> term
```

`task schedule mode start exam os 1405-09-20` gives values to the params, in order: in the seven days before the OS exam on 20 Azar, the architecture slots, the floating slot and every other research slot work on `os`, and architecture has no floor. The mode then ends on its own. Slots given by a mode show it, as in `os (exam)`, and the week lists the active modes.

- `mode start` checks the values: dates must be dates, and categories categories. A mode whose time is already over is refused; one in the future starts on its own.
- `mode stop` ends a mode now, by its id (`m1`) or name; a mode that has not started yet is removed. Like every change, starting and stopping can be undone with `task schedule undo`.
- `mode list` shows the template's modes and the started ones.
- If two active modes give the same slot, the one started later wins. Changes made for one week (swap, move, skip, assign) win over modes.

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
2. **[Modes](#modes)**, such as exam season, which give slots to other categories for a while.
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
| `pacing` | `on` (the default) or `off`: [pacing](#pacing) of tasks with estimates and due dates. |
| `max.per-day` | The most time one task gets in a day, in hours, such as `3` or `2.5h`, except for critical tasks. No limit by default. |
| `overdue.critical-days` | For how many days after its due date an overdue task stays critical. The default is 7. |

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
- Options at the end of the line, after the filter:
  - `accounting:off` keeps the category out of [the hours](#which-category-hours-count-for) and the review.
  - `estimate:4h` is the [estimate](#estimates) of its tasks that have none, as in `os  4  project:os  estimate:3h`.
- A **catch-all** category, for free slots that should take whatever is most urgent, uses a virtual tag: `free  -  +PENDING`. Virtual tags are in capitals (`+PENDING`, `+OVERDUE`, `+DUETODAY`, `+ACTIVE`, `+READY`, ...): `+pending` is an ordinary tag, which no task has, and `task schedule check` warns about it. It also notes any category that slots use and that no pending task matches, since those slots then get work only from their fallback chain.
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

A section for each day, named in Finglish, `[shanbe]`, `[yekshanbe]`, `[doshanbe]`, `[seshanbe]`, `[chaharshanbe]`, `[panjshanbe]`, `[jome]`, or in English, `[saturday]` to `[friday]`, whatever `dayname.style` says. Each line is one slot, made of:

1. **Time**: `HH:MM-HH:MM`. `wake` and `sleep` stand for the `schedule.wake` and `schedule.sleep` settings, as in `12:00-sleep`. Slots must fit between them, and must not overlap on the same day, except that a day's slot replaces an [`[everyday]`](#everyday) slot it overlaps.
2. **Id**: a unique name, such as `sat-research`, used to refer to the slot. It uses letters, digits, `-` and `_`, and is not only digits.
3. **Kind**: `fixed`, `study`, `floating`, `buffer` or `rest` (see [above](#how-a-slot-gets-its-task)).
4. **Category**: a category name, or `-` for none. Study and floating slots need a category or a `filter:` option. A slot can also list several, in order of priority, such as `ds,algo`: the first is its own category (the one its hours count for, and modes select it by), and the others are tried when it has nothing to do, before the fallback chain. Unlike the `fallback:` option, which replaces the whole chain for that slot, secondary categories come before the usual chain:

   ```
   07:00-09:00   thu-ds   study   ds,algo
   ```
5. **Options**, any of:

| Option | Meaning |
|---|---|
| `filter:"..."` | Pull tasks with this filter instead of the category's. The category is still the slot's category, for planning. Inside the double quotes, quote a phrase with single quotes: `filter:"description.has:'two words'"`. |
| `fallback:"..."` | This slot's own fallback chain, replacing the global one. |
| `optional` | Shown dimmed, and counted as planned time only when it gets a task. |
| `protected` | Never lends its time to another task, and never moves. |

Fixed and rest slots never take tasks, so they cannot have `filter:` or `fallback:`.

### [everyday]

```
[everyday]
# time         id        kind   category  options
12:30-13:30    lunch     rest   -         except:jome
19:30-20:30    dinner    rest   -
```

Slots that every day has, such as meals, written once. They are written like the slots of a day, and can be of any kind, with any options, and one more: `except:<days>` leaves out some days, such as `except:jome` or `except:panjshanbe,jome` (Finglish or English day names).

Each day gets its own copy of every `[everyday]` slot, named after the day: `lunch` becomes `sat-lunch` on Shanbe, `sun-lunch` on Yekshanbe, then `mon-`, `tue-`, `wed-`, `thu-` and `fri-`. The copies are ordinary slots: `task schedule week` shows them, and `move`, `swap`, `skip`, `assign` and `reset` change one day's copy, as in `task schedule move sat-lunch shanbe 13:00`. Where a whole set of copies is meant, the `[everyday]` id stands for all of them: in the `borrow` setting, and in a mode's `give` rule.

**A day's own slot replaces an `[everyday]` slot that it overlaps, on that day.** This is not an overlap mistake: it is how one day differs from the others. For example, with the lunch above:

```
[yekshanbe]
10:30-13:30    sun-long-lunch   rest    -     # a longer lunch, instead of 12:30-13:30

[doshanbe]
18:00-20:30    mon-commute      fixed   -     # no dinner at home on Doshanbe
```

The whole `[everyday]` slot gives way, even if the overlap is only partial: there is no `sun-lunch` on Yekshanbe, and no `mon-dinner` on Doshanbe. When the day's slots do not cover all of the `[everyday]` slot's time, so that some of it is lost that day, `task schedule check` warns (without failing), and `check --expand` marks the slot `(partly overlapping)`. `task schedule check --expand` shows the result for every day. Two `[everyday]` slots must not overlap each other on a day they share, and a copy's name must not be taken already: an `[everyday]` slot `lunch` and a day slot `sat-lunch` cannot both exist.

Templates without an `[everyday]` section work as before.

### [mode name]

A mode is a temporary change to the week, such as exam season, which starts when you ask and ends on its own. See [Modes](#modes) for how to use them.

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
| `params` | Names for the values given when starting the mode, used as `$name`. A param used in `active` is a date; one used as a `give` target or in a `floor` is a category. |
| `active` | When the mode applies: `<N>d before $param` (the N days before that date, not the date itself), or `until $param` (from the day it is started to the end of that date). Without `active`, the mode applies from the day it is started until it is stopped. |
| `give` | `<slots> [N% \| except N] -> <category>`: those slots work on another category while the mode is active. `<slots>` is a category name (its slots), a slot id, or `kind:<kind>`. `50%` gives every other one, spread over the time, and `except 1` all but the first in the week. Fixed, rest and protected slots are never given. |
| `floor` | `<category> <hours>`: a different floor, in any week the mode is active. |

Notes on modes:

- A percentage is spread evenly over the selected slots, in time order, but is approximate when it does not divide 100: with `33%`, the 1st, 5th and 8th slots are given, so 1 of 3 slots, but 2 of 5 (40%). The share comes out exact for 50%, 25%, 20% and 10% when the number of slots is a multiple of 2, 4, 5 or 10.
- A `floor` rule applies to **every week the mode overlaps, even by a single day**. An exam on a Monday thus changes the floor of the week before it as a whole, and also of the exam's own week if the seven days reach into it. A rule that lowers a floor (as `floor = arch 0` does) is harmless there; but keep in mind that a rule raising a floor would demand the higher floor for those whole weeks.

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
| `schedule.estimate.ask` | `on` | Ask for an estimate in `task add` and `task modify`, on a terminal, when a task comes into a category without one. |
| `schedule.layout` | `auto` | The layout of the schedule's tables: `full`, `compact` for narrow terminals, or `auto`, which chooses by width. |
| `dateformat.schedule` | `A d B Y` | The date format of the schedule's headers. |
| `color.schedule.header` | `bold` | Color of the day headers. |
| `color.schedule.now` | `bold green` | Color of the current slot. |
| `color.schedule.past` | `gray12` | Color of past slots. |
| `color.schedule.optional` | `gray12` | Color of optional slots with no task, and of slots with nothing to do (`rest (fallback)`). |
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
                 4  12:30-13:30  sat-lunch                                    rest
                 5  14:00-16:00  sat-os-hw       1  OS homework 3             os
                 6  19:30-20:30  sat-dinner                                   rest
                 7  21:00-21:45  sat-uptodate                                 rest (fallback)
```

- `sat-research` takes the paper draft. Its estimate of 2h becomes 3h of planned time (`estimate.factor = 1.5`), which this 3-hour slot covers, so the other research slots of the week are free for other research tasks.

- `sat-research`, `sat-ds` and `sat-os-hw` each take their own category's task. `sat-os-hw` only takes OS tasks tagged `+homework`, because of its `filter:` option.
- `sat-review` has no review task to do, so it goes down the fallback chain, `soon term ds arch rest`. The OS homework is due soon, but it already has its slot today, so the next step, `term`, gives the term project.
- `sat-uptodate` has no up-to-date task either, and its fallback chain has nothing left for today (the OS homework, the term project and the tree exercises already have their slots, and there is no arch task), so it reaches `rest` and stays free: `rest (fallback)`, unlike `sat-lunch` and `sat-dinner`, which are rest slots by design, from the template's `[everyday]` section. Being optional, it is dimmed, and does not count as planned time.

On Yekshanbe, the classes (`sun-db-class`, `sun-net-class`, `sun-os-class`) are fixed: they appear in the schedule, but never get a task.
