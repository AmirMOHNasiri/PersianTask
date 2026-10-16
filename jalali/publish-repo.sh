#!/usr/bin/env bash
#
# Publishes the package in jalali/out as the 'persiantask' pacman repository,
# hosted as the assets of the 'repo' GitHub release.  Users add:
#
#   [persiantask]
#   Server = https://github.com/<owner>/<repo>/releases/download/repo
#
# If GPG_PRIVATE_KEY is set, the package and the database are signed with it.
#
# Usage: jalali/publish-repo.sh     (with GH_TOKEN and GH_REPO set, for gh)

set -euo pipefail

TAG=repo
cd "$(dirname "$0")/out"

pkg=$(ls persiantask-r*-x86_64.pkg.tar.zst)
files=("$pkg")
sign=()

if [ -n "${GPG_PRIVATE_KEY:-}" ]; then
  echo "$GPG_PRIVATE_KEY" | gpg --batch --import
  gpg --batch --yes --detach-sign --no-armor "$pkg"
  files+=("$pkg.sig")
  sign=(--sign)
fi

# The database only lists this package, which replaces any previous version.
repo-add "${sign[@]}" persiantask.db.tar.gz "$pkg"

# repo-add makes persiantask.db and persiantask.files symlinks, but release
# assets must be regular files.
for name in persiantask.db persiantask.files; do
  rm "$name"
  cp "$name.tar.gz" "$name"
  files+=("$name")
  if [ -f "$name.tar.gz.sig" ]; then
    cp "$name.tar.gz.sig" "$name.sig"
    files+=("$name.sig")
  fi
done

if ! gh release view "$TAG" > /dev/null 2>&1; then
  gh release create "$TAG" --title "Pacman repository" --latest=false \
    --notes "The PersianTask pacman repository. See the README for how to use it."
fi

old=$(gh release view "$TAG" --json assets --jq '.assets[].name')

# Upload the new files first, so the repository is never left without the
# package that its database lists.
gh release upload "$TAG" "${files[@]}" --clobber

for asset in $old; do
  if ! printf '%s\n' "${files[@]}" | grep -qxF "$asset"; then
    gh release delete-asset "$TAG" "$asset" --yes
  fi
done

echo "Published $pkg."
