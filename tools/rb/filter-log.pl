#!/usr/bin/perl -w
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Filter a refcount log to show only the entries for a single object.
# Useful when manually examining refcount logs containing multiple
# objects.

use 5.004;
use strict;
use Getopt::Long;

GetOptions("object=s");

$::opt_object ||
     die qq{
usage: filter-log-for.pl < logfile
  --object <obj>         The address of the object to examine (required)
};

warn "object $::opt_object\n";

LINE: while (<>) {
    next LINE if (! /^</);
    my $line = $_;
    my @fields = split(/ /, $_);

    my $class = shift(@fields);
    my $obj   = shift(@fields);
    next LINE unless ($obj eq $::opt_object);
    my $sno   = shift(@fields);
    my $op  = shift(@fields);
    my $cnt = shift(@fields);

    print $line;

    # The lines in the stack trace
    CALLSITE: while (<>) {
        print;
        last CALLSITE if (/^$/);
    }
}
