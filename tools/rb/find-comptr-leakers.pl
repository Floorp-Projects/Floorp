#!/usr/bin/perl -w
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#
# Original Author:
#   L. David Baron <dbaron@fas.harvard.edu> - wrote script loosely
#     based on Chris Waterson's find-leakers.pl and make-tree.pl
#
# See http://www.mozilla.org/performance/refcnt-balancer.html for more info.
#

use 5.004;
use strict;
use Getopt::Long;

# GetOption will create $opt_object, so ignore the
# warning that gets spit out about those vbls.
GetOptions("object=s", "list", "help");

# use $::opt_help twice to eliminate warning...
($::opt_help) && ($::opt_help) && die qq{
usage: find-comptr-leakers.pl < logfile
  --object <obj>		 Examine only object <obj>
  --list				 Only list leaked objects
  --help				 This message :-)
};

if ($::opt_object) {
	warn "Examining only object $::opt_object (THIS IS BROKEN)\n";
} else {
	warn "Examining all objects\n";
}

my %allocs = ( );
my %counter;
my $id = 0;

my $accumulating = 0;
my $savedata = 0;
my $class;
my $obj;
my $sno;
my $op;
my $cnt;
my $ptr;
my $strace;

sub save_data {
	# save the data
	if ($op eq 'nsCOMPtrAddRef') {
		push @{ $allocs{$sno}->{$ptr} }, [ +1, $strace ];
	}
	elsif ($op eq 'nsCOMPtrRelease') {
		push @{ $allocs{$sno}->{$ptr} }, [ -1, $strace ];
		my $sum = 0;
		my @ptrallocs = @{ $allocs{$sno}->{$ptr} };
		foreach my $alloc (@ptrallocs) {
			$sum += @$alloc[0];
		}
		if ( $sum == 0 ) {
			delete($allocs{$sno}{$ptr});
		}
	}
}

LINE: while (<>) {
	if (/^</) {
		chop; # avoid \n in $ptr
		my @fields = split(/ /, $_);

		($class, $obj, $sno, $op, $cnt, $ptr) = @fields;

		$strace = "";

		if ($::opt_list) {
			save_data();
		} elsif (!($::opt_object) || ($::opt_object eq $obj)) {
			$accumulating = 1;
		}
	} elsif ( $accumulating == 1 ) {
		if ( /^$/ ) {
			# if line is empty
			$accumulating = 0;
			save_data();
		} else {
			$strace = $strace . $_;
		}
	}
}
if ( $accumulating == 1) {
	save_data();
}

foreach my $serial (keys(%allocs)) {
	foreach my $comptr (keys( %{$allocs{$serial}} )) {
		my $sum = 0;
		my @ptrallocs = @{ $allocs{$serial}->{$comptr} };
		foreach my $alloc (@ptrallocs) {
			$sum += @$alloc[0];
		}
		print "Object ", $serial, " held by ", $comptr, " is ", $sum, " out of balance.\n";
		unless ($::opt_list) {
			print "\n";
			foreach my $alloc (@ptrallocs) {
				if (@$alloc[0] == +1) {
					print "Put into nsCOMPtr at:\n";
				} elsif (@$alloc[0] == -1) {
					print "Released from nsCOMPtr at:\n";
				}
				print @$alloc[1]; # the stack trace
				print "\n";
			}
			print "\n\n";
		}
	}
}

