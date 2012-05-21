#!/usr/bin/perl
# vim:sw=4:ts=4:et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# $Id: fix-linux-stack.pl,v 1.16 2008/05/05 21:51:11 dbaron%dbaron.org Exp $
#
# This script uses addr2line (part of binutils) to process the output of
# nsTraceRefcnt's Linux stack walking code.  This is useful for two
# things:
#  (1) Getting line number information out of
#      |nsTraceRefcntImpl::WalkTheStack|'s output in debug builds.
#  (2) Getting function names out of |nsTraceRefcntImpl::WalkTheStack|'s
#      output on optimized builds (where it mostly prints UNKNOWN
#      because only a handful of symbols are exported from component
#      libraries).
#
# Use the script by piping output containing stacks (such as raw stacks
# or make-tree.pl balance trees) through this script.

use strict;
use IPC::Open2;
use File::Basename;

# XXX Hard-coded to gdb defaults (works on Fedora).
my $global_debug_dir = '/usr/lib/debug';

# addr2line wants offsets relative to the base address for shared
# libraries, but it wants addresses including the base address offset
# for executables.  This function returns the appropriate address
# adjustment to add to an offset within file.  See bug 230336.
my %address_adjustments;
sub address_adjustment($) {
    my ($file) = @_;
    unless (exists $address_adjustments{$file}) {
        # find out if it's an executable (as opposed to a shared library)
        my $elftype;
        open(ELFHDR, '-|', 'readelf', '-h', $file);
        while (<ELFHDR>) {
            if (/^\s*Type:\s+(\S+)/) {
                $elftype = $1;
                last;
            }
        }
        close(ELFHDR);

        # If it's an executable, make adjustment the base address.
        # Otherwise, leave it zero.
        my $adjustment = 0;
        if ($elftype eq 'EXEC') {
            open(ELFSECS, '-|', 'readelf', '-S', $file);
            while (<ELFSECS>) {
                if (/^\s*\[\s*\d+\]\s+\.text\s+\w+\s+(\w+)\s+(\w+)\s+/) {
                    # Subtract the .text section's offset within the
                    # file from its base address.
                    $adjustment = hex($1) - hex($2);
                    last;
                }
            }
            close(ELFSECS);
        }

        $address_adjustments{$file} = $adjustment;
    }
    return $address_adjustments{$file};
}

