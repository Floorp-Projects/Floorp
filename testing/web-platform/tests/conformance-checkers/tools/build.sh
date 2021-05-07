#!/usr/bin/env sh
set -ex

cd "${0%/*}"
for script in *.py; do
    python2.7 "$script"
done
