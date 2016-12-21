#!/bin/sh

d=$(dirname $0)
exec $d/git-copy.sh https://github.com/mozilla/nss-fuzzing-corpus master $d/corpus
