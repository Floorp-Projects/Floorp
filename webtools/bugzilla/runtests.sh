#!/bin/sh

export TEST_VERBOSE=0
PART1='use Test::Harness qw(&runtests $verbose); $verbose='
PART2='; runtests @ARGV;'
for f in $*; do
  if [ $f == "--verbose" ] ; then
    export TEST_VERBOSE=1
  fi
done

/usr/bonsaitools/bin/perl -e "${PART1}${TEST_VERBOSE}${PART2}" t/*.t
