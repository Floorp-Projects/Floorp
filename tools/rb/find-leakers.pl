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
# $Id: find-leakers.pl,v 1.1 2000/07/12 01:32:54 dbaron%fas.harvard.edu Exp $
#
# See http://www.mozilla.org/performance/refcnt-balancer.html for more info.
#

my %allocs;
my %counter;
my $id = 0;

LINE: while (<>) {
    next LINE if (! /^</);
    my @fields = split(/ /, $_);

    my $class = shift(@fields);
    my $obj   = shift(@fields);
    my $sno   = shift(@fields);
    my $op    = shift(@fields);
    my $cnt   = shift(@fields);

    if ($op eq 'AddRef' && $cnt == 1) {
        $allocs{$obj} = ++$counter{$class}; # the order of allocation
        $classes{$obj} = $class;
    }
    elsif ($op eq 'Release' && $cnt == 0) {
        delete($allocs{$obj});
    }
}

foreach $key (keys(%allocs)) {
    print "$key (", $allocs{$key}, ") @ ", $classes{$key}, "\n";
}

