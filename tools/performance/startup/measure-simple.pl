#!/usr/bin/perl

#
# Script to time mozilla startup
#
# Requirements:
# - a Profile with name "Default User"
# - a MOZ_TIMELINE build
#
# This starts mozilla with the profile "Default User" and makes it quit immediately.
# Measurement is of main1 from MOZ_TIMELINE output
#
# This does multiple runs [default 3] and averages all but the first run.
#
# startup-quick.pl <exe> [n times]
# n - default 3
#

require 5.003;

use strict;
use Cwd;

sub PrintUsage {
  die <<END_USAGE
  usage: measure-simple.pl [n]
  n defaults to 3
  e.g
      measure-simple.pl ../../../dist/bin/mozilla
      measure-simple.pl ../../../dist/bin/mozilla 10
END_USAGE
}

{
  PrintUsage() unless $#ARGV >= 0;
  my $exe = shift @ARGV;
  my $ntimes = shift @ARGV || 3;
  my $timelinelog = "timeline.log";
  # Do one more than what was requested as we ignore the first run timing
  $ntimes++;

  # Build up command string.
  my $cwd = Cwd::getcwd();
  my $cmd = "$exe -P \"Default User\" file:///$cwd/quit.html";
  print "cmd = $cmd\n";
  print "$ntimes times\n";
  
  # Setup run environment
  $ENV{"NS_TIMELINE_LOG_FILE"} = $timelinelog;
  $ENV{"XPCOM_DEBUG_BREAK"} = "warn";
  unlink $timelinelog;

  my $i;
  my @runtimes = ();
  for($i = 0; $i < $ntimes; $i++) {
      # Run the command.
      system($cmd);

      # find the time taken from the TIMELINE LOG
      my $F;
      open(F, "< $timelinelog") || die "no timeline log ($timelinelog) found";
      while(<F>) {
          if (/^(.*): \.\.\.main1$/) {
              my $t = $1 + 0;
              push @runtimes, $t;
              print "[$i] startup time : $t sec\n";
              last;
          }
      }
      close(F);
      # Cleanup
      unlink $timelinelog;
  }

  # Compute final number. Skip first run and average the rest.
  my $sum = 0;
  shift @runtimes;
  foreach $i (@runtimes) {
      $sum += $i;
  }
  printf "Average startup time : %8.4f secs (%d trials - %s)\n", $sum/($ntimes-1), $ntimes-1, join(" ", @runtimes);
}

