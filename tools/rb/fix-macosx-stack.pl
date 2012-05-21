#!/usr/bin/perl
# vim:sw=4:ts=4:et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# $Id: fix-macosx-stack.pl,v 1.6 2007/08/17 06:26:09 dbaron%dbaron.org Exp $
#
# This script processes the output of nsTraceRefcnt's Mac OS X stack
# walking code.  This is useful for two things:
#  (1) Getting line number information out of
#      |nsTraceRefcntImpl::WalkTheStack|'s output in debug builds.
#  (2) Getting function names out of |nsTraceRefcntImpl::WalkTheStack|'s
#      output on all builds (where it mostly prints UNKNOWN because only
#      a handful of symbols are exported from component libraries).
#
# Use the script by piping output containing stacks (such as raw stacks
# or make-tree.pl balance trees) through this script.

use strict;
use IPC::Open2;

sub separate_debug_file_for($) {
    my ($file) = @_;
    return '';
}

my %address_adjustments;
sub address_adjustment($) {
    my ($file) = @_;
    unless (exists $address_adjustments{$file}) {
        my $result = -1;

        open(OTOOL, '-|', 'otool', '-l', $file);
        while (<OTOOL>) {
            if (/^  segname __TEXT$/) {
                if (<OTOOL> =~ /^   vmaddr (0x[0-9a-f]{8})$/) {
                    $result = hex($1);
                    last;
                } else {
                    die "Bad output from otool";
                }
            }
        }
        close(OTOOL);

        $result >= 0 || die "Bad output from otool";

        $address_adjustments{$file} = $result;
    }

    return $address_adjustments{$file};
}

sub add_info($$$) {
    my ($array, $address, $data) = @_;

    # only remember the last item at a given address
    pop @{$array} if ($#{$array} >= 0 && $array->[$#{$array}]->[0] == $address);

    push @{$array}, [ $address, $data ];
}

sub sort_by_address() {
    return $a->[0] <=> $b->[0];
}

# Return a reference to a hash whose {read} and {write} entries are a
# bidirectional pipe to an addr2line process that gives symbol
# information for a file.
my %nmstructs;
sub nmstruct_for($) {
    my ($file) = @_;
    my $nmstruct;
    my $curdir;
    unless (exists $nmstructs{$file}) {
        $nmstruct = { symbols => [], files => [], lines => [] };

        my $debug_file = separate_debug_file_for($file);
        $debug_file = $file if ($debug_file eq '');

        open(NM, '-|', 'nm', '-an', $debug_file);
        while (<NM>) {
            chomp;
            my ($addr, $ty, $rest) = ($_ =~ /^([0-9a-f ]{8}) (.) (.*)$/);
            $addr = hex($addr);
            if ($ty eq 't' || $ty eq 'T') {
                my $sym = $rest;
                if (substr($sym, 0, 1) eq '_') {
                    # symbols on Mac have an extra leading _
                    $sym = substr($sym, 1);
                }
                add_info($nmstruct->{symbols}, $addr, $sym);
            } elsif ($ty eq '-') {
                # nm gives us stabs debugging information
                my ($n1, $n2, $ty2, $rest2) =
                    ($rest =~ /^([0-9a-f]{2}) ([0-9a-f]{4}) (.{5}) (.*)$/);
                # ignore $ty2 == '  FUN'
                if ($ty2 eq 'SLINE') {
                    add_info($nmstruct->{lines}, $addr, hex($n2));
                } elsif ($ty2 eq '  SOL') {
                    # We get SOL lines within the code for a source
                    # file.  They always have file names.
                    my $file = $rest2;
                    if (!($file =~ /^\//)) {
                        # resolve relative paths
                        $file = $curdir . $file;
                    }
                    add_info($nmstruct->{files}, $addr, $file);
                } elsif ($ty2 eq '   SO') {
                    # We get SO lines at the beginning of the code for a
                    # source file, for:
                    #  * the directory of the compilation
                    #  * the file
                    #  * sometimes a blank line
                    if ($rest2 =~ /\/$/) {
                        $curdir = $rest2;
                    } elsif ($rest2 ne '') {
                        add_info($nmstruct->{files}, $addr, $rest2);
                    }
                }
            }
        }
        close(NM);

        # nm -n Doesn't sort across .o files.
        @{$nmstruct->{symbols}} = sort sort_by_address @{$nmstruct->{symbols}};
        @{$nmstruct->{lines}} = sort sort_by_address @{$nmstruct->{lines}};
        @{$nmstruct->{files}} = sort sort_by_address @{$nmstruct->{files}};

        $nmstructs{$file} = $nmstruct;
    } else {
        $nmstruct = $nmstructs{$file};
    }
    return $nmstruct;
}

my $cxxfilt_pipe;
sub cxxfilt($) {
    my ($sym) = @_;

    unless($cxxfilt_pipe) {
        my $pid = open2($cxxfilt_pipe->{read}, $cxxfilt_pipe->{write},
                        'c++filt', '--no-strip-underscores',
                                   '--format', 'gnu-v3');
    }
    my $out = $cxxfilt_pipe->{write};
    my $in = $cxxfilt_pipe->{read};
    print {$out} $sym . "\n";
    chomp(my $fixedsym = <$in>);
    return $fixedsym;
}

# binary search the array for the address
sub array_lookup($$) {
    my ($array, $address) = @_;

    my $start = 0;
    my $end = $#{$array};

    return [ -1 , "" ] if ($end == -1);

    while ($start != $end) {
        my $test = int(($start + $end + 1) / 2); # may equal $end
        # Since we're processing stack traces, and the addresses in
        # stack traces are the instructions to return to, and we really
        # want the instruction that made the call (the previous
        # instruction), use > instead of >=.
        if ($address > $array->[$test]->[0]) {
            $start = $test;
        } else {
            $end = $test - 1;
        }
    }

    return $array->[$start];
}

sub nm_lookup($$) {
    my ($nmstruct, $address) = @_;
    my $sym = array_lookup($nmstruct->{symbols}, $address);
    return {
             symbol => cxxfilt($sym->[1]),
             symbol_offset => ($address - $sym->[0]),
             file => array_lookup($nmstruct->{files}, $address)->[1],
             line => array_lookup($nmstruct->{lines}, $address)->[1]
           };
}

select STDOUT; $| = 1; # make STDOUT unbuffered
while (<>) {
    my $line = $_;
    if ($line =~ /^([ \|0-9-]*)(.*) ?\[([^ ]*) \+(0x[0-9A-F]{1,8})\](.*)$/) {
        my $before = $1; # allow preservation of balance trees
        my $badsymbol = $2;
        my $file = $3;
        my $address = hex($4);
        my $after = $5; # allow preservation of counts

        if (-f $file) {
            my $nmstruct = nmstruct_for($file);
            $address += address_adjustment($file);

            my $info = nm_lookup($nmstruct, $address);
            my $symbol = $info->{symbol};
            my $fileandline = $info->{file} . ':' . $info->{line};

            # I'm not sure if it's possible for dlsym to have gotten
            # better information, but just in case:
            if (my ($offset) = ($badsymbol =~ /\+0x([0-9A-F]{8})/)) { # FIXME: add $
                if (hex($offset) < $info->{symbol_offset}) {
                    $symbol = $badsymbol;
                }
            }

            if ($fileandline eq ':') { $fileandline = $file; }
            print "$before$symbol ($fileandline)$after\n";
        } else {
            print STDERR "Warning: File \"$file\" does not exist.\n";
            print $line;
        }

    } else {
        print $line;
    }
}
