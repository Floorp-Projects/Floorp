#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


#
# Script to time mozilla startup.
# Feeds in start time as url argument, startup-test.html
# takes this arg and computes the time difference.
# So something like this happens:
#
# mozilla file:/foo/startup-test.html?begin=T
#   where T = ms since 1970, e.g.:
# mozilla file:/foo/startup-test.html?begin=999456977124
# 
# NOTE: You will get much better results if you install the
# Time::HiRes perl module (urls in gettime.pl) and test
# an optimized build.
#
# For optimized builds, startup-test.html will also dump
# the time out to stdout if you set:
#   user_pref("browser.dom.window.dump.enabled", true);
#

require 5.003;

require "gettime.pl";

use strict;
use Cwd;

sub PrintUsage {
  die <<END_USAGE
  usage: startup-unix.pl <exe> [<args>]
  e.g
      startup-unix.pl ../../../dist/bin/mozilla
      startup-unix.pl ../../../dist/bin/mozilla -P \"Default User\"
END_USAGE
}

{
  PrintUsage() unless $#ARGV >= 0;
  # Build up command string.
  my $cwd = Cwd::getcwd();

  my $cmd = join(' ', map {/\s/ ? qq("$_") : $_} @ARGV);
  my $time = Time::PossiblyHiRes::getTime();
  $cmd .= qq( -url "file:$cwd/startup-test.html?begin=$time");
  print "cmd = $cmd\n";

  # Run the command.
  exec $cmd;
}

