#!/usr/bin/env bash
#
# Runs PersianTask's own tests with the real clock moved to other dates, with
# libfaketime, to catch tests that pass only around the date they were written
# (time bombs), such as a task due 'now+2d' in a test that pins the
# schedule's time.
#
# Usage: persiantask/faketime-tests.sh <task binary> <date>...
#
# Dates are anything 'date -d' reads: '2027-03-21', '+1 year', 'next month'.
# The clock is moved to noon of each date, and keeps ticking from there.
# libfaketime is found in its usual places (Arch: libfaketime, Debian and
# Ubuntu: faketime), or set FAKETIME_LIB to its libfaketime.so.1.

set -euo pipefail

task=$(realpath "${1:?Usage: faketime-tests.sh <task binary> <date>...}")
shift
cd "$(dirname "$0")"

lib=${FAKETIME_LIB:-}
for candidate in /usr/lib/faketime/libfaketime.so.1 \
                 /usr/lib/x86_64-linux-gnu/faketime/libfaketime.so.1; do
  [ -n "$lib" ] || [ ! -f "$candidate" ] || lib=$candidate
done
[ -f "$lib" ] || { echo "libfaketime not found: install it, or set FAKETIME_LIB." >&2; exit 1; }

log=$(mktemp)
trap 'rm -f "$log"' EXIT
failed=()
for when in "$@"; do
  day=$(date -d "$when" +%F)
  offset=$(( $(date -d "$day 12:00:00" +%s) - $(date +%s) ))
  for suite in test_jalali.py test_schedule.py test_calendar.py; do
    echo "== $suite on $day ($when)"
    # A relative offset, so that time passes normally, also between the many
    # task processes of a suite.
    status=0
    LD_PRELOAD="$lib" FAKETIME="${offset/#[0-9]/+&}" FAKETIME_DONT_FAKE_MONOTONIC=1 \
      python3 "$suite" "$task" > "$log" 2>&1 || status=$?
    grep -v '^ok' "$log" || true
    [ "$status" -eq 0 ] || failed+=("$suite on $day")
  done
done

if [ ${#failed[@]} -gt 0 ]; then
  printf 'Failed: %s\n' "${failed[@]}"
  exit 1
fi
echo "All suites passed on every date."
