#!/usr/bin/env python3
"""Tests for the calendar settings: calendar=jalali|gregorian and
dayname.style=finglish|english, in all four combinations.

Usage: python3 persiantask/test_calendar.py path/to/task
"""

import datetime
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from testlib import check, check_true, jstr, ref, run_tests  # noqa: E402

COMBINATIONS = [("jalali", "finglish"), ("jalali", "english"),
                ("gregorian", "finglish"), ("gregorian", "english")]


def settings(calendar, style):
    return ["rc.calendar:" + calendar, "rc.dayname.style:" + style]


################################################################################
def test_display(task):
    """Every combination shows dates in its calendar, with its day names."""
    task.out("add", "Sunday task", "due:2026-10-18")
    expected = {
        ("jalali", "finglish"): ("Yekshanbe 26 Mehr 1405", "1405-07-26"),
        ("jalali", "english"): ("Sunday 26 Mehr 1405", "1405-07-26"),
        ("gregorian", "finglish"): ("Yekshanbe 18 October 2026", "2026-10-18"),
        ("gregorian", "english"): ("Sunday 18 October 2026", "2026-10-18"),
    }
    report = ["rc.report.x.columns:due", "rc.report.x.labels:Due", "rc.report.x.filter:status:pending"]
    for (calendar, style), (long, short) in expected.items():
        name = "{}/{}".format(calendar, style)
        out = task.out(*settings(calendar, style), "rc.dateformat.report:A d B Y", *report, "x")
        check("{}: a report".format(name), out, long)
        out = task.out(*settings(calendar, style), "1", "info")
        check_true("{}: task info".format(name), re.search(r"Due\s+" + short, out), out)


def test_calendar_view(task):
    for (calendar, style), (month, days) in {
            ("jalali", "finglish"): ("Mehr 1405", "Sh Ye Do Se Ch Pa Jo"),
            ("jalali", "english"): ("Mehr 1405", "Sa Su Mo Tu We Th Fr"),
            ("gregorian", "finglish"): ("October 2026", "Sh Ye Do Se Ch Pa Jo"),
            ("gregorian", "english"): ("October 2026", "Sa Su Mo Tu We Th Fr")}.items():
        when = ["10", "2026"] if calendar == "gregorian" else ["7", "1405"]
        out = task.out(*settings(calendar, style), "rc.calendar.monthsperline:1", "calendar", *when)
        check_true("{}/{}: the calendar's month".format(calendar, style), month in out, out[:200])
        check_true("{}/{}: the calendar's days".format(calendar, style), days in out, out[:200])

    out = task.out("rc.calendar:gregorian", "rc.weekstart:sunday", "rc.dayname.style:english",
                   "rc.calendar.monthsperline:1", "calendar", "10", "2026")
    check_true("gregorian with weekstart=sunday, as upstream", "Su Mo Tu We Th Fr Sa" in out, out[:200])


def test_named_dates(task):
    """Named dates follow the calendar.  With gregorian, weeks are upstream's:
    Monday-based, whatever weekstart says."""
    today = datetime.date.today()
    jy, jm, jd = ref.to_jalali(today)
    next_jm = ref.from_jalali(jy, jm + 1, 1)
    next_gm = (today.replace(day=28) + datetime.timedelta(days=4)).replace(day=1)
    for calendar, word, expected in [
            ("jalali", "som", "{:04d}-{:02d}-01T00:00:00".format(jy, jm)),
            ("gregorian", "som", today.strftime("%Y-%m-01T00:00:00")),
            ("jalali", "eom", "{}T23:59:59".format(jstr(next_jm - datetime.timedelta(days=1)))),
            ("gregorian", "eom", (next_gm - datetime.timedelta(days=1)).strftime("%Y-%m-%dT23:59:59")),
            ("jalali", "soy", "{:04d}-01-01T00:00:00".format(jy)),
            ("gregorian", "soy", today.strftime("%Y-01-01T00:00:00"))]:
        check("{}: {}".format(calendar, word), task.out("rc.calendar:" + calendar, "calc", word),
              expected)

    task.out("rc.calendar:gregorian", "add", "Sow", "due:sow")
    show = ["rc.dayname.style:english", "rc.report.x.columns:due", "rc.report.x.labels:D",
            "rc.dateformat.report:A", "rc.report.x.filter:description:Sow", "x"]
    check("gregorian: sow is a Monday, as upstream", task.out("rc.calendar:gregorian", *show),
          "Monday")
    check("The stored date does not change with the calendar", task.out(*show), "Monday")

    for calendar in ("jalali", "gregorian"):
        check("{}: day names in either style".format(calendar),
              task.out("rc.calendar:" + calendar, "calc", "shanbe"),
              task.out("rc.calendar:" + calendar, "calc", "saturday"))

    check("gregorian: English month names", task.out("rc.calendar:gregorian", "calc", "december"),
          task.out("rc.calendar:gregorian", "calc", "{}-12-01".format(
              today.year + (1 if today.month == 12 else 0))))
    check("jalali: Jalali month names", task.out("calc", "dey"),
          task.out("calc", "{}-10-01".format(jy + (1 if jm >= 10 else 0))))


