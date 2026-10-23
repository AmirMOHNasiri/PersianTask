#!/usr/bin/env python3
"""Tests for the Jalali (Shamsi) calendar in PersianTask.

Usage: python3 persiantask/test_jalali.py path/to/task

Runs the given task binary against a temporary data directory, and compares
its dates with the independent reference conversion in jalali_reference.py.
Prints TAP-style results, and exits non-zero if any test fails.
"""

import datetime
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from testlib import (SATURDAY, check, check_true, export_date, jstr, ref,  # noqa: E402
                     run_tests, tm_wday)


################################################################################
def test_gregorian_to_jalali(task):
    """Gregorian input is displayed as Jalali."""
    dates = []
    date = datetime.date(1980, 1, 7)
    while date < datetime.date(2094, 1, 1):
        dates.append(date)
        date += datetime.timedelta(days=173)
    # Every Nowruz and the day before it, around the present.
    for jy in range(1395, 1415):
        nowruz = ref.from_jalali(jy, 1, 1)
        dates += [nowruz, nowruz - datetime.timedelta(days=1)]

    bad = []
    for date in dates:
        actual = task.calc(date.isoformat())
        expected = jstr(date) + "T00:00:00"
        if actual != expected:
            bad.append("{} -> {} (expected {})".format(date, actual, expected))
    check_true("Gregorian -> Jalali for {} dates".format(len(dates)), not bad, "; ".join(bad[:5]))


def test_jalali_to_gregorian(task):
    """Jalali input is stored as the right Gregorian date."""
    # Taskwarrior does not accept dates before 1980 (Jalali 1359).
    dates = []
    for i in range(220):
        jy = 1360 + i // 2
        jm = 1 + (i * 5) % 12
        jd = 1 + (i * 7) % 29
        dates.append((jy, jm, jd))
    # Every month end, including Esfand 30th in leap years.
    for jy in range(1395, 1415):
        for jm in range(1, 13):
            dates.append((jy, jm, ref.days_in_month(jy, jm)))

    # The description holds the input and the expected Gregorian date.
    for jy, jm, jd in dates:
        jalali = "{:04d}-{:02d}-{:02d}".format(jy, jm, jd)
        task.out("add", "project:conv", "due:" + jalali,
                 jalali, ref.from_jalali(jy, jm, jd).isoformat())

    bad = []
    for t in task.export("project:conv"):
        jalali, expected = t["description"].split()
        actual = export_date(t["due"]).isoformat()
        if actual != expected:
            bad.append("{} -> {} (expected {})".format(jalali, actual, expected))
    check_true("Jalali -> Gregorian for {} dates".format(len(dates)), not bad, "; ".join(bad[:5]))


def test_validity(task):
    for date, valid in [("1403-12-30", True),   # 1403 is a leap year.
                        ("1404-12-30", False),  # 1404 is not.
                        ("1404-12-29", True),
                        ("1405-06-31", True),   # The first six months have 31 days.
                        ("1405-07-31", False),  # The next five have 30.
                        ("1405-13-01", False),
                        ("1405-00-10", False)]:
        code, _, _ = task.run("add", "project:valid", "due:" + date, "validity test")
        check("{} is {}".format(date, "valid" if valid else "invalid"), code == 0, valid)


def test_digits(task):
    check("Persian digits", task.calc("۱۴۰۵-۰۸-۰۱"), "1405-08-01T00:00:00")
    check("Arabic-Indic digits", task.calc("١٤٠٥-٠٨-٠١"), "1405-08-01T00:00:00")


def test_machine_formats(task):
    """Formats used by export, import, JSON and hooks stay Gregorian."""
    check("Internal UTC format", task.calc("20250320T120000Z"), "1403-12-30T12:00:00")
    check("ISO Gregorian date", task.calc("2026-10-16"), "1405-07-24T00:00:00")

    task.out("add", "project:export", "due:1405-08-01", "export test")
    check("Export is Gregorian", task.export("project:export")[0]["due"], "20261023T000000Z")

    imported = {"description": "import test", "project": "import",
                "status": "pending", "due": "20250320T000000Z",
                "uuid": "8c8a8f4f-2d4d-4f4e-9a52-3a3d6c0a1b2c"}
    path = os.path.join(task.dir, "import.json")
    with open(path, "w") as f:
        json.dump([imported], f)
    task.out("import", path)
    check("Import is Gregorian", task.out("_get", imported["uuid"] + ".due"), "1403-12-30T00:00:00")


