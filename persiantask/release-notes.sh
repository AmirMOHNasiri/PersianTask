#!/usr/bin/env bash
#
# Prints the CHANGELOG.md section of a version, without its heading: the notes
# of its GitHub release.  Links to files in the repository are made absolute,
# pointing at the version's tag, since release notes are not shown next to
# the files.  Fails if there is no such section.
#
# Usage: persiantask/release-notes.sh 3.5.0-persiantask.1

set -euo pipefail

version=${1:?Usage: release-notes.sh <version>}
changelog="$(dirname "$0")/../CHANGELOG.md"

# The lines after '## <version> (<date>)', up to the next '## ' heading,
# without the blank lines around them.
notes=$(awk -v version="$version" '
  /^## / { if (found) exit; found = ($2 == version); next }
  found' "$changelog" | sed -e '/./,$!d' | tac | sed -e '/./,$!d' | tac)

if [ -z "$notes" ]; then
  echo "CHANGELOG.md has no section for $version." >&2
  exit 1
fi
files="https://github.com/AmirMOHNasiri/PersianTask/blob/v$version"
printf '%s\n' "$notes" | sed -E "s#\]\(([A-Za-z][^):]*)\)#]($files/\1)#g"
