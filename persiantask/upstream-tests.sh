#!/usr/bin/env bash
#
# Runs upstream Taskwarrior's own test suite (test/) against this build,
# with calendar=gregorian and dayname.style=english, to show that the
# Gregorian calendar behaves exactly as upstream.
#
# Usage: persiantask/upstream-tests.sh [extra taskrc settings...]
#
# The extra settings, such as weekstart=sunday, are added to the taskrc of
# every test.  Failures listed in persiantask/upstream-known-failures, with
# their reasons, are expected; any other failure fails this script, and so
# does a listed failure that now passes, so that the list stays accurate.

set -euo pipefail

cd "$(dirname "$0")/.."
root=$(pwd)

# A Debug build, as upstream's CI uses: only Debug builds point the default
# theme at the source tree (TASK_TEST_RCDIR), which the tests need.
build=build-upstream
cmake -S . -B $build -DCMAKE_BUILD_TYPE=Debug > /dev/null
cmake --build $build --target test_runner --target task_executable -j"$(nproc)" > /dev/null

# The tests write their own taskrc.  The settings are added to the build
# tree's copy of basetest/task.py, so that upstream's files stay untouched.
python3 - $build/test/basetest/task.py << 'EOF'
import sys
path = sys.argv[1]
source = open(path).read()
marker = "# PersianTask: extra settings"
if marker not in source:
    anchor = '                "".format(self.datadir)\n            )\n'
    assert anchor in source, "upstream's basetest/task.py changed; update upstream-tests.sh"
    source = source.replace(anchor, anchor + (
        "            " + marker + ", from upstream-tests.sh.\n"
        "            for setting in os.environ.get('PERSIANTASK_TEST_RC', '').split(';'):\n"
        "                if setting:\n"
        "                    rc.write(setting + '\\n')\n"), 1)
    open(path, "w").write(source)
EOF

settings="calendar=gregorian;dayname.style=english"
for extra in "$@"; do settings="$settings;$extra"; done
echo "Upstream test suite, with: ${settings//;/, }"

# The C++ tests use the libshared defaults, which are upstream's.
log=$(mktemp)
trap 'rm -f "$log"' EXIT
PERSIANTASK_TEST_RC="$settings" ctest --test-dir $build -j "$(nproc)" > "$log" 2>&1 || true
grep -E "tests passed|tests failed" "$log"

failed=$(grep -E "^\s+[0-9]+ - .* \((Failed|SEGFAULT|Timeout|Not Run|Subprocess aborted)\)" "$log" |
         sed -E 's/^\s+[0-9]+ - (\S+) .*/\1/' | sort)
list="$root/persiantask/upstream-known-failures"
known=$(awk '!/^#/ && NF && $2 !~ /^flaky/ {print $1}' "$list" | sort)
flaky=$(awk '!/^#/ && NF && $2 ~ /^flaky/ {print $1}' "$list" | sort)

# Flaky tests may fail or pass.
failed=$(comm -23 <(echo "$failed") <(echo "$flaky") | grep . || true)
unexpected=$(comm -23 <(echo "$failed") <(echo "$known") | grep . || true)
fixed=$(comm -13 <(echo "$failed") <(echo "$known") | grep . || true)

if [ -n "$failed" ]; then
  echo "Failed, as expected (see persiantask/upstream-known-failures):"
  comm -12 <(echo "$failed") <(echo "$known") | sed 's/^/  /'
fi
if [ -n "$fixed" ]; then
  echo "Listed as failing, but passed: remove them from persiantask/upstream-known-failures:"
  echo "$fixed" | sed 's/^/  /'
fi
if [ -n "$unexpected" ]; then
  echo "Unexpected failures:"
  echo "$unexpected" | sed 's/^/  /'
fi
[ -z "$unexpected" ] && [ -z "$fixed" ]
