#!/bin/sh

TEST_VERBOSE=0
for f in $*; do
  if [ "$f" = "--verbose" ] ; then
    TEST_VERBOSE="--verbose"
  fi
done

/usr/bonsaitools/bin/perl runtests.pl ${TEST_VERBOSE}
