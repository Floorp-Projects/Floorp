#!/bin/sh

export VERBOSE=0
PART1='use Test::Harness qw(&runtests $verbose); $verbose='
PART2='; runtests @ARGV;'
for f in $*; do
  if [ $f == "--verbose" ] ; then
    export VERBOSE=1
  fi
done

/usr/bonsaitools/bin/perl -e "${PART1}${VERBOSE}${PART2}" t/*.t
