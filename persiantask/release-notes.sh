#!/usr/bin/env bash
#
# Prints the CHANGELOG.md section of a version, without its heading: the notes
# of its GitHub release.  Fails if there is no such section.
#
# CHANGELOG.md is wrapped, but GitHub shows every line break of release notes,
# so paragraphs and list items are joined into single lines; code blocks,
# headings, tables and blank lines are kept as they are.  Links to files in
# the repository are made absolute, pointing at the version's tag, since
# release notes are not shown next to the files.
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
printf '%s\n' "$notes" | awk '
  function flush() { if (line != "") print line; line = "" }
  /^[ \t]*```/          { flush(); print; code = !code; next }
  code                  { print; next }
  /^[ \t]*$/            { flush(); print; next }
  /^(#|[ \t]*\|)/        { flush(); print; next }   # Headings and tables.
  /^[ \t]*([-*+]|[0-9]+\.)[ \t]/ { flush(); line = $0; next }  # A list item.
  { text = $0; sub(/^[ \t]+/, "", text); line = (line == "") ? $0 : line " " text }
  END { flush() }' |
  sed -E "s#\]\(([A-Za-z][^):]*)\)#]($files/\1)#g"
