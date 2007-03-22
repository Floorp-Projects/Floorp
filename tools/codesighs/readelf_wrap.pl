#!/usr/bin/perl -w
# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
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
# The Original Code is readelf_wrap.pl.
#
# The Initial Developer of the Original Code is
# IBM Corporation.
# Portions created by the Initial Developer are Copyright (C) 2003
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Brian Ryner <bryner@brianryner.com>
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

use strict;

# Section fields (the full list of values is in <elf.h>)
my $SECT_NUM  = 0;  # section index
my $SECT_NAME = 1;  # section name
my $SECT_TYPE = 2;  # section type
my $SECT_ADDR = 3;  # section virtual address
my $SECT_OFF  = 4;  # section offset in file
my $SECT_SIZE = 5;  # size of section
my $SECT_ES   = 6;  # section entry size
my $SECT_FLG  = 7;  # section flags
my $SECT_LK   = 8;  # link to another section
my $SECT_INF  = 9;  # additional section info
my $SECT_AL   = 10; # section alignment


# Symbol fields (note: the full list of possible values for each field
# is given in <elf.h>)

my $SYM_NUM   = 0;     # unique index of the symbol
my $SYM_VALUE = 1;     # value of the symbol
my $SYM_SIZE  = 2;     # size of the symbol
my $SYM_TYPE  = 3;     # type (NOTYPE, OBJECT, FUNC, SECTION, FILE, ...)
my $SYM_BIND  = 4;     # binding/scope (LOCAL, GLOBAL, WEAK, ...)
my $SYM_VIS   = 5;     # visibility (DEFAULT, INTERNAL, HIDDEN, PROTECTED)
my $SYM_NDX   = 6;     # index of section the symbol is in
my $SYM_NAME  = 7;     # name of the symbol
my $SYM_FILE  = 8;     # (not part of readelf) file for symbol

# Tell readelf to print out the list of sections and then the symbols
die "Usage: $^X <binary>\n" unless ($#ARGV >= 0);
my $readelf = $ENV{'READELF_PROG'};
if (!$readelf) {
    $readelf = 'readelf';
}
open(READELF_OUTPUT, "$readelf -Ss $ARGV[0] 2>/dev/null | c++filt |") or die "readelf failed to run on $ARGV[0]\n";

my @section_list;
my @symbol_list;
my ($module) = ($ARGV[0] =~ /([^\/]+)$/);
my $in_symbols = 0;

while (<READELF_OUTPUT>) {

    if (!$in_symbols) {
        if (/^ *\[ *(\d+)\]/) {
            my @section;

            # note that we strip off the leading '.' of section names for
            # readability
            if (! (@section = (/^ *\[ *(\d+)\] \.([\w\.\-]+) *(\w+) *(.{8}) (.{6}[0-9a-fA-F]*) (.{6}[0-9a-fA-F]*) *(\d+) ([a-zA-Z]+ +| +[a-zA-Z]+|) *(\d+) *(\w+) *(\d+)/))) {
                # capture the 'null' section which has no name, so that the
                # array indices are the same as the section indices.

                @section = ($1, '', 'NULL', '00000000', '000000', '000000',
                            '00', '', '0', '0', '0');
            }

            push (@section_list, \@section);
        } elsif (/^Symbol table/) {
            $in_symbols = 1;
        }
    } else {

        my @sym;

        if (@sym = /^\s*(\d+): (\w+)\s*(\d+)\s*(\w+)\s*(\w+)\s*(\w+)\s*(\w+) (.*)/)
        {
            # Filter out types of symbols that we don't care about:
            #  - anything that's not of type OBJECT or FUNC
            #  - any undefined symbols (ndx = UND[EF])
            #  - any 0-size symbols

            if (($sym[$SYM_TYPE] !~ /^(OBJECT|FUNC)$/) ||
                $sym[$SYM_NDX] eq 'UND' || $sym[$SYM_NDX] eq 'UNDEF'
                || $sym[$SYM_SIZE] eq '0') {
                next;
            }
            push (@symbol_list, \@sym);
        }
        elsif (/^Symbol table .*'\.symtab'/) {
            # We've been using .dynsym up to this point, but if we have .symtab
            # available, it will have everything in .dynsym and more.
            # So, reset our symbol list.

            @symbol_list = ();
        }
    }
}

close(READELF_OUTPUT);

# spit them out in codesighs TSV format
my $sym;
my @section_sizes;
$#section_sizes = $#section_list;
foreach (@section_sizes) { $_ = 0; }

foreach $sym (@symbol_list) {
    # size
    printf "%08x\t", $sym->[$SYM_SIZE];

    # code or data
    if ($sym->[$SYM_TYPE] eq 'FUNC') {
        print "CODE\t";
    } else {  # OBJECT
        print "DATA\t";
    }

    # scope
    if ($sym->[$SYM_BIND] eq 'LOCAL') {
        print "STATIC\t";
    } elsif ($sym->[$SYM_BIND] =~ /(GLOBAL|WEAK)/) {
        print "PUBLIC\t";
    } else {
        print "UNDEF\t";
    }

    # module name

    print "$module\t";

    # section
    my $section = $section_list[$sym->[$SYM_NDX]]->[$SECT_NAME];
    print "$section\t";

    # should be the object file, but for now just module/section
    print "UNDEF:$module:$section\t";

    # now the symbol name
    print $sym->[$SYM_NAME]."\n";

    # update our cumulative section sizes
    $section_sizes[$section_list[$sym->[$SYM_NDX]]->[$SECT_NUM]] += $sym->[$SYM_SIZE];
}

# Output extra entries to make the sum of the symbol sizes equal the
# section size.

my $section;
foreach $section (@section_list) {

    my $diff = hex($section->[$SECT_SIZE]) - $section_sizes[$section->[$SECT_NUM]];
    if ($diff > 0) {
        my $sectname = $section->[$SECT_NAME];
        if ($section->[$SECT_NAME] =~ /^(rodata|data|text|bss)/) {
            printf "%08x", $diff;
            print "\tDATA\tSTATIC\t$module\t$sectname\tUNDEF:$module:$sectname\t.nosyms.$sectname\n";
#        } else {
#            print "ignoring $diff bytes of empty space in $sectname section\n";
        }
    }
}