# Files sometimes contain a link to a separate object file that contains
# the debug sections of the binary, removed so that a smaller file can
# be shipped, but kept separately so that it can be obtained by those
# who want it.
# See http://sources.redhat.com/gdb/current/onlinedocs/gdb_16.html#SEC154
# for documentation of debugging information in separate files.
# On Fedora distributions, these files can be obtained by installing
# *-debuginfo RPM packages.
sub separate_debug_file_for($) {
    my ($file) = @_;
    # We can read the .gnu_debuglink section using either of:
    #   objdump -s --section=.gnu_debuglink $file
    #   readelf -x .gnu_debuglink $file
    # Since readelf prints things backwards on little-endian platforms
    # for some versions only (backwards on Fedora Core 6, forwards on
    # Fedora 7), use objdump.

    # See if there's a .gnu_debuglink section
    my $have_debuglink = 0;
    open(ELFSECS, '-|', 'readelf', '-S', $file);
    while (<ELFSECS>) {
        if (/^\s*\[\s*\d+\]\s+\.gnu_debuglink\s+\w+\s+(\w+)\s+(\w+)\s+/) {
            $have_debuglink = 1;
            last;
        }
    }
    close(ELFSECS);
    return '' unless ($have_debuglink);

    # Determine the endianness of the shared library.
    my $endian = '';
    open(ELFHDR, '-|', 'readelf', '-h', $file);
    while (<ELFHDR>) {
        if (/^\s*Data:\s+.*(little|big) endian.*$/) {
            $endian = $1;
            last;
        }
    }
    close(ELFHDR);
    if ($endian ne 'little' && $endian ne 'big') {
        print STDERR "Warning: could not determine endianness of $file.\n";
        return '';
    }


    # Read the debuglink section as an array of words, in hexidecimal.
    open(DEBUGLINK, '-|', 'objdump', '-s', '--section=.gnu_debuglink', $file);
    my @words;
    while (<DEBUGLINK>) {
        if ($_ =~ /^ [0-9a-f]* ([0-9a-f ]{8}) ([0-9a-f ]{8}) ([0-9a-f ]{8}) ([0-9a-f ]{8}).*/) {
            push @words, $1, $2, $3, $4;
        }
    }
    close(DEBUGLINK);

    while (@words[$#words] eq '        ') {
        pop @words;
    }

    if ($#words < 1) {
        print STDERR "Warning: .gnu_debuglink section in $file too short.\n";
        return '';
    }

    my @chars;
    while ($#words >= 0) {
        my $w = shift @words;
        if ($w =~ /^([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})$/) {
            push @chars, $1, $2, $3, $4;
        } else {
            print STDERR "Warning: malformed objdump output for $file.\n";
            return '';
        }
    }

    my @hash_bytes = map(hex, @chars[$#chars - 3 .. $#chars]);
    $#chars -= 4;

    my $hash;
    if ($endian eq 'little') {
        $hash = ($hash_bytes[3] << 24) | ($hash_bytes[2] << 16) | ($hash_bytes[1] << 8) | $hash_bytes[0];
    } else {
        $hash = ($hash_bytes[0] << 24) | ($hash_bytes[1] << 16) | ($hash_bytes[2] << 8) | $hash_bytes[3];
    }

    # The string ends with a null-terminator and then 0 to three bytes
    # of padding to fill the current 32-bit unit.  (This padding is
    # usually null bytes, but I've seen null-null-H, on Ubuntu x86_64.)
    my $terminator = 1;
    while ($chars[$terminator] ne '00') {
        if ($terminator == $#chars) {
            print STDERR "Warning: missing null terminator in " .
                         ".gnu_debuglink section of $file.\n";
            return '';
        }
        ++$terminator;
    }
    if ($#chars - $terminator > 3) {
        print STDERR "Warning: Excess padding in .gnu_debuglink section " .
                     "of $file.\n";
        return '';
    }
    $#chars = $terminator - 1;

    my $basename = join('', map { chr(hex($_)) } @chars);

    # Now $basename and $hash represent the information in the
    # .gnu_debuglink section.
    #printf STDERR "%x: %s\n", $hash, $basename;

    my @possible_results = (
        dirname($file) . $basename,
        dirname($file) . '.debug/' . $basename,
        $global_debug_dir . dirname($file) . '/' . $basename
    );
    foreach my $result (@possible_results) {
        if (-f $result) {
            # XXX We should check the hash.
            return $result;
        }
    }

    return '';
}

# Return a reference to a hash whose {read} and {write} entries are a
# bidirectional pipe to an addr2line process that gives symbol
# information for a file.
my %pipes;
sub addr2line_pipe($) {
    my ($file) = @_;
    my $pipe;
    unless (exists $pipes{$file}) {
        my $debug_file = separate_debug_file_for($file);
        $debug_file = $file if ($debug_file eq '');

        my $pid = open2($pipe->{read}, $pipe->{write},
                        '/usr/bin/addr2line', '-C', '-f', '-e', $debug_file);
        $pipes{$file} = $pipe;
    } else {
        $pipe = $pipes{$file};
    }
    return $pipe;
}

# Ignore SIGPIPE as a workaround for addr2line crashes in some situations.
$SIG{PIPE} = 'IGNORE';

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
            my $pipe = addr2line_pipe($file);
            $address += address_adjustment($file);

            my $out = $pipe->{write};
            my $in = $pipe->{read};
            printf {$out} "0x%X\n", $address;
            chomp(my $symbol = <$in>);
            chomp(my $fileandline = <$in>);
            if (!$symbol || $symbol eq '??') { $symbol = $badsymbol; }
            if (!$fileandline || $fileandline eq '??:0') {
                $fileandline = $file;
            }
            print "$before$symbol ($fileandline)$after\n";
        } else {
            print STDERR "Warning: File \"$file\" does not exist.\n";
            print $line;
        }

    } else {
        print $line;
    }
}
