#!/usr/bin/env bash

set -xeu
cd "$(dirname "$0")/.."

git add -u
git diff @
git diff-index --quiet HEAD
