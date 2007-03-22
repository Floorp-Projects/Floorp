#!/usr/bin/perl -w
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org Code.
#
# The Initial Developer of the Original Code is
# L. David Baron <dbaron@dbaron.org> 
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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

