#!/usr/bin/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# split-profile.pl Documentation:
# 
# This script uses jprof's includes (-i) and excludes (-e) options to
# split profiles into segments.  It takes as input a single text file,
# and from that text file creates a series of jprof profiles in the
# directory in which it is run.  It expects the application binaries
# with which the profile was made, including jprof, and the jprof
# profile data, to be in a directory called "bin" that is a subdirectory
# of the current directory, and it will output the profiles into the
# current directory.
#
# The input file format looks like the following:
#
#   poll g_main_poll
#   GetRuleCascade CSSRuleProcessor::GetRuleCascade(nsPresContext *, nsIAtom *)
#   RuleProcessorData RuleProcessorData::RuleProcessorData(nsPresContext *, nsIContent *, nsRuleWalker *, nsCompatibility *)
#
# From this input file, the script will construct a profile called
# 00.html that contains the whole profile, a profile called 01-poll.html
# that includes only stacks with g_main_poll, a profile called
# 02-GetRuleCascade.html that includes only stacks that have
# GetRuleCascade and do not have g_main_poll, a profile called
# 03-RuleProcessorData.html that includes only stacks that have the
# RuleProcessorData constructor and do not have GetRuleCascade or
# g_main_poll, and a profile called 04.html that includes only stacks
# that do not have any of the three functions in them.
#
# This means that all of the segments of the profile, except 00.html,
# are mutually exclusive.  Thus clever ordering of the functions in the
# input file can lead to a logical splitting of the profile into
# segments.


use strict;

my @names;
my @sigs;

sub read_info($) {
    my ($fname) = @_;

    open(INFO, "<$fname");
    my $i = 0;
    while (<INFO>) {
        chop;
        my $line = $_;
        my $idx = index($line, " ");
        my $name = substr($line, 0, $idx);
        my $sig = substr($line, $idx+1);

        $names[$i] = $name;
        $sigs[$i] = $sig;
        ++$i;
    }
}

sub run_profile($$) {
    my ($options, $outfile) = @_;

    print  "./jprof$options mozilla-bin jprof-log > ../$outfile.html\n";
    system "./jprof$options mozilla-bin jprof-log > ../$outfile.html";
}

sub run_profiles() {
    run_profile("", "00");

    for (my $i = 0; $i <= $#names + 1; ++$i) {
        my $options = "";
        for (my $j = 0; $j < $i; ++$j) {
            $options .= " -e\"$sigs[$j]\"";
        }
        if ($i <= $#names) {
            $options .= " -i\"$sigs[$i]\"";
        }
        my $num;
        my $n = $i + 1;
        if ($n < 10) {
            $num = "0$n";
        } else {
            $num = "$n";
        }
        if ($i <= $#names) {
            run_profile($options, "$num-$names[$i]");
        } else {
            run_profile($options, "$num");
        }
    }
}

($#ARGV == 0) || die "Usage: split-profile.pl <info-file>\n";

read_info($ARGV[0]);
chdir "bin" || die "Can't change directory to bin.";
run_profiles();
