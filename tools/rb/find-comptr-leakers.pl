#!/usr/bin/perl -w
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Script loosely based on Chris Waterson's find-leakers.pl and make-tree.pl

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

