#!/usr/bin/perl -w
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;

my %allocs;
my %classes;
my %counter;

LINE: while (<>) {
    next LINE if (! /^</);

    my @fields = split(/ /, $_);
    my $class = shift(@fields);
    my $obj   = shift(@fields);
    my $sno   = shift(@fields);
    my $op    = shift(@fields);
    my $cnt   = shift(@fields);

    # for AddRef/Release $cnt is the refcount, for Ctor/Dtor it's the size

    if ($op eq 'AddRef' && $cnt == 1) {
        # Example: <nsStringBuffer> 0x01AFD3B8 1 AddRef 1

        $allocs{$obj} = ++$counter{$class}; # the order of allocation
        $classes{$obj} = $class;
    }
    elsif ($op eq 'Release' && $cnt == 0) {
        # Example: <nsStringBuffer> 0x01AFD3B8 1 Release 0

        delete($allocs{$obj});
        delete($classes{$obj});
    }
    elsif ($op eq 'Ctor') {
        # Example: <PStreamNotifyParent> 0x08880BD0 8 Ctor (20)

        $allocs{$obj} = ++$counter{$class};
        $classes{$obj} = $class;
    }
    elsif ($op eq 'Dtor') {
        # Example: <PStreamNotifyParent> 0x08880BD0 8 Dtor (20)

        delete($allocs{$obj});
        delete($classes{$obj});
    }
}


sub sort_by_value {
    my %x = @_;
    sub _by_value($) { my %x = @_; $x{$a} cmp $x{$b}; } 
    sort _by_value keys(%x);
} 


foreach my $key (&sort_by_value(%allocs)) {
    # Example: 0x03F1D818 (2078) @ <nsStringBuffer>
    print "$key (", $allocs{$key}, ") @ ", $classes{$key}, "\n";
}
