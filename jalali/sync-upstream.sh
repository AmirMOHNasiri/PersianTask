#!/usr/bin/env bash
#
# Merges upstream Taskwarrior into the current branch.
#
# Usage: jalali/sync-upstream.sh [upstream-ref]     (default: upstream/develop)
#
# Upstream keeps src/libshared as a git submodule, while PersianTask vendors it
# (the Jalali calendar lives in src/libshared/src/Datetime.cpp).  So when
# upstream moves the submodule to a new libshared commit, the same change is
# applied to the vendored copy as a patch, and the submodule side of the merge
# is dropped.
#
# Exits non-zero, leaving the repository mid-merge, if anything conflicts.

set -euo pipefail

UPSTREAM=${1:-upstream/develop}
LIBSHARED_URL=https://github.com/GothenburgBitFactory/libshared.git

if git merge-base --is-ancestor "$UPSTREAM" HEAD; then
  echo "Already up to date with $UPSTREAM."
  exit 0
fi

base=$(git merge-base HEAD "$UPSTREAM")
old=$(git rev-parse "$base:src/libshared")
new=$(git rev-parse "$UPSTREAM:src/libshared")

if [ "$old" != "$new" ]; then
  echo "Upstream updated libshared from ${old:0:7} to ${new:0:7}."
  work=$(mktemp -d)
  trap 'rm -rf "$work"' EXIT
  git clone --quiet --filter=blob:none "$LIBSHARED_URL" "$work/libshared"
  git -C "$work/libshared" diff --binary "$old" "$new" > "$work/libshared.patch"

  if ! git apply --index --directory=src/libshared "$work/libshared.patch"; then
    echo "The libshared update does not apply cleanly to the vendored copy." >&2
    exit 1
  fi

  git commit --quiet -m "Update vendored libshared from ${old:0:7} to ${new:0:7}"
fi

if ! git merge --no-ff --no-edit "$UPSTREAM"; then
  # The libshared submodule always conflicts with the vendored copy, when
  # upstream moved it.  That was handled above, so drop the submodule.  Git
  # may have renamed it to src/libshared~<branch>, to make way for the
  # vendored directory.
  for path in $(git ls-files --unmerged | awk '$1 == "160000" { print $4 }' | sort -u); do
    case "$path" in
      src/libshared)
        git update-index --force-remove "$path"
        ;;
      src/libshared~*)
        git update-index --force-remove "$path"
        rm -rf "$path"
        ;;
    esac
  done

  conflicts=$(git diff --name-only --diff-filter=U)
  if [ -n "$conflicts" ]; then
    echo "Merge conflicts:" >&2
    echo "$conflicts" >&2
    exit 1
  fi

  git commit --quiet --no-edit
fi

echo "Merged $UPSTREAM."
