#!/usr/bin/perl

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

require 5.003;

require "gettime.pl";

use strict;
use Cwd;

sub PrintUsage {
  die <<END_USAGE
  usage: startup-unix.pl <exe>
END_USAGE
}

{
  PrintUsage() if $#ARGV != 0;

  # Build up command string.
  my $cwd = Cwd::getcwd();
  my $time = Time::PossiblyHiRes::getTime();
  my $cmd = "@ARGV[0] \"file:$cwd/startup-test.html?begin=" . $time . "\"";	
  print "cmd = $cmd\n";
  
  # Run the command.
  exec $cmd;
}