def named_dates(today):
    """The value of each named date, on the given day."""
    jy, jm, jd = ref.to_jalali(today)
    day = datetime.timedelta(days=1)

    def start(date):
        return jstr(date) + "T00:00:00"

    def end(date):
        return jstr(date) + "T23:59:59"

    sow = today - datetime.timedelta(days=(tm_wday(today) - SATURDAY) % 7)
    qm = jm - (jm - 1) % 3

    expected = {
        "today":     start(today),
        "yesterday": start(today - day),
        "tomorrow":  start(today + day),
        "sopw":      start(sow - 7 * day),
        "sow":       start(sow),
        "sonw":      start(sow + 7 * day),
        "eopw":      end(sow - day),
        "eow":       end(sow + 6 * day),
        "eonw":      end(sow + 13 * day),
        "soww":      start(sow),
        "eoww":      end(sow + 4 * day),  # Saturday to Wednesday.
        "sopm":      start(ref.from_jalali(jy, jm - 1, 1)),
        "som":       start(ref.from_jalali(jy, jm, 1)),
        "sonm":      start(ref.from_jalali(jy, jm + 1, 1)),
        "eopm":      end(ref.from_jalali(jy, jm, 1) - day),
        "eom":       end(ref.from_jalali(jy, jm + 1, 1) - day),
        "eonm":      end(ref.from_jalali(jy, jm + 2, 1) - day),
        "sopq":      start(ref.from_jalali(jy, qm - 3, 1)),
        "soq":       start(ref.from_jalali(jy, qm, 1)),
        "sonq":      start(ref.from_jalali(jy, qm + 3, 1)),
        "eopq":      end(ref.from_jalali(jy, qm, 1) - day),
        "eoq":       end(ref.from_jalali(jy, qm + 3, 1) - day),
        "eonq":      end(ref.from_jalali(jy, qm + 6, 1) - day),
        "sopy":      start(ref.from_jalali(jy - 1, 1, 1)),
        "soy":       start(ref.from_jalali(jy, 1, 1)),
        "sony":      start(ref.from_jalali(jy + 1, 1, 1)),
        "eopy":      end(ref.from_jalali(jy, 1, 1) - day),
        "eoy":       end(ref.from_jalali(jy + 1, 1, 1) - day),
        "eony":      end(ref.from_jalali(jy + 2, 1, 1) - day),
    }

    # Ordinals are the next such day of the month.
    for number, word in [(1, "1st"), (15, "15th"), (30, "30th")]:
        month = jm + 1 if number <= jd else jm
        expected[word] = start(ref.from_jalali(jy, month, number))

    # Month names are the start of the next such month.
    for month, word in [(1, "farvardin"), (1, "far"), (6, "shahrivar"), (6, "shah"), (7, "mehr"),
                        (12, "esfand")]:
        expected[word] = start(ref.from_jalali(jy + 1 if jm >= month else jy, month, 1))

    # Day names are the next such day, never today.
    for dow, word in [(6, "shanbe"), (6, "sha"), (0, "yekshanbe"), (3, "chaharshanbe"),
                      (5, "jome"), (5, "friday"), (1, "monday")]:
        delta = dow - tm_wday(today)
        expected[word] = start(today + datetime.timedelta(days=delta + 7 if delta <= 0 else delta))

    # Persian words for nearby days.
    for offset, word in [(-2, "pariruz"), (-1, "diruz"), (0, "emruz"), (1, "farda"),
                         (2, "pasfarda")]:
        expected[word] = start(today + datetime.timedelta(days=offset))

    return expected


def test_named_dates(task):
    """Named dates are relative to today, in Jalali terms."""
    # If midnight passes while task is asked, ask again, on the new day.
    for attempt in range(2):
        today = datetime.date.today()
        got = {word: task.calc(word) for word in named_dates(today)}
        if datetime.date.today() == today:
            break

    for word, value in named_dates(today).items():
        check("Named date '{}'".format(word), got[word], value)


