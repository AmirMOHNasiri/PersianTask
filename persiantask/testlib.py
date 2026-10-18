"""Shared helpers for the PersianTask tests.

Each test runs the task binary against its own temporary data directory, so
the tests never touch the user's data.
"""

import datetime
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import jalali_reference as ref  # noqa: E402

# All dates are compared in UTC, so that midnight is the same everywhere.
os.environ["TZ"] = "UTC"
time.tzset()


class Task:
    def __init__(self, binary):
        self.binary = binary
        self.dir = tempfile.mkdtemp(prefix="persiantask-test-")
        self.rc = os.path.join(self.dir, "taskrc")
        with open(self.rc, "w") as rc:
            rc.write("data.location={}\n".format(os.path.join(self.dir, "data")))
            rc.write("confirmation=off\n")
            rc.write("recurrence.confirmation=off\n")
            rc.write("color=off\n")
            rc.write("verbose=nothing\n")
            rc.write("news.version=99.0.0\n")
            rc.write("hooks=off\n")
            rc.write("defaultwidth=200\n")  # So that tables do not wrap.
        # The default configuration includes a theme, which is looked up in the
        # current directory, so task is run from here.
        shutil.copy(os.path.join(HERE, "..", "doc", "rc", "default.theme"), self.dir)
        self.env = dict(os.environ, TASKRC=self.rc, TASKDATA=os.path.join(self.dir, "data"))

    def run(self, *args):
        p = subprocess.run([self.binary] + list(args), env=self.env, cwd=self.dir,
                           stdin=subprocess.DEVNULL, capture_output=True, text=True)
        return p.returncode, p.stdout, p.stderr

    def out(self, *args):
        code, out, err = self.run(*args)
        if code != 0:
            raise AssertionError("task {} failed ({}): {}".format(" ".join(args), code, err.strip()))
        return out.strip()

    def calc(self, expression):
        return self.out("calc", expression)

    def export(self, *filter):
        return json.loads(self.out(*filter, "export"))

    def cleanup(self):
        shutil.rmtree(self.dir, ignore_errors=True)


def jstr(date):
    """Jalali YYYY-MM-DD of a datetime.date."""
    return "{:04d}-{:02d}-{:02d}".format(*ref.to_jalali(date))


def export_date(value):
    """datetime.date of an exported (UTC) date, such as 20261016T000000Z."""
    return datetime.datetime.strptime(value, "%Y%m%dT%H%M%SZ").date()


def tm_wday(date):
    """Day of the week, 0 = Sunday, like tm_wday."""
    return (date.weekday() + 1) % 7


SATURDAY = 6

results = []


def check(name, actual, expected):
    results.append((name, actual == expected, actual, expected))


def check_true(name, condition, detail=""):
    results.append((name, bool(condition), detail, True))



def run_tests(tests, fresh=False):
    """Runs the tests against the binary named on the command line, and prints
    TAP results.  With fresh, each test gets its own data directory."""
    if len(sys.argv) != 2:
        print(sys.modules["__main__"].__doc__.strip())
        return 2

    binary = os.path.abspath(sys.argv[1])
    task = None if fresh else Task(binary)
    try:
        for test in tests:
            current = Task(binary) if fresh else task
            try:
                test(current)
            except Exception as e:
                results.append((test.__name__, False, "exception: {}".format(e), "no exception"))
            finally:
                if fresh:
                    current.cleanup()
    finally:
        if task:
            task.cleanup()

    failed = 0
    print("1..{}".format(len(results)))
    for number, (name, ok, actual, expected) in enumerate(results, 1):
        print("{}ok {} - {}".format("" if ok else "not ", number, name))
        if not ok:
            failed += 1
            print("#   got:      {!r}".format(actual))
            print("#   expected: {!r}".format(expected))

    print("# {} passed, {} failed".format(len(results) - failed, failed))
    return 1 if failed else 0
