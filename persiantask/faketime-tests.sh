#!/usr/bin/env bash
#
# Runs PersianTask's own tests with the real clock moved to other dates, with
# libfaketime, to catch tests that pass only around the date they were written
# (time bombs), such as a task due 'now+2d' in a test that pins the
# schedule's time.
#
# Usage: persiantask/faketime-tests.sh <task binary> [<when>...]
#
# Each <when> is anything 'date -d' reads, as UTC, the time zone of the tests:
# '2027-03-21', '+1 year', or with a time of day, '2027-03-20 23:59:30'.
# Without a time of day, the clock is set to noon.  Every suite starts at that
# moment, and the clock keeps ticking from there, so a time just before
# midnight crosses into the next day while the suite runs.
#
# Without any <when>, the dates and times below are used.
#
# libfaketime is found in its usual places (Arch: libfaketime, Debian and
# Ubuntu: libfaketime), or set FAKETIME_LIB to its libfaketime.so.1.

set -euo pipefail

# Days and times worth checking.  Around midnight, the starts just before it
# make midnight fall inside each suite, which take about 10-25 (jalali), 3-8
# (schedule) and 1-3 (calendar) seconds, depending on the machine.
DEFAULT=(
  "+1 week" "+3 months" "+1 year" "+2 years"
  # Nowruz 1406: the last day of Esfand 1405, and Farvardin 1.
  "2027-03-20" "2027-03-21"
  # The end of the next leap year: Esfand 30, 1408.
  "2030-03-20"
)
# Midnight on an ordinary day, on Nowruz 1406 eve, and from Esfand 30, 1408
# into Farvardin 1, 1409.
for day in 2026-11-10 2027-03-20 2030-03-20; do
  next=$(date -d "$day + 1 day" +%F)
  DEFAULT+=("$day 23:59:30" "$day 23:59:52" "$day 23:59:57" "$day 23:59:59" "$next 00:00:30")
done

task=$(realpath "${1:?Usage: faketime-tests.sh <task binary> [<when>...]}")
shift
[ $# -gt 0 ] || set -- "${DEFAULT[@]}"
cd "$(dirname "$0")"
export TZ=UTC

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
  # A time of day, if given; otherwise noon.
  if [[ "$when" == *:* ]]; then
    moment=$(date -d "$when" '+%F %T')
  else
    moment="$(date -d "$when" +%F) 12:00:00"
  fi
  for suite in test_jalali.py test_schedule.py test_calendar.py; do
    echo "== $suite at $moment UTC ($when)"
    # A relative offset, so that time passes normally, also between the many
    # task processes of a suite.
    offset=$(( $(date -d "$moment" +%s) - $(date +%s) ))
    status=0
    LD_PRELOAD="$lib" FAKETIME="${offset/#[0-9]/+&}" FAKETIME_DONT_FAKE_MONOTONIC=1 \
      python3 "$suite" "$task" > "$log" 2>&1 || status=$?
    grep -v '^ok' "$log" || true
    [ "$status" -eq 0 ] || failed+=("$suite at $moment UTC")
  done
done

if [ ${#failed[@]} -gt 0 ]; then
  printf 'Failed: %s\n' "${failed[@]}"
  exit 1
fi
echo "All suites passed at every time."
