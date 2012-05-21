#!/usr/bin/perl -w
# vim:cindent:ts=8:et:sw=4:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
$::opt_use_address = 0;

# XXX Change --use-address to be the default and remove the option
# once tinderbox is no longer using it without --use-address.

Getopt::Long::Configure("pass_through");
Getopt::Long::GetOptions("help", "allocation-count", "depth=i",
                         "include-zero", "use-address");

if ($::opt_help) {
    die "usage: diffbloatdump.pl [options] <dump1> <dump2>
  --help                 Display this message

  --allocation-count     Use allocation count rather than size (i.e., treat
                           all sizes as 1).
  --depth=<num>          Only display <num> frames at top of allocation stack.
  --include-zero         Display subtrees totalling zero.
  --use-address          Don't ignore the address part of the stack trace
                           (can make comparison more accurate when comparing
                           results from the same build)

  The input files (<dump1> and <dump2> above) are either trace-malloc
  memory dumps OR this script's output.  (If this script's output,
  --allocation-count and --use-address are ignored.)  If the input files
  have .gz or .bz2 extension, they are uncompressed.
";
}

my $calltree = { count => 0 }; # leave children undefined

sub get_child($$) {
    my ($node, $frame) = @_;
    if (!defined($node->{children})) {
        $node->{children} = {};
    }
    if (!defined($node->{children}->{$frame})) {
        $node->{children}->{$frame} = { count => 0 };
    }
    return $node->{children}->{$frame};
}

sub add_tree_file($$$) {
    my ($infile, $firstline, $factor) = @_;

    my @nodestack;
    $nodestack[1] = $calltree;
    $firstline =~ /^(-?\d+) malloc$/;
    $calltree->{count} += $1 * $factor;

    my $lineno = 1;
    while (!eof($infile)) {
        my $line = <$infile>;
        ++$lineno;
        $line =~ /^( *)(-?\d+) (.*)$/ || die "malformed input, line $lineno";
        my $depth = length($1);
        my $count = $2;
        my $frame = $3;
        die "malformed input, line $lineno" if ($depth % 2 != 0);
        $depth /= 2;
        die "malformed input, line $lineno" if ($depth > $#nodestack);
        $#nodestack = $depth;
        my $node = get_child($nodestack[$depth], $frame);
        push @nodestack, $node;
        $node->{count} += $count * $factor;
    }
}

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
        while ( defined($infile) && ($line = <$infile>) && substr($line,0,1) eq "\t" ) {
            # do nothing
        }

        # read the stack
        do {
            chomp($line);
            if ( ! $::opt_use_address &&
                 $line =~ /(.*)\[(.*)\]/) {
                $line = $1;
            }
            $stack[$#stack+1] = $line;
        } while ( defined($infile) && ($line = <$infile>) && $line ne "\n" && $line ne "\r\n" );

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
            $node = get_child($node, $stack[$i]);
            ++$i;
        }
        $node->{count} += $factor;
    }

    my ($infile, $factor) = @_;

    if ($infile =~ /\.bz2$/) {
        # XXX This doesn't propagate errors from bzip2.
        open (INFILE, "bzip2 -cd '$infile' |") || die "Can't open input \"$infile\"";
    } elsif ($infile =~ /\.gz$/) {
        # XXX This doesn't propagate errors from gzip.
        open (INFILE, "gzip -cd '$infile' |") || die "Can't open input \"$infile\"";
    } else {
        open (INFILE, "<$infile") || die "Can't open input \"$infile\"";
    }
    my $first = 1;
    while ( ! eof(INFILE) ) {
        # read the type and address
        my $line = <INFILE>;
        if ($first) {
            $first = 0;
            if ($line =~ /^-?\d+ malloc$/) {
                # We're capable of reading in our own output as well.
                add_tree_file(\*INFILE, $line, $factor);
                close INFILE;
                return;
            }
        }
        unless ($line =~ /.*\((\d*)\)[\r|\n]/) {
            die "badly formed allocation header in $infile";
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

sub print_node_indent($$$);

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

    print_node_indent("malloc", $calltree, 0);
}

add_file($ARGV[0], -1);
add_file($ARGV[1],  1);
print_calltree();
