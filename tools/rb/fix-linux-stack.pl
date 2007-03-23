#!/usr/bin/perl
# vim:sw=4:ts=4:et:
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
# The Original Code is fix-linux-stack.pl.
#
# The Initial Developer of the Original Code is L. David Baron.
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   L. David Baron <dbaron@dbaron.org> (original author)
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
# the debug sections of the binary.
# See http://sources.redhat.com/gdb/current/onlinedocs/gdb_16.html#SEC154
# for documentation of external debuginfo.
# On Fedora distributions, these files can be obtained by installing
# *-debuginfo RPM packages.
sub debuginfo_file_for($) {
    my ($file) = @_;
    # We can read the .gnu_debuglink section using either of:
    #   objdump -s --section=.gnu_debuglink $file
    #   readelf -x .gnu_debuglink $file
    # Since we already depend on readelf in |address_adjustment|, use it
    # again here.

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

    # Read the debuglink section as an array of words, in hexidecimal,
    # library endianness.  (I'm guessing that readelf's big-endian
    # output is sensible; I've only tested little-endian, where it's a
    # bit odd.)
    open(DEBUGLINK, '-|', 'readelf', '-x', '.gnu_debuglink', $file);
    my @words;
    while (<DEBUGLINK>) {
        if ($_ =~ /^  0x[0-9a-f]{8} ([0-9a-f ]{8}) ([0-9a-f ]{8}) ([0-9a-f ]{8}) ([0-9a-f ]{8}).*/) {
            if ($endian eq 'little') {
                push @words, $4, $3, $2, $1;
            } else {
                push @words, $1, $2, $3, $4;
            }
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
            if ($endian eq 'little') {
                push @chars, $4, $3, $2, $1;
            } else {
                push @chars, $1, $2, $3, $4;
            }
        } else {
            print STDERR "Warning: malformed readelf output for $file.\n";
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

    my $old_num = $#chars;
    while ($chars[$#chars] eq '00') {
        pop @chars;
    }
    if ($old_num == $#chars || $old_num - 4 > $#chars) {
        print STDERR "Warning: malformed .gnu_debuglink section in $file.\n";
        return '';
    }

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
        my $debug_file = debuginfo_file_for($file);
        $debug_file = $file if ($debug_file eq '');

        my $pid = open2($pipe->{read}, $pipe->{write},
                        '/usr/bin/addr2line', '-C', '-f', '-e', $debug_file);
        $pipes{$file} = $pipe;
    } else {
        $pipe = $pipes{$file};
    }
    return $pipe;
}

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
            if ($symbol eq '??') { $symbol = $badsymbol; }
            if ($fileandline eq '??:0') { $fileandline = $file; }
            print "$before$symbol ($fileandline)$after\n";
        } else {
            print STDERR "Warning: File \"$file\" does not exist.\n";
            print $line;
        }

    } else {
        print $line;
    }
}
