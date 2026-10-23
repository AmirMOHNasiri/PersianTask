#!/usr/bin/env python3
"""Times the schedule with a realistic dataset: the example template (45
slots a week), and 200 pending tasks across its categories, many with due
dates up to 26 weeks away (the planning horizon) and estimates.

Usage: python3 persiantask/perf_schedule.py path/to/task [more/task ...] [--runs N]

Prints the median wall time of plain 'task', 'task schedule week' and
'task schedule week next' for each binary, with the same data.
"""

import datetime
import json
import os
import random
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

PROJECTS = ["research", "ds", "os", "algo", "net", "db", "arch", "uptodate", "review", "misc",
            "home", "work"]
COMMANDS = [("task", []), ("schedule week", ["schedule", "week"]),
            ("schedule week next", ["schedule", "week", "next"])]


def dataset(count=200, seed=1405):
    random.seed(seed)
    now = datetime.datetime.now(datetime.timezone.utc)
    tasks = []
    for i in range(count):
        task = {
            "uuid": "00000000-0000-4000-8000-{:012d}".format(i),
            "description": "Task {}".format(i),
            "status": "pending",
            "entry": (now - datetime.timedelta(days=random.randint(0, 60))).strftime("%Y%m%dT%H%M%SZ"),
            "project": random.choice(PROJECTS),
        }
        if random.random() < 0.6:
            due = now + datetime.timedelta(days=random.randint(-3, 182), hours=random.randint(0, 23))
            task["due"] = due.strftime("%Y%m%dT%H%M%SZ")
        if random.random() < 0.5:
            task["estimate"] = "PT{}H".format(random.randint(1, 20))
        if random.random() < 0.2:
            task["priority"] = random.choice(["H", "M", "L"])
        if random.random() < 0.1:
            task["tags"] = ["term"]
        tasks.append(task)
    return tasks


def setup(directory):
    data = os.path.join(directory, "data")
    os.makedirs(data)
    rc = os.path.join(directory, "taskrc")
    with open(rc, "w") as f:
        f.write("data.location={}\nconfirmation=off\ncolor=off\nverbose=nothing\n"
                "news.version=99.0.0\nhooks=off\n".format(data))
    shutil.copy(os.path.join(ROOT, "doc", "rc", "default.theme"), directory)
    shutil.copy(os.path.join(ROOT, "examples", "schedule", "student-week"),
                os.path.join(data, "schedule"))
    return dict(os.environ, TASKRC=rc, TASKDATA=data)


def main():
    args = sys.argv[1:]
    runs = 7
    if "--runs" in args:
        i = args.index("--runs")
        runs = int(args[i + 1])
        del args[i:i + 2]
    binaries = [os.path.abspath(b) for b in args]
    if not binaries:
        sys.exit(__doc__)

    directory = tempfile.mkdtemp(prefix="persiantask-perf-")
    try:
        env = setup(directory)
        with open(os.path.join(directory, "tasks.json"), "w") as f:
            json.dump(dataset(), f)
        subprocess.run([binaries[0], "import", os.path.join(directory, "tasks.json")], env=env,
                       cwd=directory, check=True, capture_output=True)

        print("{:<24}".format("") + "".join("{:>22}".format(name) for name, _ in COMMANDS))
        for binary in binaries:
            row = "{:<24}".format(os.path.basename(binary)[:23])
            for name, command in COMMANDS:
                times = []
                for _ in range(runs + 1):
                    start = time.perf_counter()
                    subprocess.run([binary] + command, env=env, cwd=directory, check=True,
                                   capture_output=True)
                    times.append(time.perf_counter() - start)
                # The first run warms the caches.
                row += "{:>19.0f} ms".format(statistics.median(times[1:]) * 1000)
            print(row)
    finally:
        shutil.rmtree(directory, ignore_errors=True)


if __name__ == "__main__":
    main()
