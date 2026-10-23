#!/usr/bin/env python3
"""Tests for the weekly schedule (task schedule) in PersianTask.

Usage: python3 persiantask/test_schedule.py path/to/task

Each test runs in its own temporary data directory, with fixed dates, so the
results do not depend on the day they run or on the user's data.
"""

import datetime
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from testlib import check, check_true, jstr, ref, run_tests  # noqa: E402

ROOT = os.path.join(HERE, "..")

# Saturday, 1405-07-25 (2026-10-17), at noon.
NOW = "rc.schedule.now:2026-10-17T12:00:00"
# Before any slot of that Shanbe, for tests about planning, not about slots
# that have ended.
EARLY = "rc.schedule.now:2026-10-17T06:00:00"


def write_template(task, text):
    os.makedirs(os.path.join(task.dir, "data"), exist_ok=True)
    with open(os.path.join(task.dir, "data", "schedule"), "w") as f:
        f.write(text)


def slots(output):
    """{slot id: (task id, description, from)} from a schedule table.  The
    descriptions used in these tests are single words, or 'nothing to do'."""
    result = {}
    for line in output.splitlines():
        match = re.search(r"\b\d+ \d\d:\d\d-\d\d:\d\d (\S+)(?: \(moved\))?\s*(.*)$", line)
        if not match:
            continue
        rest = match.group(2).split()
        if rest and rest[0].isdigit():
            result[match.group(1)] = (rest[0], rest[1], " ".join(rest[2:]))
        elif rest[:3] == ["nothing", "to", "do"]:
            result[match.group(1)] = ("", "nothing to do", " ".join(rest[3:]))
        else:
            result[match.group(1)] = ("", "", " ".join(rest))
    return result


def places(output):
    """{slot id: (day, time, moved)} from a 'task schedule week' table."""
    result = {}
    day = None
    for line in output.splitlines():
        match = re.match(r"^((?:Shanbe|Yekshanbe|Doshanbe|Seshanbe|Chaharshanbe|Panjshanbe|Jome)"
                         r" \d+ \w+)?\s+\d* ?(\d\d:\d\d-\d\d:\d\d) (\S+)( \(moved\))?", line)
        if not match:
            continue
        day = match.group(1) or day
        result[match.group(3)] = (day.split()[0], match.group(2), bool(match.group(4)))
    return result


EDIT = """
[settings]
fallback = soon rest

[categories]
os     4   project:os
ds     2   project:ds
soon   -   due.before:now+7d

[shanbe]
08:00-09:00  a-os     study  os
09:00-10:00  b-ds     study  ds
10:00-11:00  c-fixed  fixed  os
14:00-15:00  d-ds     study  ds

[yekshanbe]
08:00-09:00  e-os     study  os
18:00-sleep  f-rest   rest   -    protected
"""


BORROW = """
[settings]
fallback = rest
borrow   = sat-buf arch res

[categories]
os     -   project:os
arch   1   project:arch
res    3   project:res

[shanbe]
08:00-09:00  sat-res   study   res
09:00-10:00  sat-fix   fixed   os
10:00-11:00  sat-arch  study   arch
12:00-13:00  sat-buf   buffer  -
13:00-14:00  sat-prot  study   arch   protected

[yekshanbe]
08:00-10:00  sun-res   study   res
10:00-11:00  sun-arch  study   arch
18:00-sleep  sun-rest  rest    -
"""


def planned(output):
    """{category: hours} from the 'Planned:' line of a week."""
    for line in output.splitlines():
        if line.startswith("Planned: "):
            result = {}
            for part in line[len("Planned: "):].split(", "):
                name, hours = part.rsplit(" ", 1)
                result[name] = hours
            return result
    return {}


SMALL = """
[settings]
fallback = soon term rest

[categories]
os      4   project:os
ds      2   project:ds
review  -   project:review
soon    -   due.before:now+7d
term    -   +term

[shanbe]
08:00-09:00  a-os     study   os
09:00-10:00  b-os     study   os
10:00-11:00  c-ds     study   ds
11:00-12:00  d-ds     study   ds      fallback:"term"
12:00-13:00  e-fixed  fixed   os
13:00-14:00  f-rest   rest    -
14:00-15:00  g-buf    buffer  -
"""


################################################################################
def test_no_template(task):
    task.out("add", "Ordinary")

    code, out, _ = task.run("schedule")
    check_true("No template: schedule shows the init hint", code != 0 and "task schedule init" in out, out)
    check_true("No template: the hint mentions help", "task schedule help" in out, out)

    code, out, _ = task.run("schedule", "check")
    check_true("No template: check shows the init hint", code != 0 and "task schedule init" in out, out)

    code, out, _ = task.run("schedule", "week")
    check_true("No template: week shows the init hint", code != 0 and "task schedule init" in out, out)

    out = task.out()
    check_true("No template: plain task shows the normal report",
               "Ordinary" in out and "schedule" not in out, out)

    out = task.out("schedule", "help")
    check_true("No template: help works, and says to run init", "not created yet" in out, out)


def test_init(task):
    out = task.out("schedule", "init")
    check_true("init points to help", "task schedule help" in out, out)

    with open(os.path.join(ROOT, "examples", "schedule", "student-week")) as f:
        example = f.read()
    with open(os.path.join(task.dir, "data", "schedule")) as f:
        check("init writes the example template", f.read(), example)

    code, out, _ = task.run("schedule", "init", "blank")
    check_true("init never overwrites a template", code != 0 and "already exists" in out, out)

    out = task.out("schedule", "check")
    check_true("The example template has no problems", "no problems" in out and "45 slots" in out, out)


def test_init_blank(task):
    task.out("schedule", "init", "blank")
    out = task.out("schedule", "check")
    check_true("The blank template has no problems", "no problems" in out, out)


def test_check_errors(task):
    write_template(task, """[settings]
fallback = soon rest term
frobnicate = 1

[categories]
os     4    project:os
soon   -    due.before:now+7d
bad    -    ( project:os

[shanbe]
08:00-10:00  sat-a   study   os
09:00-11:00  sat-b   study   os
12:00-13:00  sat-a   study   os
13:00-14:00  sat-c   studdy  os
14:00-15:00  sat-d   study   oss
15:00-14:00  sat-e   study   os
05:00-06:30  sat-f   study   os
16:00-17:00  sat-g   fixed   os      filter:"project:os"

[mondey]
""")
    code, out, _ = task.run("schedule", "check")
    check_true("check fails on a broken template", code != 0, code)

    def reports(line, text):
        return any(l.startswith("{}:{}:".format(os.path.join(task.dir, "data", "schedule"), line))
                   and text in l for l in out.splitlines())

    for line, text, name in [
            (2, "'rest' must come last", "rest not last in the fallback chain"),
            (3, "Unknown setting 'frobnicate'", "unknown setting"),
            (8, "does not work", "a filter that does not work"),
            (12, "overlaps slot 'sat-a' on line 11", "overlapping slots"),
            (13, "already defined on line 11", "duplicate slot id"),
            (14, "did you mean 'study'", "unknown kind, with a suggestion"),
            (15, "did you mean 'os'", "unknown category, with a suggestion"),
            (16, "ends before it starts", "a backwards time range"),
            (17, "outside the day", "a slot outside wake and sleep"),
            (18, "cannot have a filter", "a fixed slot with a filter"),
            (20, "Unknown section '[mondey]'", "unknown section")]:
        check_true("check reports {} on line {}".format(name, line), reports(line, text), out)

    code, out, _ = task.run("schedule", "week")
    check_true("A broken template is not shown, but points to check",
               code != 0 and "task schedule check" in out, out)


def test_week_boundaries(task):
    days = ["shanbe", "yekshanbe", "doshanbe", "seshanbe", "chaharshanbe", "panjshanbe", "jome"]
    write_template(task, "[categories]\nx - project:x\n" + "".join(
        "[{}]\n08:00-09:00  {}-slot  study  x\n".format(day, day) for day in days))

    months = ["Farvardin", "Ordibehesht", "Khordad", "Tir", "Mordad", "Shahrivar",
              "Mehr", "Aban", "Azar", "Dey", "Bahman", "Esfand"]

    def label(date):
        y, m, d = ref.to_jalali(date)
        return "{} {} {}".format(d, months[m - 1], y)

    # Weeks around Nowruz: 1404 is not a leap year, 1403 is.
    for date in [datetime.date(2026, 10, 22),   # A Thursday, inside a month.
                 datetime.date(2026, 3, 20),    # 1404-12-29, the last day of 1404.
                 datetime.date(2026, 3, 21),    # 1405-01-01, a Saturday.
                 datetime.date(2025, 3, 20),    # 1403-12-30, the leap day.
                 datetime.date(2025, 3, 21)]:   # 1404-01-01, a Friday.
        saturday = date - datetime.timedelta(days=(date.weekday() - 5) % 7)
        friday = saturday + datetime.timedelta(days=6)
        out = task.out("schedule", "week", jstr(date))
        expected = "Shanbe {} - Jome {}".format(label(saturday), label(friday))
        check_true("Week of {} ({}) runs from Shanbe to Jome".format(jstr(date), date),
                   expected in out, out.splitlines()[:2])
        order = [line.split()[0] for line in out.splitlines()
                 if re.match(r"^(Shanbe|Yekshanbe|Doshanbe|Seshanbe|Chaharshanbe|Panjshanbe|Jome) \d", line)]
        check("Week of {} lists the days in order".format(jstr(date)), order,
              ["Shanbe", "Yekshanbe", "Doshanbe", "Seshanbe", "Chaharshanbe", "Panjshanbe", "Jome"])

    out = task.out("rc.weekstart:monday", "schedule", "week", "1405-07-30")
    check_true("weekstart=monday starts the week on Doshanbe",
               "Doshanbe 27 Mehr 1405 - Yekshanbe 3 Aban 1405" in out, out.splitlines()[:2])

    for arg, expected in [("next", "Shanbe 2 Aban 1405"), ("prev", "Shanbe 18 Mehr 1405"),
                          ("۱۴۰۵-۰۸-۱۰", "Shanbe 9 Aban 1405")]:
        out = task.out(NOW, "schedule", "week", arg)
        check_true("schedule week {}".format(arg), "Week" in out and expected in out,
                   out.splitlines()[:2])


