#!/usr/bin/perl -w
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# List the missing symbols (or, do a set difference).
#

use 5.004;
use strict;
use Getopt::Long;

$::opt_help = 0;
$::opt_defined = "mozilla-bin.order";
GetOptions("help", "defined=s");

if ($::opt_help) {
    die "usage: missing-syms.pl --defined=<file> <symlist>";
}

$::Defined = { };

open(DEFINED, "<$::opt_defined") || die "couldn't open defined symbol list $::opt_defined";
while (<DEFINED>) {
    chomp;
    $::Defined{$_} = 1;
}
close(DEFINED);

while (<>) {
    chomp;
    print "$_\n" unless ($::Defined{$_});
}

