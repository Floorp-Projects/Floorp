#!/usr/bin/perl -w
# vim:cindent:ts=8:et:sw=4:
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   L. David Baron <dbaron@fas.harvard.edu> (original author)
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
# ***** END LICENSE BLOCK ***** */

# This script produces a diff between two files that are the result of
# calling NS_TraceMallocDumpAllocations.  Such files can be created
# through the command-line option --shutdown-leaks=<filename> or through
# the DOM function window.TraceMallocDumpAllocations(<filename>).  Both
# methods will work only if --trace-malloc=<malloc-log> is also given on
# the command line.

use 5.004;
use strict;
use Getopt::Long;

$::opt_help = 0;
$::opt_depth = 6;
$::opt_include_zero = 0;
$::opt_allocation_count = 0;

Getopt::Long::Configure("pass_through");
Getopt::Long::GetOptions("help", "depth=i", "include-zero", "allocation-count");

if ($::opt_help) {
    die "usage: diffbloatdump.pl [options] <dump1> <dump2>
  --help                 Display this message
  --depth=<num>          Only display <num> frames at top of allocation stack.
  --include-zero         Display subtrees totalling zero.
  --allocation-count     Use allocation count rather than size (i.e., treat
                           all sizes as 1).
";
}

my $calltree = { count => 0 }; # leave children undefined

sub add_file($$) {
    # Takes (1) a reference to a file descriptor for input and (2) the
    # factor to multiply the stacks by (generally +1 or -1).
    # Returns a reference to an array representing the stack, allocation
    # site in array[0].
    sub read_stack($) {
        my ($infile) = @_;
        my $line;
        my @stack;

        # read the data at the memory location
        while ( ($line = <$infile>) && substr($line,0,1) eq "\t" ) {
            # do nothing
        }

        # read the stack
        do {
            chomp($line);
            $stack[$#stack+1] = $line;
        } while ( ($line = <$infile>) && $line ne "\n" );

        return \@stack;
    }

    # adds the stack given as a parameter (reference to array, $stack[0] is
    # allocator) to $calltree, with the call count multiplied by $factor
    # (typically +1 or -1).
    sub add_stack($$) {
        my @stack = @{$_[0]};
        my $factor = $_[1];

        my $i = 0;
        my $node = $calltree;
        while ($i < $#stack && $i < $::opt_depth) {
            $node->{count} += $factor;
            if (!defined($node->{children})) {
                $node->{children} = {};
            }
            if (!defined($node->{children}->{$stack[$i]})) {
                $node->{children}->{$stack[$i]} = { count => 0 };
            }
            $node = $node->{children}->{$stack[$i]};
            ++$i;
        }
        $node->{count} += $factor;
    }

    my ($infile, $factor) = @_;

    open (INFILE, "<$infile") || die "Can't open input \"$infile\"";
    while ( ! eof(INFILE) ) {
        # read the type and address
        my $line = <INFILE>;
        unless ($line =~ /.*\((\d*)\)\n/) {
            die "badly formed allocation header";
        }
        my $size;
        if ($::opt_allocation_count) {
            $size = 1;
        } else {
            $size = $1;
        }

        add_stack(read_stack(\*INFILE), $size * $factor);
    }
    close INFILE;
}

sub print_calltree() {
    sub print_indent($) {
        my ($i) = @_;
        while (--$i >= 0) {
            print "  ";
        }
    }

    sub print_node_indent($$$) {
        my ($nodename, $node, $indent) = @_;

        if (!$::opt_include_zero && $node->{count} == 0) {
            return;
        }

        print_indent($indent);
        print "$node->{count} $nodename\n";
        if (defined($node->{children})) {
            my %kids = %{$node->{children}};
            ++$indent;
            foreach my $kid (sort { $kids{$b}->{count} <=> $kids{$a}->{count} }
                                  keys (%kids)) {
                print_node_indent($kid, $kids{$kid}, $indent);
            }
        }
    }

    print_node_indent(".root", $calltree, 0);
}

add_file($ARGV[0], -1);
add_file($ARGV[1],  1);
print_calltree();