def test_fallback(task):
    write_template(task, SMALL)
    task.out("add", "OS1", "project:os", "priority:H")
    task.out("add", "OS2", "project:os")
    task.out("add", "DS1", "project:ds")
    task.out("add", "T1", "+term")
    task.out("add", "S1", "due:2026-10-19T12:00:00")  # Two days after EARLY.
    task.out("add", "Waiting", "project:ds", "wait:now+30d")
    task.out("add", "Blocker")
    task.out("add", "Blocked", "project:ds", "priority:H", "depends:7")

    found = slots(task.out(EARLY, "schedule", "week"))
    check("The earliest slot gets the most urgent task", found.get("a-os", ("", ""))[1], "OS1")
    check("A task fills one slot a day", found.get("b-os", ("", ""))[1], "OS2")
    check("A slot takes its own category's task", found.get("c-ds"), ("3", "DS1", "ds"))
    check("A slot's own fallback chain overrides the global one",
          found.get("d-ds"), ("4", "T1", "term (fallback)"))
    check("A fixed slot takes no task", found.get("e-fixed"), ("", "", "fixed"))
    check("A rest slot takes no task", found.get("f-rest"), ("", "", "rest"))
    check("A buffer goes down the global fallback chain", found.get("g-buf"), ("5", "S1", "soon (fallback)"))
    check_true("Waiting and blocked tasks are not scheduled",
               all(t[1] not in ("Waiting", "Blocked") for t in found.values()), found)


def test_fallback_order(task):
    """One slot's fallback never takes the task that a later slot's own
    category was meant for."""
    write_template(task, SMALL.replace("fallback = soon term rest", "fallback = os rest")
                   .replace("08:00-09:00  a-os     study   os", "08:00-09:00  a-os     study   review"))
    task.out("add", "OS1", "project:os")

    found = slots(task.out(EARLY, "schedule", "week"))
    check("A later slot keeps its own category's task", found.get("b-os"), ("1", "OS1", "os"))
    check("An earlier slot's fallback finds nothing, which is not a rest slot",
          found.get("a-os"), ("", "nothing to do", "rest (fallback)"))


def test_context_ignored(task):
    write_template(task, SMALL)
    task.out("add", "OS1", "project:os")
    task.out("context", "define", "dsonly", "project:ds")
    task.out("context", "dsonly")

    found = slots(task.out(EARLY, "schedule", "week"))
    check("Slots ignore the active context", found.get("a-os", ("", ""))[1], "OS1")


def test_relative_filters(task):
    """'now' in a filter means rc.schedule.now for this week, and the start of
    the week for other weeks."""
    write_template(task, """
[settings]
fallback = soon rest

[categories]
soon  -  due.after:now due.before:now+7d

[shanbe]
09:00-10:00  sat-buf  buffer  -
""")
    task.out("add", "ThisWeek", "due:2026-10-20")   # Within 7 days of EARLY.
    task.out("add", "NextWeek", "due:2026-10-28")   # Within 7 days of next Shanbe.
    task.out("add", "LastWeek", "due:2026-10-13")   # Within 7 days of last Shanbe.

    for args, expected in [([], "ThisWeek"), (["next"], "NextWeek"), (["prev"], "LastWeek")]:
        found = slots(task.out(EARLY, "schedule", "week", *args))
        check("Filters in week {} are relative to that week".format(" ".join(args) or "(this)"),
              found.get("sat-buf", ("", ""))[1], expected)


def test_quoted_filters(task):
    """Filters are re-tokenized, so quoted strings with spaces must survive."""
    write_template(task, """
[categories]
two    -   description.has:"two words"
other  -   project:nothing

[shanbe]
09:00-10:00  sat-two    study  two
10:00-11:00  sat-other  study  other  filter:"description.has:'other words'"
""")
    task.out("add", "has two words here")
    task.out("add", "with other words inside")
    task.out("add", "two and words apart")

    out = task.out(EARLY, "schedule", "week")
    check_true("A category filter with a quoted phrase", "has two words here" in out
               and "two and words apart" not in out, out)
    check_true("A slot filter with a quoted phrase", "with other words inside" in out, out)
    code, out, _ = task.run("schedule", "check")
    check_true("check accepts quoted filters", code == 0, out)


def test_tasks_alias(task):
    task.out("add", "One")
    task.out("add", "Two")

    check("task tasks runs default.command", task.out("rc.default.command:count", "tasks"), "2")
    check_true("task tasks is the next report by default", "One" in task.out("tasks"))
    check_true("task tasks never runs the schedule",
               "One" in task.out("rc.default.command:schedule", "tasks"))
    check("A user's own alias.tasks is kept",
          task.out("rc.alias.tasks:count", "rc.default.command:next", "tasks"), "2")


def test_plain_task_with_problems(task):
    write_template(task, SMALL + "\n09:00-09:30  bad  studdy  os\n")
    task.out("add", "Urgent", "priority:H")

    code, out, _ = task.run(NOW)
    check("Plain task succeeds when the template has problems", code, 0)
    check_true("Plain task shows the problems note and the report",
               "task schedule check" in out and "Urgent" in out, out)

    code, out, _ = task.run(NOW, "schedule")
    check_true("task schedule fails when the template has problems", code != 0, code)


def test_today_and_plain_task(task):
    write_template(task, SMALL)
    task.out("add", "OS1", "project:os")
    task.out("add", "Urgent", "priority:H")
    task.out("add", "Someday")

    out = task.out(NOW, "schedule")
    check_true("schedule shows today's slots", "Shanbe 25 Mehr 1405" in out and "a-os" in out, out)
    check_true("schedule shows the important tasks below", "Urgent" in out and "Someday" not in out, out)

    out = task.out(NOW)
    check_true("Plain task shows today's schedule", "a-os" in out and "Urgent" in out, out)

    out = task.out(NOW, "rc.schedule.default:off")
    check_true("schedule.default=off: plain task shows the normal report",
               "a-os" not in out and "Someday" in out, out)

    out = task.out(NOW, "project:os")
    check_true("A filter with no command shows the normal report", "a-os" not in out and "OS1" in out, out)

    out = task.out(NOW, "tasks")
    check_true("task tasks shows all tasks", "Someday" in out and "a-os" not in out, out)

    out = task.out(NOW, "rc.schedule.report:none", "schedule")
    check_true("schedule.report=none shows no report", "Urgent" not in out, out)

    code, out, _ = task.run(NOW, "rc.schedule:off", "schedule")
    check_true("schedule=off turns the schedule off", code != 0 and "turned off" in out, out)


def test_viewing_writes_nothing(task):
    write_template(task, SMALL)
    task.out("add", "OS1", "project:os")
    data = os.path.join(task.dir, "data")

    # A read-only SQLite connection leaves its -shm and -wal side files behind,
    # as upstream read-only commands such as 'task calc' do.  An empty -wal
    # means nothing was written.
    def snapshot():
        files = {}
        for name in os.listdir(data):
            info = os.stat(os.path.join(data, name))
            if name.endswith("-wal"):
                check("Viewing the schedule leaves the SQLite log empty", info.st_size, 0)
            elif not name.endswith("-shm"):
                files[name] = (info.st_mtime_ns, info.st_size)
        return files

    before = snapshot()
    for args in [["schedule"], ["schedule", "week"], ["schedule", "check"],
                 ["schedule", "review"], []]:
        task.out(NOW, *args)
    check("Viewing the schedule changes no file", snapshot(), before)


def test_help(task):
    out = task.out("schedule", "help")
    listed = re.findall(r"^  task schedule (\w+)", out, re.MULTILINE)
    check_true("help lists the commands", len(listed) >= 5, listed)

    code, out, err = task.run("schedule", "frobnicate")
    named = re.search(r"The commands are: ([\w, ]+)\.", out + err)
    check("Every command is in the help, and vice versa",
          sorted(named.group(1).split(", ")) if named else None, sorted(listed))

    for name in listed:
        code, out, _ = task.run("schedule", "help", name)
        check_true("help {} shows usage and examples".format(name),
                   code == 0 and out.startswith("Usage: task schedule " + name) and "Examples:" in out, out)

    out = task.out("schedule", "help", "template")
    check_true("help template explains the format", "[categories]" in out and "docs/schedule.md" in out, out)

    with open(os.path.join(ROOT, "docs", "schedule.md")) as f:
        docs = f.read()
    with open(os.path.join(ROOT, ".github", "README.md")) as f:
        readme = f.read()
    for name in listed:
        check_true("docs/schedule.md documents '{}'".format(name), "task schedule " + name in docs)
        check_true("The README mentions '{}'".format(name), "task schedule " + name in readme)


def test_swap(task):
    write_template(task, EDIT)

    task.out(NOW, "schedule", "swap", "a-os", "d-ds")
    found = places(task.out(NOW, "schedule", "week"))
    check("swap gives each slot the other's time", (found.get("a-os"), found.get("d-ds")),
          (("Shanbe", "14:00-15:00", True), ("Shanbe", "08:00-09:00", True)))

    found = places(task.out(NOW, "schedule", "week", "next"))
    check("swap changes only that week", found.get("a-os"), ("Shanbe", "08:00-09:00", False))

    task.out(NOW, "schedule", "swap", "2", "5")  # b-ds and e-os, by number.
    found = places(task.out(NOW, "schedule", "week"))
    check("swap by slot number", found.get("b-ds"), ("Yekshanbe", "08:00-09:00", True))

    task.out(NOW, "schedule", "swap", "b-ds", "e-os", "next")
    found = places(task.out(NOW, "schedule", "week", "next"))
    check("swap in the next week", found.get("e-os"), ("Shanbe", "09:00-10:00", True))

    for args, text in [(["a-os", "c-fixed"], "fixed slot"), (["a-os", "f-rest"], "protected"),
                       (["a-os", "a-os"], "itself"), (["a-os", "zz"], "no slot 'zz'")]:
        code, out, err = task.run(NOW, "schedule", "swap", *args)
        check_true("swap {} fails: {}".format(" ".join(args), text), code != 0 and text in err, err)


