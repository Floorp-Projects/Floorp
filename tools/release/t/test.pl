#!/usr/bin/perl -w
use strict;
use Bootstrap::Step;
use Bootstrap::Config;
use t::Bootstrap::Step::Dummy;

my $config = new Bootstrap::Config();
unless ($config->Exists(sysvar => 'buildDir')) {
  print "FAIL: buildDir should exist\n";
}
unless ($config->Get(sysvar => 'buildDir')) {
  print "FAIL: buildDir should be retrievable\n";
}
my $sysname = $config->SystemInfo(var => 'sysname');
unless ($sysname) {
  print "FAIL: sysname should exist\n";
} 

my $step = t::Bootstrap::Step::Dummy->new();

$step->Execute();
$step->Verify();
$step->Push();
$step->Announce();