def test_numeric_dates(task):
    """How Y-M-D dates are read under each calendar."""
    task.out("add", "J", "due:1405-07-26")
    task.out("add", "G", "due:2026-10-18")
    task.out("rc.calendar:gregorian", "add", "GG", "due:2026-10-18")
    dues = {t["description"]: t["due"] for t in task.export()}
    check("jalali: 1405-07-26 is a Jalali date", dues["J"], dues["G"])
    check("jalali: 2026-10-18 is a Gregorian date", dues["G"], dues["GG"])

    code, out, err = task.run("rc.calendar:gregorian", "add", "X", "due:1405-07-26")
    check_true("gregorian: a Jalali-looking date fails, with a hint",
               code != 0 and "for Jalali dates, use 'task config calendar jalali'" in err, err)
    code, out, err = task.run("add", "X", "due:1800-01-01")
    check_true("jalali: a year between the calendars fails, with a hint",
               code != 0 and "years before 1700 are Jalali" in err, err)
    code, out, err = task.run("add", "X", "due:1404-12-30")
    check_true("jalali: an invalid Jalali date fails, without a misleading hint",
               code != 0 and "With calendar" not in err, err)

    task.out("rc.dateformat:y-M-D", "add", "J2", "due:05-07-26")
    task.out("rc.calendar:gregorian", "rc.dateformat:y-M-D", "add", "G2", "due:26-10-18")
    dues = {t["description"]: t["due"] for t in task.export()}
    check("Two-digit years: 14xx with jalali, 20xx with gregorian", dues["J2"], dues["G2"])


def test_switching_keeps_data(task):
    """The calendar only changes display and parsing, never stored data."""
    task.out("add", "One", "due:1405-07-26", "scheduled:1405-07-20", "wait:1405-07-01")
    task.out("add", "Two", "due:eom", "recur:monthly")
    task.out("add", "Three", "project:x", "estimate:2h")
    task.out("list")  # Generates the recurring instances.

    # Urgency is computed when exporting, from the task's age among others,
    # so it changes as time passes; everything else is stored data.
    def stored():
        return [{k: v for k, v in t.items() if k != "urgency"} for t in task.export()]

    before = stored()

    for calendar, style in COMBINATIONS + [("jalali", "finglish")]:
        task.out("config", "calendar", calendar)
        task.out("config", "dayname.style", style)
        for args in [["list"], ["1", "info"], ["calendar"], ["next"], ["summary"]]:
            task.run(*args)
        check("Export is unchanged under {}/{}".format(calendar, style), stored(), before)


def test_settings(task):
    out = task.out("show")
    check_true("task show reports no unrecognized variables", "unrecognized" not in out, out)
    check_true("task show shows calendar", re.search(r"calendar\s+jalali", out), out)
    check_true("task show shows dayname.style", re.search(r"dayname.style\s+finglish", out), out)

    for setting, value, text in [("calendar", "persian", "'jalali' or 'gregorian'"),
                                 ("dayname.style", "farsi", "'finglish' or 'english'")]:
        code, out, err = task.run("rc.{}:{}".format(setting, value), "calc", "now")
        check_true("An invalid {} is refused".format(setting), code != 0 and text in err, err)


def test_weekdays_and_weekend(task):
    """recur:weekdays and the calendar's weekend follow the calendar."""
    # 1405-07-23 is a Thursday (2026-10-15).
    task.out("add", "W", "due:2026-10-14", "recur:weekdays")  # A Wednesday.
    task.out("list")
    dues = sorted(t["due"][:8] for t in task.export() if t["status"] == "pending")
    check_true("jalali: weekdays skip Thursday and Friday",
               "20261015" not in dues and "20261016" not in dues and "20261017" in dues, dues)

    task.out("rc.calendar:gregorian", "add", "G", "due:2026-10-16", "recur:weekdays")  # A Friday.
    task.out("rc.calendar:gregorian", "list")
    dues = sorted(t["due"][:8] for t in task.export("rc.calendar:gregorian", "description:G")
                  if t["status"] == "pending")
    check_true("gregorian: weekdays skip Saturday and Sunday, as upstream",
               "20261017" not in dues and "20261018" not in dues and "20261019" in dues, dues)

    # Switching the calendar must not add instances on other dates.
    task.out("list")
    after = sorted(t["due"][:8] for t in task.export("description:G") if t["status"] == "pending")
    check("A recurring task keeps its calendar's rule after a switch", after, dues)
    parent = [t for t in task.export("description:G") if t["status"] == "recurring"][0]
    check("A recurring task records its calendar", parent.get("rcalendar"), "gregorian")
    check_true("Its instances do not", all("rcalendar" not in t for t in task.export("description:G")
                                           if t["status"] != "recurring"))


