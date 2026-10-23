#!/usr/bin/env bash
#
# Builds task in ./build, and runs the PersianTask tests against it.
#
# With PERSIANTASK_FAKETIME_TESTS=1, they run again with the clock a year
# ahead (see faketime-tests.sh, which needs libfaketime).  With
# PERSIANTASK_UPSTREAM_TESTS=1, upstream Taskwarrior's own tests run too (see
# upstream-tests.sh), which take several more minutes.
#
# Usage: persiantask/build-and-test.sh
#        PERSIANTASK_FAKETIME_TESTS=1 PERSIANTASK_UPSTREAM_TESTS=1 persiantask/build-and-test.sh

set -euo pipefail

cd "$(dirname "$0")/.."

# A test build: with the test-only switches, which the tests then use.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPERSIANTASK_TESTING=ON
export PERSIANTASK_TESTING=1
cmake --build build --target task_executable -j"$(nproc)"
python3 persiantask/test_jalali.py build/src/task
python3 persiantask/test_schedule.py build/src/task
python3 persiantask/test_calendar.py build/src/task

# The same tests a year from now, to catch tests that pass only around the
# date they were written.  In the upstream sync, and on a manual run of the
# Tests workflow.
if [ "${PERSIANTASK_FAKETIME_TESTS:-}" = "1" ]; then
  bash persiantask/faketime-tests.sh build/src/task "+1 year"
fi

# Upstream Taskwarrior's own tests, with calendar=gregorian, in a Debug build
# of their own.  They run in the upstream sync, on a manual run of the Tests
# workflow, and before a release (release.sh).
if [ "${PERSIANTASK_UPSTREAM_TESTS:-}" = "1" ]; then
  bash persiantask/upstream-tests.sh
fi
