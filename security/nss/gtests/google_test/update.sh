#!/bin/bash
# Use this script to update the copy of google test.
# This won't commit any changes, so build and test afterwards.

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <tag/commit>" 1>&2
    exit 2
fi

cd "$(dirname "$0")"
d=$(mktemp -d)
trap 'rm -rf "$d"' EXIT
../../fuzz/config/git-copy.sh https://github.com/google/googletest \
    "$1" "$d"/googletest
rm -rf gtest
mv "$d"/googletest/googletest gtest
echo "$1" > VERSION
cat "$d"/googletest/.git-copy >> VERSION