def test_move(task):
    write_template(task, EDIT)

    task.out(NOW, "schedule", "move", "d-ds", "yekshanbe", "12:00")
    found = places(task.out(NOW, "schedule", "week"))
    check("move keeps the slot's length", found.get("d-ds"), ("Yekshanbe", "12:00-13:00", True))

    task.out(NOW, "schedule", "move", "d-ds", "shanbe", "14:00")
    found = places(task.out(NOW, "schedule", "week"))
    check("Moving a slot back home is no move", found.get("d-ds"), ("Shanbe", "14:00-15:00", False))

    task.out(NOW, "schedule", "move", "b-ds", "1405-08-03", "12:00")  # Next week's Yekshanbe.
    check("move to a date changes that date's week",
          places(task.out(NOW, "schedule", "week", "next")).get("b-ds"),
          ("Yekshanbe", "12:00-13:00", True))
    check("move to a date leaves this week alone",
          places(task.out(NOW, "schedule", "week")).get("b-ds"), ("Shanbe", "09:00-10:00", False))

    task.out(NOW, "schedule", "event", "add", "Dentist", "1405-07-26", "15:00-16:00")
    for args, text in [(["b-ds", "shanbe", "10:30"], "fixed slot c-fixed"),
                       (["b-ds", "shanbe", "08:30"], "schedule swap b-ds a-os"),
                       (["b-ds", "yekshanbe", "15:30"], "event e1"),
                       (["b-ds", "shanbe", "05:00"], "outside the day"),
                       (["b-ds", "shanbe", "23:30"], "outside the day"),
                       (["c-fixed", "shanbe", "12:00"], "fixed slot"),
                       (["f-rest", "jome", "12:00"], "protected"),
                       (["b-ds", "fooday", "12:00"], "not a day"),
                       (["b-ds", "shanbe", "25:00"], "not a time"),
                       (["b-ds", "1405-08-03", "12:00", "prev"], "not in the week given")]:
        code, out, err = task.run(NOW, "schedule", "move", *args)
        check_true("move {} fails: {}".format(" ".join(args), text), code != 0 and text in err, err)

    task.out(NOW, "schedule", "move", "b-ds", "shanbe", "۱۲:۰۰")
    check("move accepts Persian digits", places(task.out(NOW, "schedule", "week")).get("b-ds"),
          ("Shanbe", "12:00-13:00", True))


def test_skip_and_assign(task):
    write_template(task, EDIT)
    task.out("add", "OS1", "project:os")
    task.out("add", "DS1", "project:ds")
    task.out("add", "Soon1", "due:2026-10-19")

    task.out(EARLY, "schedule", "skip", "b-ds")
    found = slots(task.out(EARLY, "schedule", "week"))
    check("A skipped slot goes down its fallback chain", found.get("b-ds"),
          ("3", "Soon1", "soon (fallback), skipped"))
    check("Another slot takes the skipped slot's task", found.get("d-ds", ("", ""))[1], "DS1")

    task.out(EARLY, "schedule", "assign", "b-ds", "1")
    found = slots(task.out(EARLY, "schedule", "week"))
    check("assign pins a task, and replaces a skip", found.get("b-ds"), ("1", "OS1", "assigned"))

    task.out(EARLY, "schedule", "assign", "e-os", "due.before:2026-10-25")
    found = slots(task.out(EARLY, "schedule", "week"))
    check("assign pins a filter", found.get("e-os"), ("3", "Soon1", "assigned"))

    task.out(EARLY, "schedule", "skip", "b-ds")
    found = slots(task.out(EARLY, "schedule", "week"))
    check_true("skip replaces an assign", "skipped" in found.get("b-ds", ("", "", ""))[2], found)

    for args, text in [(["c-fixed", "1"], "fixed slot"), (["f-rest", "1"], "rest slot"),
                       (["a-os", "99"], "no task 99"), (["a-os", "( project:os"], "Mismatched")]:
        code, out, err = task.run(EARLY, "schedule", "assign", *args)
        check_true("assign {} fails: {}".format(" ".join(args), text), code != 0 and text in err, err)
    code, out, err = task.run(EARLY, "schedule", "skip", "c-fixed")
    check_true("skip of a fixed slot fails", code != 0 and "fixed slot" in err, err)


def test_precedence(task):
    """template -> per-week changes: a change always wins over the template."""
    write_template(task, EDIT)
    task.out("add", "OS1", "project:os", "priority:H")
    task.out("add", "DS1", "project:ds")

    found = slots(task.out(EARLY, "schedule", "week"))
    check("The template gives a-os its category's task", found.get("a-os", ("", ""))[1], "OS1")

    task.out(EARLY, "schedule", "assign", "a-os", "2")
    task.out(EARLY, "schedule", "swap", "a-os", "d-ds")
    out = task.out(EARLY, "schedule", "week")
    check("A moved slot keeps its assigned task", slots(out).get("a-os"), ("2", "DS1", "assigned"))
    check("A moved slot is at its new time", places(out).get("a-os"), ("Shanbe", "14:00-15:00", True))


def test_reset(task):
    write_template(task, EDIT)

    task.out(NOW, "schedule", "swap", "a-os", "d-ds")
    out = task.out(NOW, "schedule", "reset", "a-os")
    check_true("Resetting one slot of a swap says so", "d-ds" in out, out)
    found = places(task.out(NOW, "schedule", "week"))
    check("Resetting one slot of a swap resets both", (found.get("a-os"), found.get("d-ds")),
          (("Shanbe", "08:00-09:00", False), ("Shanbe", "14:00-15:00", False)))

    task.out(NOW, "schedule", "move", "b-ds", "shanbe", "12:00")
    task.out(NOW, "schedule", "move", "a-os", "shanbe", "09:00")
    code, out, err = task.run(NOW, "schedule", "reset", "b-ds")
    check_true("reset refuses to put a slot over another", code != 0 and "a-os" in err, err)

    task.out(NOW, "schedule", "skip", "e-os")
    task.out(NOW, "schedule", "reset", "week")
    found = places(task.out(NOW, "schedule", "week"))
    check_true("reset week undoes every change", not any(p[2] for p in found.values()), found)
    check_true("reset week undoes skips", "skipped" not in task.out(NOW, "schedule", "week"))


def test_events(task):
    write_template(task, EDIT)

    out = task.out(NOW, "schedule", "event", "add", "Dentist", "visit", "1405-07-25", "08:30-09:30")
    check_true("event add reports overlapped slots", "a-os" in out and "b-ds" in out, out)
    task.out(NOW, "schedule", "event", "add", "Meeting", "farda", "11:00-12:00")

    out = task.out(NOW, "schedule", "week")
    check_true("Events show in the week", "Dentist visit" in out and "Meeting" in out, out)
    check("Events have their place", places(out).get("e2"), ("Yekshanbe", "11:00-12:00", False))

    out = task.out(NOW, "schedule", "event", "list")
    check_true("event list", "e1" in out and "Dentist visit" in out and "e2" in out, out)

    task.out(NOW, "schedule", "event", "delete", "e1")
    check_true("event delete", "Dentist" not in task.out(NOW, "schedule", "week"))

    for args in [["add", "X", "notadate", "10:00-11:00"], ["add", "X", "farda", "11:00-10:00"],
                 ["delete", "e9"]]:
        code, _, _ = task.run(NOW, "schedule", "event", *args)
        check_true("event {} fails".format(" ".join(args)), code != 0, code)

    code, out, err = task.run(NOW, "schedule", "skip", "e2")
    check_true("Events are not slots", code != 0 and "is an event" in err, err)


def test_undo_and_log(task):
    write_template(task, EDIT)

    out = task.out(NOW, "schedule", "undo")
    check_true("undo with no changes", "nothing to undo" in out, out)
    check_true("log with no changes", "No schedule changes" in task.out(NOW, "schedule", "log"))

    task.out(NOW, "schedule", "move", "d-ds", "yekshanbe", "12:00")
    task.out(NOW, "schedule", "skip", "a-os")
    task.out(NOW, "schedule", "event", "add", "Dentist", "1405-07-27", "17:00-18:00")

    log = task.out(NOW, "schedule", "log").splitlines()
    check_true("log lists changes, most recent first",
               "Added event" in log[0] and "Skipped a-os" in log[1] and "Moved d-ds" in log[2], log)

    out = task.out(NOW, "schedule", "undo")
    check_true("undo says what it undid", "Added event" in out, out)
    check_true("undo restores the state", "Dentist" not in task.out(NOW, "schedule", "week"))
    task.out(NOW, "schedule", "undo")
    task.out(NOW, "schedule", "undo")
    found = places(task.out(NOW, "schedule", "week"))
    check("Repeated undo restores the template", found.get("d-ds"), ("Shanbe", "14:00-15:00", False))
    check_true("Nothing left to undo", "nothing to undo" in task.out(NOW, "schedule", "undo"))

    check_true("task undo does not touch the schedule",
               task.run(NOW, "schedule", "undo")[0] == 0)


