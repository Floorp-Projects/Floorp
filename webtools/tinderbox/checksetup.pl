#!/usr/bonsaitools/bin/perl
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Christopher Seawood are
# Copyright (C) 2006 Christopher Seawood.  All Rights Reserved.
#
# Contributor(s): 

require "tbglobals.pl";
use strict;
use Getopt::Std;
use Tie::IxHash;
my $verbose = 0;
my $write_changes = 0;

sub usage() {
    print "Usage: $0 [-h] [-v] [-w]\n";
    print "\t-h\tHelp\n";
    print "\t-v\tVerbose\n";
    print "\t-w\tWrite live changes\n";
    exit(1);
}

sub validate_treedata() {
    my @treelist = &make_tree_list();
    tie my %treedata => 'Tie::IxHash';
    print "Trees: @treelist\n";
    for my $treedir (@treelist) {
	my $file = "$treedir/treedata.pl";
	my $changedvalues = 0;
	print "Tree: $treedir\n" if ($verbose);
	open(CONFIG, "$file") or die ("$file: $!\n");
	# Copy hash to set structure & unset each member
	undef %treedata;
	%treedata = %::default_treedata;
	for my $var (keys %treedata) {
	    $treedata{$var} = undef;
	}
	# Read variable assignments into treedata hash
	while (my $line = <CONFIG>) {
	    chomp($line);
	    if ($line =~ m/^\$[\w]+\s*=.*;$/) {
		$line =~ s/^\$([\w]+)=(.*);$/\$treedata{\1}=\2;/;
		eval($line);
	    } elsif ($line =~ m/^1\;$/) {
		next;
	    } else {
		print "Ignoring: $file: $line\n";
	    }
	}
	close(CONFIG);
	# Skip tree if treedata_version is current
	if (defined($treedata{treedata_version}) &&
	    $treedata{treedata_version} == $::default_treedata{treedata_version}) {
	    print "\t$treedir is current\n" if ($verbose);
	    next;
	}
	# Fill in missing gaps
	for my $var (keys %treedata) {
	    if (!defined($treedata{$var})) {
		print "\tSetting $treedir :: $var to $::default_treedata{$var}\n" if ($verbose);
		$treedata{$var} = $::default_treedata{$var};
		$changedvalues++;
	    }
	}
	# Write new data file if any changes were found
	if ($changedvalues) {
	    my $newfile ="$treedir/treedata.pl";
	    $newfile .= ".new" if (!$write_changes);
	    print "Values changed for $treedir\n" if ($verbose);
	    print "Updating $newfile\n";
	    &write_treedata("$newfile", \%treedata);
	}
    }
}



# main

# Parse args
our ($opt_h, $opt_v, $opt_w);
getopts('hvw');
&usage() if (defined($opt_h));
$verbose++ if (defined($opt_v));
if (defined($opt_w)) {
    $write_changes++;
} else {
    print "Use the -w option to write live changes.\n";
}
&validate_treedata();
exit(0);
# end main

