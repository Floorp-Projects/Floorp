#!/bin/bash
# Usage: sh update.sh <upstream_src_directory>

set -e

[[ -n "$1" ]] || ( echo "syntax: $0 update_src_directory"; exit 1 )
[[ -e "$1/src/nestegg.c" ]] || ( echo "$1: nestegg not found"; exit 1 )

cp $1/include/nestegg/nestegg.h include
cp $1/src/nestegg.c src
cp $1/LICENSE .
cp $1/README.md .
cp $1/AUTHORS .

if [ -d $1/.git ]; then
  rev=$(cd $1 && git rev-parse --verify HEAD)
  date=$(cd $1 && git show -s --format=%ci HEAD)
  dirty=$(cd $1 && git diff-index --name-only HEAD)
  set +e
  pre_rev=$(grep -o '[[:xdigit:]]\{40\}' moz.yaml)
  commits=$(cd $1 && git log --pretty=format:'%h - %s' $pre_rev..$rev)
  set -e
fi

if [ -n "$rev" ]; then
  version=$rev
  if [ -n "$dirty" ]; then
    version=$version-dirty
    echo "WARNING: updating from a dirty git repository."
  fi
  sed -i.bak -e "s/^ *release:.*/  release: \"$version ($date)\"/" moz.yaml
  if [[ ! "$( grep "$version" moz.yaml )" ]]; then
    echo "Updating moz.yaml failed."
    exit 1
  fi
  rm moz.yaml.bak
  [[ -n "$commits" ]] && echo -e "Pick commits:\n$commits"
else
  echo "Remember to update moz.yaml with the version details."
fi
