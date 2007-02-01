#!/usr/bin/perl -w
use strict;
use Bootstrap::Step;
use t::Bootstrap::Step::Dummy;
use t::Bootstrap::Step::Tag;

my $step = t::Bootstrap::Step::Dummy->new();

$step->Execute();
$step->Verify();
$step->Push();
$step->Announce();

#$step = t::Bootstrap::Step::Tag->new();

#$step->Execute();
