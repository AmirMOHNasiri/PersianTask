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


def write_template(task, text):
    os.makedirs(os.path.join(task.dir, "data"), exist_ok=True)
    with open(os.path.join(task.dir, "data", "schedule"), "w") as f:
        f.write(text)


def slots(output):
    """{slot id: (task id, description, from)} from a schedule table.  The
    descriptions used in these tests are single words."""
    result = {}
    for line in output.splitlines():
        match = re.search(r"\b\d+ \d\d:\d\d-\d\d:\d\d (\S+)\s*(.*)$", line)
        if not match:
            continue
        rest = match.group(2).split()
        if rest and rest[0].isdigit():
            result[match.group(1)] = (rest[0], rest[1], " ".join(rest[2:]))
        else:
            result[match.group(1)] = ("", "", " ".join(rest))
    return result


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
    check_true("The example template has no problems", "no problems" in out and "34 slots" in out, out)


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
    task.out("add", "S1", "due:now+2d")
    task.out("add", "Waiting", "project:ds", "wait:now+30d")
    task.out("add", "Blocker")
    task.out("add", "Blocked", "project:ds", "priority:H", "depends:7")

    found = slots(task.out(NOW, "schedule", "week"))
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

    found = slots(task.out(NOW, "schedule", "week"))
    check("A later slot keeps its own category's task", found.get("b-os"), ("1", "OS1", "os"))
    check("An earlier slot's fallback finds nothing", found.get("a-os"), ("", "", "rest"))


def test_context_ignored(task):
    write_template(task, SMALL)
    task.out("add", "OS1", "project:os")
    task.out("context", "define", "dsonly", "project:ds")
    task.out("context", "dsonly")

    found = slots(task.out(NOW, "schedule", "week"))
    check("Slots ignore the active context", found.get("a-os", ("", ""))[1], "OS1")


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
    for args in [["schedule"], ["schedule", "week"], ["schedule", "check"], []]:
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


if __name__ == "__main__":
    sys.exit(run_tests([test_no_template, test_init, test_init_blank, test_check_errors,
                        test_week_boundaries, test_fallback, test_fallback_order,
                        test_context_ignored, test_today_and_plain_task,
                        test_viewing_writes_nothing, test_help], fresh=True))
