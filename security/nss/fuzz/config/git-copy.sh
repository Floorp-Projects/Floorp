#!/usr/bin/env bash

set -e

if [ $# -lt 3 ]; then
  echo "Usage: $0 <repo> <branch> <directory>" 1>&2
  exit 2
fi

REPO="$1"
COMMIT="$2"
DIR="$3"

echo "Copy '$COMMIT' from '$REPO' to '$DIR'"
if [ -f "$DIR"/.git-copy ]; then
  CURRENT=$(cat "$DIR"/.git-copy)
  if [ $(echo -n "$COMMIT" | wc -c) != "40" ]; then
    # On the off chance that $COMMIT is a remote head.
    ACTUAL=$(git ls-remote "$REPO" "$COMMIT" | cut -c 1-40 -)
  else
    ACTUAL="$COMMIT"
  fi
  if [ "$CURRENT" = "$ACTUAL" ]; then
    echo "Up to date."
    exit
  fi
fi

rm -rf "$DIR"
git init -q "$DIR"
git -C "$DIR" fetch -q --depth=1 "$REPO" "$COMMIT":git-copy-tmp
git -C "$DIR" reset --hard git-copy-tmp
git -C "$DIR" rev-parse --verify HEAD > "$DIR"/.git-copy
rm -rf "$DIR"/.git
