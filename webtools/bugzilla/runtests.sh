#!/bin/sh

/usr/bonsaitools/bin/perl -e 'use Test::Harness qw(&runtests $verbose); $verbose=0; runtests @ARGV;' t/*.t
