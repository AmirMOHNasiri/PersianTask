#!/usr/bin/env bash
#
# Builds the 'persiantask' Arch Linux package from the current commit, into
# jalali/out.  This is the prebuilt package published in the pacman
# repository, made from pkg/arch/PKGBUILD (the 'persiantask-git' AUR package),
# but built from this checkout instead of the latest commit on GitHub.
#
# Usage: jalali/build-package.sh     (as a regular user, not root)

set -euo pipefail

repo=$(cd "$(dirname "$0")/.." && pwd)
out="$repo/jalali/out"
commit=$(git -C "$repo" rev-parse HEAD)

# Not in /tmp, which is often a small tmpfs.
mkdir -p "$out"
work=$(mktemp -d "$out/.build.XXXXXX")
trap 'rm -rf "$work"' EXIT

sed -e "s/^pkgname=.*/pkgname=persiantask/" \
    -e "s/^conflicts=.*/conflicts=('task' 'persiantask-git')/" \
    -e "s|git+https://github.com/AmirMOHNasiri/PersianTask.git#branch=main|git+file://$repo#commit=$commit|" \
    "$repo/pkg/arch/PKGBUILD" > "$work/PKGBUILD"

# Fail loudly if pkg/arch/PKGBUILD changed in a way that the above misses.
for expected in "^pkgname=persiantask$" "'persiantask-git'" "git+file://"; do
  if ! grep -q "$expected" "$work/PKGBUILD"; then
    echo "Could not adapt pkg/arch/PKGBUILD (missing $expected)." >&2
    exit 1
  fi
done

cd "$work"
makepkg --noconfirm

# makepkg does not fail on every packaging error (such as running out of
# space while stripping), so check that the packaged task actually runs.
pkg=$(ls persiantask-r*-x86_64.pkg.tar.zst)
bsdtar -xf "$pkg" usr/bin/task
if ! version=$(./usr/bin/task --version); then
  echo "The task binary in $pkg does not run." >&2
  exit 1
fi
echo "Packaged task $version."

rm -f "$out"/*.pkg.tar.zst
mv "$pkg" "$out/"
ls "$out"
