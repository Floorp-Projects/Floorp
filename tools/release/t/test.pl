#!/usr/bin/perl -w
use strict;
use Bootstrap::Step;
use t::Bootstrap::Step::Dummy;

my $step = t::Bootstrap::Step::Dummy->new();

$step->Execute();
$step->Verify();
