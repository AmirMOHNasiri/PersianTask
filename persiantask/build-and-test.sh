#!/usr/bin/env bash
#
# Builds task in ./build, and runs the PersianTask tests against it, and then
# upstream Taskwarrior's own tests (see upstream-tests.sh).
#
# Usage: persiantask/build-and-test.sh

set -euo pipefail

cd "$(dirname "$0")/.."

# A test build: with the test-only switches, which the tests then use.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPERSIANTASK_TESTING=ON
export PERSIANTASK_TESTING=1
cmake --build build --target task_executable -j"$(nproc)"
python3 persiantask/test_jalali.py build/src/task
python3 persiantask/test_schedule.py build/src/task
python3 persiantask/test_calendar.py build/src/task

# Upstream Taskwarrior's own tests, with calendar=gregorian, in a Debug build
# of their own.  PERSIANTASK_SKIP_UPSTREAM_TESTS=1 skips them, for a quick run.
if [ "${PERSIANTASK_SKIP_UPSTREAM_TESTS:-}" != "1" ]; then
  bash persiantask/upstream-tests.sh
fi