def test_dangling_references(task):
    write_template(task, EDIT)
    task.out("add", "OS1", "project:os")
    task.out(EARLY, "schedule", "assign", "a-os", "1")
    task.out(EARLY, "schedule", "skip", "b-ds")
    task.out("1", "done")
    write_template(task, EDIT.replace("09:00-10:00  b-ds     study  ds\n", ""))

    code, out, _ = task.run(EARLY, "schedule", "week")
    check("A dangling change does not break the week", code, 0)
    check_true("The week notes a change to a removed slot", "'b-ds'" in out and "Note:" in out, out)
    check_true("The week notes an assigned task that is done", "assigned to a-os" in out, out)

    code, out, _ = task.run(EARLY, "schedule", "check")
    check_true("check reports dangling changes", code == 0 and "'b-ds'" in out and "a-os" in out, out)


def test_state_files(task):
    write_template(task, EDIT)
    data = os.path.join(task.dir, "data")

    for args in [["schedule"], ["schedule", "week"], ["schedule", "check"], []]:
        task.out(NOW, *args)
    check_true("Viewing never creates the state", not os.path.exists(os.path.join(data, "schedule.state.json")))

    # Concurrent changes must not lose each other.
    import subprocess
    processes = [subprocess.Popen([task.binary, NOW, "schedule", "event", "add", "E{}".format(i),
                                   "1405-07-27", "17:00-18:00"],
                                  env=task.env, cwd=task.dir, stdin=subprocess.DEVNULL,
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                 for i in range(12)]
    check("Concurrent changes all succeed", [p.wait() for p in processes], [0] * 12)

    import json
    with open(os.path.join(data, "schedule.state.json")) as f:
        events = json.load(f)["events"]
    check("Concurrent changes are all kept", sorted(e["title"] for e in events),
          sorted("E{}".format(i) for i in range(12)))
    check("Concurrent changes have distinct ids", len(set(e["id"] for e in events)), 12)
    check("Concurrent changes are all in the log",
          len(task.out(NOW, "schedule", "log").splitlines()), 12 + 2)
    check_true("No temporary files are left", not [n for n in os.listdir(data) if n.endswith(".tmp")])

    with open(os.path.join(data, "schedule.state.json"), "w") as f:
        f.write("{ not json")
    code, out, err = task.run(NOW, "schedule", "week")
    check_true("A damaged state is reported", code != 0 and "damaged" in err, err)


def test_time_reads(task):
    """Date parsing, filters and urgency must read the time through
    Datetime::currentTime, so that the schedule can evaluate them as of
    another week.  Only write-time stamps may read the clock directly."""
    allowed = {("src/libshared/src/Datetime.cpp", "Datetime::currentTime"),
               ("src/Task.cpp", "Task::setAsNow"),
               ("src/Task.cpp", "Task::addAnnotation")}
    files = ["src/libshared/src/Datetime.cpp", "src/Variant.cpp", "src/Eval.cpp",
             "src/Filter.cpp", "src/Task.cpp"]

    found = []
    for name in files:
        function = None
        with open(os.path.join(ROOT, name)) as f:
            for number, line in enumerate(f, 1):
                definition = re.match(r"^[\w:<>&*, ]*?\b(\w+::~?\w+)\s*\(", line)
                if definition and not line.startswith(" "):
                    function = definition.group(1)
                if re.search(r"\btime\s*\(\s*(nullptr|NULL|0)\s*\)", line):
                    if (name, function) not in allowed:
                        found.append("{}:{} in {}".format(name, number, function))
    check("No direct clock reads outside write-time stamps", found, [])


def test_swap_durations(task):
    """Hours use each slot's duration after this week's changes."""
    write_template(task, """
[categories]
res   -   project:res
ds    -   project:ds

[shanbe]
08:00-11:00  sat-res  study  res
14:00-15:30  sat-ds   study  ds
""")
    task.out("add", "R1", "project:res")
    task.out("add", "D1", "project:ds")

    check("Planned hours follow the template", planned(task.out(EARLY, "schedule", "week")),
          {"res": "3h", "ds": "1.5h"})
    task.out(EARLY, "schedule", "swap", "sat-res", "sat-ds")
    check("Planned hours follow a swap", planned(task.out(EARLY, "schedule", "week")),
          {"res": "1.5h", "ds": "3h"})


def test_estimates(task):
    write_template(task, """
[settings]
estimate.factor = 1.5
fallback = rest

[categories]
os   -   project:os

[shanbe]
08:00-09:00  a  study  os
09:00-10:00  b  study  os
10:00-11:00  c  study  os
11:00-12:00  d  study  os
[yekshanbe]
08:00-09:00  e  study  os
""")
    task.out("add", "Big", "project:os", "priority:H", "estimate:2h")
    task.out("add", "Small", "project:os")

    out = task.out("rc.schedule.now:2026-10-17T06:00:00", "schedule", "week")
    found = slots(out)
    check("The estimate shows with its planned time", found.get("a"), ("1", "Big", "2h (3h) os"))
    check("An estimated task fills slots until its planned time",
          [found.get(s, ("", ""))[1] for s in "abcd"], ["Big", "Big", "Big", "Small"])
    check("An unestimated task takes one slot a day", found.get("e", ("", ""))[1], "Small")


def test_tracked_time(task):
    write_template(task, """
[settings]
estimate.factor = 1.5
fallback = rest

[categories]
os   -   project:os

[shanbe]
13:00-14:00  a  study  os
14:00-15:00  b  study  os
15:00-16:00  c  study  os
""")
    task.out("add", "Worked", "project:os", "estimate:1h")
    task.out("1", "start")
    task.out("1", "modify", "start:now-1h")
    task.out("1", "stop")

    found = slots(task.out(NOW, "schedule", "week"))
    check("Tracked time reduces what is left", found.get("a", ("", "", ""))[2], "1h (1.5h), 30min left os")
    check("Only what is left is planned", [found.get(s, ("", ""))[1] for s in "abc"],
          ["Worked", "nothing to do", "nothing to do"])

    task.out("1", "modify", "estimate:30min")
    found = slots(task.out(NOW, "schedule", "week"))
    check("A task past its estimate says so", found.get("a", ("", "", ""))[2], "30min (45min), over os")
    check("A task past its estimate takes one slot a day",
          [found.get(s, ("", ""))[1] for s in "abc"],
          ["Worked", "nothing to do", "nothing to do"])


def test_estimates_from_now(task):
    """Slots already past do not use up the estimate."""
    write_template(task, """
[settings]
estimate.factor = 1
fallback = rest

[categories]
os   -   project:os

[shanbe]
08:00-09:00  a  study  os
13:00-14:00  b  study  os
14:00-15:00  c  study  os
15:00-16:00  d  study  os
""")
    task.out("add", "Big", "project:os", "estimate:2h")
    found = slots(task.out(NOW, "schedule", "week"))  # NOW is 12:00.
    check("An ended slot takes no task, and does not use up the estimate",
          [found.get(s, ("", ""))[1] for s in "abcd"],
          ["", "Big", "Big", "nothing to do"])


def test_estimate_uda(task):
    write_template(task, SMALL)
    task.out("add", "E1", "project:os", "estimate:2h")

    code, out, _ = task.run("rc.uda.estimate.type:numeric", "schedule", "check")
    check_true("check reports an estimate UDA that is not a duration",
               code != 0 and "as a numeric" in out and "schedule.estimate.uda" in out, out)
    out = task.out(EARLY, "rc.uda.estimate.type:numeric", "schedule", "week")
    check_true("The week notes that estimates are ignored", "estimates are ignored" in out, out)

    code, out, _ = task.run("rc.schedule.estimate.uda:effort", "schedule", "check")
    check_true("check reports an undefined estimate UDA",
               code != 0 and "uda.effort.type duration" in out, out)

    task.out("rc.uda.effort.type:duration", "add", "E2", "project:os", "effort:1h")
    out = task.out(EARLY, "rc.uda.effort.type:duration", "rc.schedule.estimate.uda:effort",
                   "schedule", "week")
    check_true("Estimates come from the configured UDA", "1h (1h)" in out and "2h (2h)" not in out, out)

    os.remove(os.path.join(task.dir, "data", "schedule"))
    out = task.out("rc.uda.estimate.type:numeric", "schedule", "init")
    check_true("init explains a conflicting estimate UDA", "Note:" in out and "numeric" in out, out)


def test_borrow(task):
    write_template(task, BORROW)
    task.out("add", "OS1", "project:os")
    task.out("add", "Res1", "project:res", "estimate:10h")
    task.out("add", "Arch1", "project:arch")
    early = "rc.schedule.now:2026-10-17T06:00:00"

    out = task.out(early, "schedule", "borrow", "1", "4")
    lent = re.findall(r"^  (\S+) ", out, re.MULTILINE)
    check("borrow follows the borrow order and respects floors", lent, ["sat-buf", "sat-arch"])
    check_true("borrow reports a shortfall", "Only 2h of 4h" in out, out)
    check_true("borrow names the categories kept at their floor",
               "arch, res kept at their floor" in out, out)

    found = slots(task.out(early, "schedule", "week"))
    check("Borrowed slots work on the task", found.get("sat-arch"), ("1", "OS1", "borrowed"))

    task.out(early, "schedule", "undo")
    check_true("undo gives borrowed time back", "borrowed" not in task.out(early, "schedule", "week"))

    out = task.out("rc.schedule.now:2026-10-17T12:30:00", "schedule", "borrow", "1", "1")
    check("borrow only takes slots from now on", re.findall(r"^  (\S+) ", out, re.MULTILINE),
          ["sun-arch"])
    task.out(NOW, "schedule", "reset", "week")

    task.out(early, "schedule", "assign", "sat-buf", "3")
    out = task.out(early, "schedule", "borrow", "1", "1")
    check("borrow never takes an assigned slot", re.findall(r"^  (\S+) ", out, re.MULTILINE),
          ["sat-arch"])
    task.out(early, "schedule", "reset", "week")

    task.out(early, "schedule", "swap", "sat-buf", "sun-res")
    out = task.out(early, "schedule", "borrow", "1", "1.5h")
    check_true("borrow uses durations after a swap", "Borrowed 2h" in out and "sat-buf" in out, out)
    task.out(early, "schedule", "reset", "week")

    for args, text in [(["99", "1"], "no task 99"), (["1", "zero"], "not a number of hours"),
                       (["1", "1", "prev"], "No slot can lend")]:
        code, out, err = task.run(early, "schedule", "borrow", *args)
        check_true("borrow {} fails: {}".format(" ".join(args), text), code != 0 and text in err, err)

    write_template(task, BORROW.replace("borrow   = sat-buf arch res\n", ""))
    code, out, err = task.run(early, "schedule", "borrow", "1", "1")
    check_true("borrow needs a borrow setting", code != 0 and "no borrow setting" in err, err)


def test_write_guard(task):
    """TDB2 refuses to write while a fake current time is active.  The switch
    that forces one exists only in test builds (PERSIANTASK_TESTING)."""
    if os.environ.get("PERSIANTASK_TESTING") != "1":
        code, _, err = task.run(NOW, "rc.schedule.test.leak-now:on", "add", "Released")
        check_true("The test-only switch is not in this build", code == 0, err)
        return

    code, out, err = task.run(NOW, "rc.schedule.test.leak-now:on", "add", "Leaked")
    check_true("A write with a fake now fails", code != 0 and "Internal error" in err, err)
    check_true("The task was not written", "Leaked" not in task.run("all")[1])
    check_true("Writing works without it", task.run("add", "Fine")[0] == 0)


OVERSHOOT = """
[settings]
fallback = rest
borrow = big small

[categories]
os   -   project:os

[shanbe]
08:00-11:00  big    buffer  -
12:00-13:00  small  buffer  -
"""


def test_borrow_overshoot(task):
    write_template(task, OVERSHOOT)
    task.out("add", "OS1", "project:os")
    early = "rc.schedule.now:2026-10-17T06:00:00"

    out = task.out(early, "schedule", "borrow", "1", "0.5")
    check("borrow passes over a slot that overshoots a lot, for a better later one",
          re.findall(r"^  (\S+) ", out, re.MULTILINE), ["small"])
    check_true("borrow shows the overshoot", "took 1h for 30min needed" in out, out)
    task.out(early, "schedule", "undo")

    out = task.out(early, "schedule", "borrow", "1", "2")
    check("borrow takes a slot that overshoots by at most half its length",
          re.findall(r"^  (\S+) ", out, re.MULTILINE), ["big"])
    task.out(early, "schedule", "undo")

    write_template(task, OVERSHOOT.replace("12:00-13:00  small", "12:00-12:15  small"))
    out = task.out(early, "schedule", "borrow", "1", "0.5")
    check("borrow takes an overshooting slot when no later one covers the need better",
          re.findall(r"^  (\S+) ", out, re.MULTILINE), ["big"])
    check_true("borrow shows that overshoot", "took 3h for 30min needed" in out, out)


def test_small_estimates(task):
    write_template(task, SMALL)
    task.out("add", "Tiny", "project:os", "estimate:4s")
    task.out("add", "Fine", "project:os", "estimate:4h")

    out = task.out(NOW, "schedule", "week")
    check_true("The week warns about an estimate missing its unit",
               "estimate, 4s, looks like it is missing a unit, such as 4h" in out, out)
    check_true("Only small estimates are warned about", "4h, looks like" not in out, out)
    out = task.out("schedule", "check")
    check_true("check warns about an estimate missing its unit", "4s, looks like" in out, out)


REVIEW = """
[settings]
fallback = rest
borrow   = buf res

[categories]
res   3   project:res
os    1   project:os
net   2   project:net
ds    -   project:ds

[shanbe]
08:00-11:00  sat-res  study   res
12:00-13:00  sat-os   study   os

[jome]
10:00-11:00  buf      buffer  -
"""


def test_review(task):
    write_template(task, REVIEW)
    task.out("add", "R1", "project:res")
    task.out("add", "O1", "project:os")

    out = task.out(NOW, "schedule", "review")
    check_true("review names the week", "Review of Week 31: Shanbe 25 Mehr 1405" in out, out)
    rows = {line.split()[0]: line.split()[1:] for line in out.splitlines()
            if line.split() and line.split()[0] in ("res", "os", "net", "ds")}
    check("review shows planned hours and floors", (rows.get("res"), rows.get("os")),
          (["3h", "0h", "3h"], ["1h", "0h", "1h"]))
    check_true("review lists categories with a floor even when unplanned", "net" in rows, rows)
    check_true("review hides categories with no hours and no floor", "ds" not in rows, rows)

    check_true("review uses planned hours when nothing was tracked",
               "checked\n  against planned hours" in out, out)
    check_true("review warns about a floor missed two weeks running",
               "net was below its floor of 2h two weeks running: 0h in week 29, and 0h in week 30"
               in out, out)
    check_true("review does not warn about floors that were met",
               "res was below" not in out and "os was below" not in out, out)

    out = task.out(NOW, "schedule", "review", "last")
    check_true("review last reviews the previous week",
               "Review of Week 30" in out and "(so far)" not in out, out)


def test_review_tracked(task):
    """Tracked time counts in the week it happened (the real current week)."""
    write_template(task, REVIEW)
    task.out("add", "R1", "project:res")
    task.out("add", "X1")
    for number in ("1", "2"):
        task.out(number, "start")
        task.out(number, "modify", "start:now-1min")
        task.out(number, "stop")

    out = task.out("schedule", "review")
    rows = {line.split()[0]: line.split()[1:] for line in out[:out.index("Floors")].splitlines()
            if line.split()}
    check("Tracked time counts for the task's category", rows.get("res", [""])[1:2], ["1min"])
    check_true("Tracked time with no category is listed", "(no category)" in out, out)


def test_attribution(task):
    """Hours count for subjects: the first matching category with a floor,
    then one without, never one with accounting:off."""
    write_template(task, """
[categories]
soon   -   due.before:now+7d
os     2   project:os
misc   1   +misc accounting:off
other  -   +other

[shanbe]
08:00-10:00  sat-soon   study  soon
10:00-11:00  sat-misc   study  misc
11:00-12:00  sat-other  study  other
""")
    task.out("add", "OSHW", "project:os", "due:2026-10-19")
    task.out("add", "Misc", "+misc")
    task.out("add", "Other", "+other")

    hours = planned(task.out(EARLY, "schedule", "week"))
    check("Planned hours count for the subject, not the fallback-style category", hours,
          {"os": "2h", "other": "1h"})

    for number in ("1", "2"):
        task.out(number, "start")
        task.out(number, "modify", "start:now-1min")
        task.out(number, "stop")
    out = task.out("schedule", "review")
    rows = {line.split()[0]: line.split()[1:] for line in out[:out.index("Floors")].splitlines()
            if line.split()}
    check("Tracked hours count for the subject", rows.get("os", [""])[1:2], ["1min"])
    check_true("A soon category gets no tracked hours", rows.get("soon", ["", "0h"])[1] == "0h", rows)
    check_true("accounting:off keeps a category out of the hours",
               "misc" not in rows and "(no category)" in out, out)


def test_review_buffer(task):
    write_template(task, REVIEW)
    task.out("add", "O1", "project:os")

    out = task.out(NOW, "schedule", "review")
    check_true("review shows a free buffer", "buf (Jome 1 Aban 10:00-11:00): free" in out, out)

    task.out(NOW, "schedule", "assign", "buf", "1")
    out = task.out(NOW, "schedule", "review")
    check_true("review shows what the buffer was used for", "assigned to task 1, 'O1'" in out, out)
    check_true("One full week is not too heavy", "too heavy" not in out, out)

    task.out(NOW, "schedule", "assign", "buf", "1", "prev")
    out = task.out(NOW, "schedule", "review")
    check_true("A buffer full two weeks running means the plan is too heavy",
               "full two weeks running, so the plan is too heavy" in out, out)
    check_true("review suggests the first category lending after the buffer",
               "Consider fewer res slots" in out, out)

    write_template(task, REVIEW.replace("fallback = rest", "fallback = os rest"))
    task.out(NOW, "schedule", "reset", "week")
    task.out(NOW, "schedule", "reset", "week", "prev")
    out = task.out(NOW, "schedule", "review")
    check_true("Fallback work does not make the buffer full",
               "used for task 1" in out and "too heavy" not in out, out)


def test_review_deadlines(task):
    write_template(task, REVIEW)
    task.out("add", "Before", "due:2026-10-10")
    task.out("add", "ThisWeek", "due:2026-10-20")
    task.out("add", "NextWeek", "due:2026-10-26")
    task.out("add", "Later", "due:2026-11-10")
    task.out("add", "Overdue", "due:2026-10-17T08:00")

    out = task.out(NOW, "schedule", "review")
    deadlines = out[out.index("Deadlines"):]
    this, next = deadlines.split("Next week:")
    check_true("Deadlines this week", "ThisWeek" in this and "NextWeek" not in this, deadlines)
    check_true("Deadlines next week", "NextWeek" in next and "ThisWeek" not in next, deadlines)
    check_true("Other deadlines are left out", "Before" not in deadlines and "Later" not in deadlines,
               deadlines)
    check_true("Overdue deadlines are marked", "Overdue, overdue" in this, deadlines)


MODES = """
[settings]
fallback = rest

[categories]
res   4     project:res
arch  1.5   project:arch
os    2     project:os
term  -     +term
float -     project:float

[shanbe]
13:00-14:00  sat-res    study     res
14:00-15:00  sat-arch   study     arch
15:00-16:00  sat-float  floating  float
16:00-17:00  sat-fix    fixed     arch
17:00-18:00  sat-prot   study     arch   protected

[yekshanbe]
13:00-14:00  sun-res    study     res
14:00-15:00  sun-arch   study     arch

[doshanbe]
13:00-14:00  mon-res    study     res
14:00-15:00  mon-arch   study     arch

[mode exam]
params = course date
active = 7d before $date
give   = arch -> $course
give   = sat-float -> $course
give   = res 50% -> $course
floor  = arch 0

[mode project]
params = end
active = until $end
give   = arch except 1 -> term
give   = sat-float -> term
"""


def mode_tasks(task):
    for name, project in [("R1", "res"), ("A1", "arch"), ("A2", "arch")]:
        task.out("add", name, "project:" + project)
    for i in range(1, 6):
        task.out("add", "O{}".format(i), "project:os")
    for i in range(1, 4):
        task.out("add", "T{}".format(i), "+term")


def sources(output):
    return {slot: found[2] for slot, found in slots(output).items()}


def test_mode_exam(task):
    write_template(task, MODES)
    mode_tasks(task)

    # The exam is on Doshanbe 27 Mehr: the mode covers Shanbe and Yekshanbe.
    out = task.out(NOW, "schedule", "mode", "start", "exam", "os", "1405-07-27")
    check_true("mode start says when it is active",
               "from Doshanbe 20 Mehr 1405 to Yekshanbe 26 Mehr 1405" in out, out)

    found = sources(task.out(NOW, "schedule", "week"))
    check("A mode gives a category's slots in its window",
          (found.get("sat-arch"), found.get("sun-arch")), ("os (exam)", "os (exam)"))
    check("A mode gives a slot by id", found.get("sat-float"), "os (exam)")
    check("A mode gives half of a category's slots, alternately",
          (found.get("sat-res"), found.get("sun-res")), ("os (exam)", "res"))
    check("A mode leaves slots outside its window alone",
          (found.get("mon-res"), found.get("mon-arch")), ("res", "arch"))
    check("A mode never gives fixed or protected slots",
          (found.get("sat-fix"), found.get("sat-prot")), ("fixed", "arch"))
    check_true("The week names the active mode",
               "Mode: exam os 1405-07-27" in task.out(NOW, "schedule", "week"))

    out = task.out(NOW, "schedule", "week", "next")
    check_true("A mode ends on its own", "(exam)" not in out and "Mode:" not in out, out)

    out = task.out(NOW, "schedule", "review")
    rows = {line.split()[0]: line.split()[1:] for line in out[:out.index("Floors")].splitlines()
            if line.split()}
    check("A mode's floor applies to its weeks", rows.get("arch", ["", "", ""])[2], "0h")


def test_mode_precedence(task):
    """template -> modes -> this week's changes -> borrow."""
    write_template(task, MODES)
    mode_tasks(task)
    task.out(NOW, "schedule", "mode", "start", "exam", "os", "1405-07-27")

    task.out(NOW, "schedule", "assign", "sat-arch", "2")
    task.out(NOW, "schedule", "skip", "sun-arch")
    found = sources(task.out(NOW, "schedule", "week"))
    check("An assignment wins over a mode", found.get("sat-arch"), "assigned")
    check("A skip wins over a mode", found.get("sun-arch"), "rest (fallback), skipped")

    task.out(NOW, "schedule", "mode", "start", "project", "1405-07-26")
    found = sources(task.out(NOW, "schedule", "week"))
    check("A mode started later wins over an earlier one", found.get("sat-float"), "term (project)")


def test_mode_project(task):
    write_template(task, MODES)
    mode_tasks(task)

    task.out(NOW, "schedule", "mode", "start", "project", "1405-07-26")
    found = sources(task.out(NOW, "schedule", "week"))
    check("'except 1' keeps the first slot", (found.get("sat-arch"), found.get("sun-arch")),
          ("arch", "term (project)"))
    check("The project mode gives the floating slot", found.get("sat-float"), "term (project)")
    check("until $end includes the end day, and no more", found.get("mon-arch"), "arch")


def test_mode_commands(task):
    write_template(task, MODES)
    mode_tasks(task)

    out = task.out(NOW, "schedule", "mode", "list")
    check_true("mode list shows the template's modes",
               "exam <course> <date>, active 7d before $date" in out and "give arch -> $course" in out,
               out)

    task.out(NOW, "schedule", "mode", "start", "exam", "os", "1405-07-27")
    task.out(NOW, "schedule", "mode", "start", "exam", "res", "1405-08-15")
    out = task.out(NOW, "schedule", "mode", "list")
    check_true("mode list shows started modes",
               "m1, exam os 1405-07-27: active" in out and "m2, exam res 1405-08-15: upcoming" in out, out)

    out = task.out(NOW, "schedule", "mode", "stop", "m2")
    check_true("Stopping an upcoming mode removes it", "had not started" in out, out)
    out = task.out(NOW, "schedule", "mode", "stop", "exam")
    check_true("mode stop by name", "Stopped mode m1" in out, out)
    check_true("A stopped mode no longer applies", "(exam)" not in task.out(NOW, "schedule", "week"))

    task.out(NOW, "schedule", "undo")
    check_true("undo restarts a stopped mode", "(exam)" in task.out(NOW, "schedule", "week"))

    for args, text in [(["start", "holiday"], "no mode 'holiday'"),
                       (["start", "exam", "os"], "Usage: task schedule mode start exam <course> <date>"),
                       (["start", "exam", "maths", "1405-07-27"], "not a category"),
                       (["start", "exam", "os", "someday-soon"], "not a date"),
                       (["start", "exam", "os", "1405-07-01"], "already be over"),
                       (["stop", "m9"], "no started mode")]:
        code, out, err = task.run(NOW, "schedule", "mode", *args)
        check_true("mode {} fails: {}".format(" ".join(args), text), code != 0 and text in err, err)


EVERYDAY = """
[settings]
fallback = rest
borrow = morning

[categories]
x    -   project:x
y    -   project:y

[everyday]
07:00-07:30  morning  study  x      except:jome
12:00-13:00  lunch    rest   -      except:jome
18:00-19:00  dinner   rest   -

[shanbe]
08:00-09:00  a               study  x

[yekshanbe]
10:30-13:30  sun-long-lunch  rest   -

[doshanbe]
18:00-20:30  commute         fixed  -

[mode m]
give = morning -> y
"""


def test_everyday(task):
    """[everyday] slots apply to all days, except the excluded ones, and a
    day's own overlapping slot replaces them on that day."""
    write_template(task, EVERYDAY)
    out = task.out(NOW, "schedule", "check")
    # morning 6 days, lunch 6 days but not Yekshanbe, dinner 7 but not Doshanbe.
    check_true("The template is valid", "no problems" in out and "20 slots" in out, out)

    found = places(task.out(NOW, "schedule", "week"))
    check("Each day gets a copy, named after the day",
          (found.get("sat-lunch"), found.get("thu-dinner")),
          (("Shanbe", "12:00-13:00", False), ("Panjshanbe", "18:00-19:00", False)))
    check_true("except:jome leaves Jome out",
               "fri-lunch" not in found and "fri-morning" not in found and "fri-dinner" in found,
               sorted(found))
    check_true("A day's own slot replaces the [everyday] slot it overlaps",
               "sun-lunch" not in found and "sun-long-lunch" in found and
               "mon-dinner" not in found and "commute" in found, sorted(found))
    check_true("The [everyday] id itself is not a slot of the week",
               "lunch" not in found and "dinner" not in found, sorted(found))

    out = task.out(NOW, "schedule", "check", "--expand")
    check_true("check --expand shows where each slot comes from",
               re.search(r"12:00-13:00  sat-lunch\s+rest\s+-\s+\[everyday\] lunch", out), out)
    check_true("check --expand shows replacements",
               re.search(r"sun-long-lunch .*replaces \[everyday\] lunch", out) and
               re.search(r"commute .*replaces \[everyday\] dinner", out), out)
    check_true("check --expand shows excluded days",
               "[everyday] lunch does not apply: except:Jome" in out, out)
    check("check expand works without dashes too", task.out(NOW, "schedule", "check", "expand"), out)

    # One day's copy can be changed alone.
    task.out(NOW, "schedule", "move", "sat-lunch", "shanbe", "14:00")
    found = places(task.out(NOW, "schedule", "week"))
    check("move changes one day's copy", (found.get("sat-lunch"), found.get("sun-dinner")[1]),
          (("Shanbe", "14:00-15:00", True), "18:00-19:00"))
    code, out, err = task.run(NOW, "schedule", "skip", "morning")
    check_true("The [everyday] id names no single slot, and says which to use",
               code != 0 and "'morning' is an [everyday] slot" in err and "sat-morning" in err, err)

    # Borrowing takes the copies in time order, by the [everyday] id.
    task.out("add", "B", "project:z")
    out = task.out(NOW, "schedule", "borrow", "1", "1")
    check_true("borrow takes [everyday] copies in time order",
               "sun-morning (Yekshanbe" in out and "mon-morning (Doshanbe" in out and
               "tue-morning" not in out, out)
    task.out(NOW, "schedule", "undo")

    # Modes select all the copies by the [everyday] id.
    task.out("add", "Y", "project:y")
    task.out(NOW, "schedule", "mode", "start", "m")
    found = sources(task.out(NOW, "schedule", "week", "next"))
    check("A mode's give rule selects every copy",
          (found.get("sat-morning"), found.get("thu-morning")), ("y (m)", "y (m)"))


def test_everyday_errors(task):
    for text, message in [
            ("[everyday]\n07:00-08:00 a rest -\n08:00-09:00 b rest -\n07:30-08:30 c rest -\n",
             "Slot 'c' overlaps slot 'a'"),
            ("[everyday]\n07:00-08:00 a rest - except:jome\n[shanbe]\n07:00-08:00 b rest - except:jome\n",
             "except: only applies to [everyday] slots"),
            ("[everyday]\n07:00-08:00 a rest - except:someday\n", "'someday' is not a day name"),
            ("[everyday]\n07:00-08:00 lunch rest -\n[shanbe]\n12:00-13:00 sat-lunch rest -\n",
             "makes 'sat-lunch' on Shanbe, but line 4 already defines"),
            ("[everyday]\n07:00-08:00 a rest -\n[shanbe]\n09:00-10:00 a rest -\n",
             "Slot 'a' is already defined on line 2"),
            ("[shanbe]\n07:00-08:00 a rest -\n07:30-08:30 b rest -\n",
             "Slot 'b' overlaps slot 'a'")]:
        write_template(task, text)
        code, out, err = task.run(NOW, "schedule", "check")
        check_true("check reports: " + message, code != 0 and message in out + err, out + err)

    # Two [everyday] slots on different days do not overlap.
    write_template(task, "[everyday]\n07:00-08:00 a rest - except:jome,shanbe,yekshanbe,doshanbe,"
                         "seshanbe,chaharshanbe\n07:30-08:30 b rest - except:panjshanbe\n")
    out = task.out(NOW, "schedule", "check")
    check_true("[everyday] slots on different days may share a time", "no problems" in out, out)


def test_rest_display(task):
    """A rest slot shows 'rest'; a slot that found nothing to do, and whose
    fallback chain ends in rest, says so, dimmed like empty optional slots."""
    write_template(task, "[settings]\nfallback = rest\n[categories]\nx - project:x\n"
                         "[shanbe]\n14:00-15:00 empty study x\n15:00-16:00 lunch rest -\n"
                         "16:00-17:00 extra study x optional\n")
    out = task.out(NOW, "schedule", "week")
    found = slots(out)
    check("A rest slot", found.get("lunch"), ("", "", "rest"))
    check("An empty study slot", found.get("empty"), ("", "nothing to do", "rest (fallback)"))
    check("An empty optional slot", found.get("extra"), ("", "", "rest (fallback)"))

    # The slots are later than NOW, so neither past nor current.
    colored = task.out(NOW, "rc._forcecolor:on", "rc.color.schedule.optional:red", "schedule",
                       "week")
    rows = {re.search(r"(empty|lunch|extra)", line).group(1): line
            for line in colored.splitlines() if re.search(r"(empty|lunch|extra)", line)}
    red = "38;5;1"  # Red, combined with the row's background.
    check_true("The empty study slot is dimmed like an optional one",
               red in rows.get("empty", "") and red in rows.get("extra", ""), rows)
    check_true("The rest slot is not", red not in rows.get("lunch", ""), rows)


DUE = """
[settings]
fallback = soon rest

[categories]
x     -   project:x
y     -   project:y
soon  -   due.before:now+7d

[everyday]
10:00-11:00  x1    study  x
14:00-15:00  free  study  y
"""


def column(found, day_ids, field=1):
    return [found.get(i, ("", "", ""))[field] for i in day_ids]


WEEK = ["sat", "sun", "mon", "tue", "wed", "thu", "fri"]


def test_due_cutoff(task):
    """No task is planned in a slot that starts after it is due: neither in
    its own category's slots, nor through the fallback chain."""
    write_template(task, DUE)
    task.out("add", "X", "project:x", "due:1405-07-27T23:59")  # Doshanbe.
    task.out("add", "S", "project:z", "due:1405-07-28T23:59")  # Seshanbe.
    found = slots(task.out(NOW, "schedule", "week"))
    # On Seshanbe, the empty x1 slot (10:00) takes S through the fallback,
    # before the free slot (14:00): one slot a day for a task with no estimate.
    check("Own category: until the due day, then nothing (S comes by fallback)",
          column(found, [d + "-x1" for d in WEEK]),
          ["", "X", "X", "S", "nothing to do", "nothing to do", "nothing to do"])
    # Shanbe's x1 slot has ended, so X takes the free slot that afternoon.
    check("Fallback: until the due day, then nothing",
          column(found, [d + "-free" for d in WEEK]),
          ["X", "S", "S", "nothing to do", "nothing to do", "nothing to do", "nothing to do"])


def test_due_next_week(task):
    """The reported case: a task due on Chaharshanbe next week is not planned
    on Panjshanbe and Jome of that week."""
    write_template(task, DUE)
    task.out("add", "N", "project:x", "due:1405-08-06T23:59")
    found = slots(task.out(NOW, "schedule", "week", "next"))
    check("Next week: until Chaharshanbe only", column(found, [d + "-x1" for d in WEEK]),
          ["N", "N", "N", "N", "N", "nothing to do", "nothing to do"])
    check("Not through the soon fallback either", column(found, ["thu-free", "fri-free"]),
          ["nothing to do", "nothing to do"])


def test_overdue(task):
    """An overdue task stays eligible everywhere, marked overdue; a task
    assigned after it is due is marked so."""
    write_template(task, DUE)
    task.out("add", "O", "project:x", "due:1405-07-24")  # Jome before the week.
    task.out("add", "X", "project:y", "due:1405-07-27T23:59")
    task.out(NOW, "schedule", "assign", "thu-free", "2")
    out = task.out(NOW, "schedule", "week")
    found = slots(out)
    check("An overdue task is planned all week, and marked",
          [found.get(d + "-x1") for d in ("sat", "fri")],
          [("", "", "past"), ("1", "O", "overdue x")])
    check("A task assigned after its due is marked", found.get("thu-free"),
          ("2", "X", "after due assigned"))
    check_true("The Pace column", "Pace" in out, out)

    task.out("1", "done")
    task.out(NOW, "schedule", "reset", "week")
    out = task.out(NOW, "schedule", "week")
    check_true("No Pace column when it would be empty", "Pace" not in out, out)


def test_borrow_due(task):
    write_template(task, DUE.replace("fallback = soon rest", "fallback = rest\nborrow = free"))
    task.out("add", "B", "project:b", "due:1405-07-27T23:59", "estimate:5h")
    out = task.out(NOW, "schedule", "borrow", "1", "5")
    check_true("borrow takes only slots before the due time",
               "sat-free" in out and "mon-free" in out and "tue-free" not in out and
               "Borrowed 3h" in out, out)


CHAIN = """
[categories]
x  -  project:x

[everyday]
10:00-12:00  x  study  x
"""


def test_chained_weeks(task):
    """A later week is planned as if the work planned before it were done."""
    write_template(task, CHAIN)
    task.out("add", "E", "project:x", "estimate:16h")
    found = slots(task.out(NOW, "schedule", "week"))
    check("This week: every slot that has not ended", column(found, [d + "-x" for d in WEEK]),
          [""] + ["E"] * 6)
    # This week plans 12h (Shanbe's slot is past), so 4h are left: two slots.
    found = slots(task.out(NOW, "schedule", "week", "next"))
    check("Next week: only what is left", column(found, [d + "-x" for d in WEEK]),
          ["E", "E"] + ["nothing to do"] * 5)
    found = slots(task.out(NOW, "schedule", "week", "1405-08-10"))
    check("The week after: nothing", column(found, [d + "-x" for d in WEEK]),
          ["nothing to do"] * 7)


def test_due_change(task):
    """Plans are computed on each view, so a new due date applies at once."""
    write_template(task, DUE)
    task.out("add", "D", "project:x", "due:1405-07-27T23:59")
    found = slots(task.out(NOW, "schedule", "week"))
    check("Before: until Doshanbe", column(found, ["mon-x1", "tue-x1", "wed-x1"]),
          ["D", "nothing to do", "nothing to do"])
    task.out("1", "modify", "due:1405-07-29T23:59")
    found = slots(task.out(NOW, "schedule", "week"))
    check("After: until Chaharshanbe", column(found, ["mon-x1", "tue-x1", "wed-x1", "thu-x1"]),
          ["D", "D", "D", "nothing to do"])


PACE = """
[settings]
fallback = soon rest

[categories]
proj  -   project:proj
os    -   project:os
soon  -   due.before:now+7d

[everyday]
09:00-11:00  p  study  proj
14:00-16:00  o  study  os
"""


def days(found, slot, field=1):
    return [found.get(d + "-" + slot, ("", "", ""))[field] for d in WEEK]


def test_pacing_spreads(task):
    """A long task due in three weeks gets its share every week, instead of
    nothing until its last week, even next to a more urgent task."""
    write_template(task, PACE)
    task.out("add", "Q", "project:proj", "priority:H", "estimate:30h")
    task.out("add", "P", "project:proj", "due:1405-08-15T23:59", "estimate:18h")
    found = slots(task.out(NOW, "schedule", "week"))
    # Shanbe's slot is past.  18h over about 38h of its slots: 3 of this
    # week's 6 future slots.
    check("This week: P takes its share first, then Q", days(found, "p"),
          ["", "P", "P", "P", "Q", "Q", "Q"])
    check_true("P is on track", "on track" in found["sun-p"][2], found["sun-p"])
    found = slots(task.out(NOW, "schedule", "week", "next"))
    check("Next week: its share again", days(found, "p"), ["P", "P", "P", "Q", "Q", "Q", "Q"])

    write_template(task, PACE.replace("fallback = soon rest", "fallback = soon rest\npacing = off"))
    out = task.out(NOW, "schedule", "week")
    check("pacing = off: by urgency, as before", days(slots(out), "p"), [""] + ["Q"] * 6)
    check_true("pacing = off: no Pace column", "Pace" not in out, out)


def test_pace_and_overload(task):
    """Two tasks that each fit, but not together: the one planned first is on
    track, the other behind, and the week warns of the overload, counting
    shared slots once."""
    write_template(task, PACE.replace("14:00-16:00  o  study  os", "13:00-15:00  q  study  proj"))
    task.out("add", "A", "project:proj", "due:1405-07-27T23:59", "estimate:6h")
    task.out("add", "B", "project:proj", "due:1405-07-27T23:59", "estimate:6h")
    out = task.out(NOW, "schedule", "week")
    found = slots(out)
    # Before Doshanbe 23:59: Shanbe 13:00, and two slots on each of Yekshanbe
    # and Doshanbe, 10h for 12h of work.
    check("A first, then B", [found[s][1] for s in ("sat-q", "sun-p", "sun-q", "mon-p", "mon-q")],
          ["A", "A", "A", "B", "B"])
    check_true("A on track", "on track" in found["sun-p"][2], found["sun-p"])
    check_true("B behind 2h", "behind 2h" in found["mon-p"][2], found["mon-p"])
    check_true("The overload", "Overloaded before Doshanbe 27 Mehr: 12h of work, 10h of slots."
               in out, out)
    check_true("The overload in today", "Overloaded before" in task.out(NOW, "schedule"),
               task.out(NOW, "schedule"))
    check_true("The overload in the review", "Overloaded before" in
               task.out(NOW, "schedule", "review"), task.out(NOW, "schedule", "review"))

    # A due date change applies at once: B, due later, is no longer behind.
    task.out("2", "modify", "due:1405-08-06T23:59")
    out = task.out(NOW, "schedule", "week")
    check_true("After moving B's due date: no overload", "Overloaded" not in out, out)
    check_true("... and B is on track", all("behind" not in row[2] for row in slots(out).values()),
               out)


def test_critical(task):
    """A task that cannot be finished in time takes the slots it can reach,
    even another category's, while that category keeps its floor."""
    template = PACE.replace("os    -   project:os", "os    11  project:os")
    write_template(task, template)
    task.out("add", "C", "project:proj", "due:1405-07-27T23:59", "estimate:12h")
    task.out("add", "O", "project:os", "estimate:30h")
    out = task.out(NOW, "schedule", "week")
    found = slots(out)
    # C can use 4h of proj slots and 6h of os slots (through soon): 2h short.
    check("C takes its own slots", [found[s][1] for s in ("sun-p", "mon-p")], ["C", "C"])
    check("... and os slots only while os keeps its floor of 11h (14h a week)",
          [found[s][1] for s in ("sat-o", "sun-o", "mon-o")], ["C", "O", "O"])
    check_true("C is short", "short 2h" in found["sun-p"][2], found["sun-p"])
    check_true("A note says it cannot be finished",
               "Task 1 'C' cannot be finished before it is due" in out and "2h short" in out, out)

    write_template(task, template.replace("os    11  project:os", "os    4   project:os"))
    found = slots(task.out(NOW, "schedule", "week"))
    check("With a lower floor, all the os slots it can reach",
          [found[s][1] for s in ("sat-o", "sun-o", "mon-o", "tue-o")], ["C", "C", "C", "O"])

    write_template(task, template.replace("os    11  project:os", "os    4   project:os")
                   .replace("study  os", "study  os  protected"))
    found = slots(task.out(NOW, "schedule", "week"))
    check("Never a protected slot", [found[s][1] for s in ("sat-o", "sun-o")], ["O", "O"])


def test_behind_through_fallback(task):
    """A task behind pace takes empty slots its fallback chain reaches,
    before the chain's earlier steps."""
    template = """
[settings]
fallback = term soon rest

[categories]
free  -  project:free
term  -  +term
soon  -  due.before:now+7d

[everyday]
14:00-16:00  s  study  free
"""
    write_template(task, template)
    task.out("add", "T", "+term")
    task.out("add", "F", "project:f", "due:1405-07-29T23:59", "estimate:4h")
    found = slots(task.out(NOW, "schedule", "week"))
    check("F first, until its share is planned, then term", days(found, "s"),
          ["F", "F", "T", "T", "T", "T", "T"])
    write_template(task, template.replace("fallback = term soon rest",
                                          "fallback = term soon rest\npacing = off"))
    found = slots(task.out(NOW, "schedule", "week"))
    check("pacing = off: term comes first", days(found, "s"), ["T"] * 7)


def test_max_per_day(task):
    template = """
[settings]
max.per-day = 2

[categories]
proj  -  project:proj

[everyday]
08:00-10:00  a  study  proj
13:00-15:00  b  study  proj
"""
    write_template(task, template)
    task.out("add", "L", "project:proj", "estimate:20h")
    found = slots(task.out(NOW, "schedule", "week"))
    # Shanbe's slot a has ended, so b is that day's first.
    check("At most 2h of a task a day", (days(found, "a"), days(found, "b")),
          ([""] + ["L"] * 6, ["L"] + ["nothing to do"] * 6))

    # A critical task is not limited: 12h due Doshanbe, with 10h of slots.
    task.out("add", "K", "project:proj", "due:1405-07-27T23:59", "estimate:12h")
    found = slots(task.out(NOW, "schedule", "week"))
    check("A critical task takes whole days", [found[s][1] for s in ("sun-a", "sun-b")],
          ["K", "K"])


def test_overdue_grace(task):
    """An overdue task is critical for overdue.critical-days, and then
    planned as usual, still marked overdue."""
    write_template(task, PACE)
    task.out("add", "D", "project:proj", "due:1405-07-22", "estimate:10h")  # 3 days ago.
    task.out("add", "O", "project:os", "estimate:30h")
    found = slots(task.out(NOW, "schedule", "week"))
    check("Within the grace period: critical, it takes os slots too",
          [found[s][1] for s in ("sat-o", "sun-o")], ["D", "D"])
    check_true("marked overdue", "overdue" in found["sat-o"][2], found["sat-o"])

    write_template(task, PACE.replace("fallback = soon rest",
                                      "fallback = soon rest\noverdue.critical-days = 2"))
    found = slots(task.out(NOW, "schedule", "week"))
    check("After it: only its own slots", [found[s][1] for s in ("sat-o", "sun-o", "sun-p")],
          ["O", "O", "D"])
    check_true("still marked overdue", "overdue" in found["sun-p"][2], found["sun-p"])


def test_pacing_settings(task):
    for setting, message in [("pacing = maybe", "pacing must be on or off"),
                             ("max.per-day = lots", "max.per-day must be a number of hours"),
                             ("overdue.critical-days = -1", "overdue.critical-days must be")]:
        write_template(task, "[settings]\n" + setting + "\n")
        code, out, err = task.run(NOW, "schedule", "check")
        check_true("check reports: " + message, code != 0 and message in out, out + err)


def test_ended_slots(task):
    """In the current week, a slot that has ended takes no task: it does not
    use up the day's slots or max.per-day, nor count as work done."""
    write_template(task, "[settings]\nmax.per-day = 1\n[categories]\nproj  2  project:proj\n"
                         "[shanbe]\n08:00-09:00 a study proj\n14:00-15:00 b study proj\n"
                         "15:00-16:00 c study proj\n")
    task.out("add", "U", "project:proj")
    found = slots(task.out(NOW, "schedule"))
    check("The morning slot has ended: past, with no task", found.get("a"), ("", "", "past"))
    check("A task with no estimate still gets the afternoon", found.get("b"), ("1", "U", "proj"))

    task.out("1", "done")
    task.out("add", "L", "project:proj", "estimate:10h")
    out = task.out(NOW, "schedule")
    found = slots(out)
    check("max.per-day is not used up by the ended slot", (found["b"][1], found["c"][1]),
          ("L", "nothing to do"))
    check_true("Nothing counts as done: 10h left", "10h (10h)" in found["b"][2], found["b"])
    out = task.out(NOW, "schedule", "week")
    check_true("The ended slot still counts for its category's hours", "Planned: proj 2h" in out,
               out)


def test_overload_count(task):
    """The first overloaded date, and how many later ones."""
    write_template(task, PACE.replace("14:00-16:00  o  study  os", "13:00-15:00  q  study  proj"))
    task.out("add", "A", "project:proj", "due:1405-07-27T23:59", "estimate:12h")
    task.out("add", "B", "project:proj", "due:1405-07-28T23:59", "estimate:6h")
    task.out("add", "C", "project:proj", "due:1405-07-29T23:59", "estimate:4h")
    out = task.out(NOW, "schedule", "week")
    # By Doshanbe 12h for 10h of slots, by Seshanbe 18h for 14h, by
    # Chaharshanbe 22h for 18h.
    check_true("The first date, and the number of later ones",
               "Overloaded before Doshanbe 27 Mehr: 12h of work, 10h of slots (and 2 later dates)."
               in out, out)


if __name__ == "__main__":
    sys.exit(run_tests([test_no_template, test_init, test_init_blank, test_check_errors,
                        test_week_boundaries, test_fallback, test_fallback_order,
                        test_context_ignored, test_relative_filters, test_quoted_filters,
                        test_tasks_alias, test_plain_task_with_problems, test_today_and_plain_task,
                        test_viewing_writes_nothing, test_swap, test_move,
                        test_skip_and_assign, test_precedence, test_reset, test_events,
                        test_undo_and_log, test_dangling_references, test_state_files,
                        test_time_reads, test_swap_durations, test_estimates, test_tracked_time,
                        test_estimates_from_now, test_estimate_uda, test_borrow,
                        test_write_guard, test_borrow_overshoot, test_small_estimates,
                        test_review, test_review_tracked, test_attribution, test_review_buffer,
                        test_review_deadlines, test_mode_exam, test_mode_precedence,
                        test_mode_project, test_mode_commands, test_everyday,
                        test_everyday_errors, test_rest_display, test_due_cutoff,
                        test_due_next_week, test_overdue, test_borrow_due, test_chained_weeks,
                        test_due_change, test_pacing_spreads, test_pace_and_overload,
                        test_critical, test_behind_through_fallback, test_max_per_day,
                        test_overdue_grace, test_pacing_settings, test_ended_slots,
                        test_overload_count, test_help], fresh=True))