def test_rcalendar(task):
    """Recurring tasks keep the calendar they were created under, in the
    rcalendar UDA, which is declared, so that it is not an orphan."""
    # Due today: a fixed date would make more instances, and a longer mask in
    # task info, with every day that passes.
    task.out("add", "Daily", "due:today", "recur:daily")
    task.out("rc.calendar:gregorian", "add", "Yearly", "due:2026-10-18", "recur:yearly")
    parents = {t["description"]: t for t in task.export("status:recurring")}
    check("Stamped when created, before any report", parents["Daily"].get("rcalendar"), "jalali")
    check("Stamped with the calendar in use", parents["Yearly"].get("rcalendar"), "gregorian")

    out = task.out("rc.verbose:on", "udas")
    check_true("task udas lists rcalendar as a UDA", re.search(r"rcalendar\s+string", out), out)
    check_true("task udas finds no orphans", "rphan" not in out, out)
    out = task.out("rc.verbose:on", "status:recurring", "description:Daily", "info")
    check_true("task info shows the calendar, labelled",
               re.search(r"Recurrence calendar\s+jalali", out) and "rphan" not in out, out)
    out = task.out("diag")
    check_true("task diag finds no orphans", "rphan" not in out, out)
    code, out, err = task.run("status:recurring", "description:Daily", "modify", "rcalendar:persian")
    check_true("Only jalali and gregorian are accepted", code != 0, out + err)


def test_rcalendar_old_tasks(task):
    """A recurring task made before rcalendar existed has none, and is given
    jalali, the only calendar of earlier versions, whatever the setting."""
    # 2026-08-01 is 10 Mordad 1405: a Jalali month later is 2026-09-01, and
    # two are 2026-10-02, where Gregorian months give 2026-10-01.
    off = "rc.recurrence:off"  # No instances yet.
    task.out(off, "add", "Old", "due:2026-08-01", "recur:monthly")
    uuid = task.export(off, "status:recurring")[0]["uuid"]
    task.out(off, "rc.confirmation:off", "rc.recurrence.confirmation:no", uuid, "modify",
             "rcalendar:")
    parent = task.export(off, uuid)[0]
    check_true("Like an old task: no rcalendar, no instances",
               "rcalendar" not in parent and not parent.get("mask"), parent)

    task.out("rc.calendar:gregorian", "list")
    parent = task.export(uuid)[0]
    check("An old task is given jalali, even with calendar=gregorian",
          parent.get("rcalendar"), "jalali")
    dues = sorted(t["due"][:8] for t in task.export("parent:" + uuid))
    check_true("... and its instances follow Jalali months",
               "20260901" in dues and "20261002" in dues and "20261001" not in dues, dues)


def test_weekstart_hint(task):
    """Switching to the Gregorian calendar suggests, but does not change,
    weekstart."""
    hint = "weekstart is still saturday; set it with 'task config weekstart monday' if you want"
    out = task.out("rc.confirmation:off", "config", "calendar", "gregorian")
    check_true("A hint when weekstart is still saturday", hint in out, out)
    check("weekstart is not changed", task.out("_get", "rc.weekstart"), "saturday")
    out = task.out("rc.confirmation:off", "config", "calendar", "jalali")
    check_true("No hint when switching to jalali", "weekstart" not in out, out)
    task.out("rc.confirmation:off", "config", "weekstart", "monday")
    out = task.out("rc.confirmation:off", "config", "calendar", "gregorian")
    check_true("No hint with another weekstart", "weekstart" not in out, out)


def test_schedule(task):
    """The schedule follows both settings; day sections accept both styles."""
    os.makedirs(os.path.join(task.dir, "data"), exist_ok=True)
    with open(os.path.join(task.dir, "data", "schedule"), "w") as f:
        f.write("[categories]\nx - project:x\n"
                "[shanbe]\n08:00-09:00 a study x\n"
                "[sunday]\n08:00-09:00 b study x\n")
    now = "rc.schedule.now:2026-10-17T12:00:00"
    for (calendar, style), label in {
            ("jalali", "finglish"): "Shanbe 25 Mehr 1405 - Jome 1 Aban 1405",
            ("jalali", "english"): "Saturday 25 Mehr 1405 - Friday 1 Aban 1405",
            ("gregorian", "finglish"): "Shanbe 17 October 2026 - Jome 23 October 2026",
            ("gregorian", "english"): "Saturday 17 October 2026 - Friday 23 October 2026"}.items():
        out = task.out(now, *settings(calendar, style), "schedule", "week")
        check_true("{}/{}: the schedule's week".format(calendar, style), label in out, out[:200])
        check_true("{}/{}: both day sections".format(calendar, style),
                   " a " in out and " b " in out, out)


if __name__ == "__main__":
    sys.exit(run_tests([test_display, test_calendar_view, test_named_dates, test_numeric_dates,
                        test_switching_keeps_data, test_settings, test_weekdays_and_weekend,
                        test_rcalendar, test_rcalendar_old_tasks, test_weekstart_hint,
                        test_schedule], fresh=True))