def test_persian_days_in_attributes(task):
    """The Persian day words do not break month names, and work in filters."""
    today = datetime.date.today()
    jy = ref.to_jalali(today)[0]

    # A month name is its next start, which for Farvardin is always next year.
    task.out("add", "project:persian", "Farvardin task", "due:far")
    farvardin = ref.from_jalali(jy + 1, 1, 1)
    check("due:far is the next Farvardin, not farda",
          export_date(task.export("project:persian", "Farvardin")[0]["due"]), farvardin)

    # A project of their own: on the last day of Esfand, Farvardin is farda.
    task.out("add", "project:days", "Today task", "due:emruz")
    task.out("add", "project:days", "Tomorrow task", "due:farda")
    check("due.before:farda in a filter", task.out("project:days", "due.before:farda", "count"), "1")
    check("due:farda in a filter", task.out("project:days", "due:farda", "count"), "1")


def test_display(task):
    task.out("add", "project:display", "due:1405-07-30", "display test")
    uuid = task.export("project:display")[0]["uuid"]

    check("DOM year", task.out("_get", uuid + ".due.year"), "1405")
    check("DOM month", task.out("_get", uuid + ".due.month"), "7")
    check("DOM day", task.out("_get", uuid + ".due.day"), "30")

    report = ["rc.report.jt.columns=due", "rc.report.jt.labels=Due",
              "rc.report.jt.filter=status:pending", "rc.report.jt.sort=due+"]
    for dateformat, value in [("Y-M-D", "1405-07-30"),
                              ("A d B Y", "Panjshanbe 30 Mehr 1405"),
                              ("a D b y", "Pan 30 Meh 05"),
                              ("Y/M/D V", "1405/07/30 31")]:
        out = task.out("rc.dateformat.report=" + dateformat, *report, uuid, "jt")
        check("Date format '{}'".format(dateformat), value in out, True)


def test_calendar(task):
    out = task.out("calendar", "1405")
    check_true("Calendar shows Jalali months", "Farvardin 1405" in out and "Esfand 1405" in out, out[:200])
    check_true("Calendar weeks start on Saturday", "Sh Ye Do Se Ch Pa Jo" in out, out[:200])
    # Farvardin 1st 1405 is a Saturday, so the first row is week 1: 1 to 7.
    check_true("Calendar places days correctly",
               re.search(r"^\s+1\s+1\s+2\s+3\s+4\s+5\s+6\s+7\s", out, re.MULTILINE), out[:400])


def test_recurrence(task):
    def instance_dates(project):
        task.run("list")  # Generates the recurring instances.
        return sorted(jstr(export_date(t["due"]))
                      for t in task.export("project:" + project, "status:pending"))

    task.out("add", "project:monthly", "due:1405-06-31", "recur:monthly", "monthly test")
    dates = instance_dates("monthly")
    check_true("Monthly recurrence clamps to the month length",
               dates[:2] == ["1405-06-31", "1405-07-30"], dates[:4])

    task.out("add", "project:yearly", "due:1403-12-30", "recur:yearly", "yearly test")
    dates = instance_dates("yearly")
    check_true("Yearly recurrence handles Esfand 30th", "1404-12-29" in dates, dates[:4])

    # 1405-07-22 is a Wednesday; Thursday and Friday are skipped.
    task.out("add", "project:weekdays", "due:1405-07-22", "recur:weekdays", "weekdays test")
    dates = instance_dates("weekdays")
    check_true("Weekday recurrence skips Thursday and Friday",
               "1405-07-25" in dates and "1405-07-23" not in dates and "1405-07-24" not in dates,
               dates[:5])


def test_config(task):
    check_true("weekstart defaults to Saturday",
               "weekstart=saturday" in task.out("_show").splitlines())


################################################################################
if __name__ == "__main__":
    sys.exit(run_tests([test_gregorian_to_jalali, test_jalali_to_gregorian, test_validity,
                        test_digits, test_machine_formats, test_named_dates,
                        test_persian_days_in_attributes, test_display,
                        test_calendar, test_recurrence, test_config]))
