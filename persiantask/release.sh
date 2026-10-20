#!/usr/bin/env bash
#
# Releases the current commit of main as the version in CMakeLists.txt, such
# as 3.5.0-persiantask.1: upstream's version, then PERSIANTASK_RELEASE.
#
# It checks that CHANGELOG.md has a section for the version, runs all the
# tests, including upstream's, and only if they pass, makes the annotated tag
# v<version> and pushes it together with main.  The Package workflow then
# builds and publishes the package of that version, and creates the GitHub
# release, with the changelog section as its notes.
#
# Usage: persiantask/release.sh

set -euo pipefail

cd "$(dirname "$0")/.."

upstream=$(sed -nE 's/^  VERSION ([0-9]+\.[0-9]+\.[0-9]+)$/\1/p' CMakeLists.txt)
release=$(sed -nE 's/^set \(PERSIANTASK_RELEASE ([0-9]+)\)$/\1/p' CMakeLists.txt)
version="$upstream-persiantask.$release"
tag="v$version"

fail() { echo "$*" >&2; exit 1; }
[ "$(git branch --show-current)" = main ] || fail "Release from main."
[ -z "$(git status --porcelain)" ] || fail "Commit or stash your changes first."
! git rev-parse -q --verify "refs/tags/$tag" > /dev/null ||
  fail "$tag exists already: raise PERSIANTASK_RELEASE in CMakeLists.txt."
bash persiantask/release-notes.sh "$version" > /dev/null

echo "Releasing $version."
PERSIANTASK_UPSTREAM_TESTS=1 bash persiantask/build-and-test.sh &&
  [ "$(build/src/task --version)" = "$version" ] &&
  git tag -a "$tag" -m "PersianTask $version" &&
  git push --atomic origin main "$tag"
echo "Pushed $tag.  The Package workflow publishes it, and creates the release."
