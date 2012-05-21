#!/usr/bin/perl -w
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This tool is used to construct the ``type inference'' file. It
# prints the total number of bytes that are attributed to a type that
# cannot be inferred, grouped by stack trace; e.g.,
#
# (100) PR_Malloc
#   (50) foo
#     (50) foo2::foo2
#   (25) bar
#   (25) baz
# (50) __builtin_new
#   (50) foo2::foo2
#
#
# Which indicates that 100 bytes were allocated by uninferrable
# classes via PR_Malloc(). Of that 100 bytes, 50 were allocated from
# calls by foo(), 25 from calls by bar(), and 25 from calls by baz().
# 50 bytes were allocated by __builtin_new from foo2's ctor.
#
#
# From this, we might be able to infer the type of the object that was
# created by examining the PR_Malloc() usage in foo() and the
# ::operator new() usage in foo2(), and could add new type inference
# rules; e.g.,
#
#   <unclassified-string>
#   foo
#   foo2
#
#   # Attribute ::operator new() usage in foo2's ctor to foo2
#   <foo2>
#   __builtin_new
#   foo2::foo2

use 5.004;
use strict;
use Getopt::Long;

# So we can find TraceMalloc.pm
use FindBin;
use lib "$FindBin::Bin";

use TraceMalloc;

# Collect program options
$::opt_help = 0;
$::opt_depth = 10;
$::opt_types = "${FindBin::Bin}/types.dat";
GetOptions("help", "depth=n", "types=s");

if ($::opt_help) {
    die "usage: uncategorized.pl [options] <dumpfile>
  --help          Display this message
  --depth=<n>     Display at most <n> stack frames
  --types=<file>  Read type heuristics from <file>";
}

# Initialize type inference juju from the type file specified by
# ``--types''.
TraceMalloc::init_type_inference($::opt_types);

# Read the objects from the dump file. For each object, remember up to
# ``--depth'' stack frames (from the top). Thread together common
# stack prefixes, accumulating the number of bytes attributed to the
# prefix.

# This'll hold the inverted stacks
$::Stacks = { '#bytes#' => 0 };

sub collect_stacks($) {
    my ($object) = @_;
    my $stack = $object->{'stack'};

    return unless ($object->{'type'} eq 'void*') && (TraceMalloc::infer_type($stack) eq 'void*');

    my $count = 0;
    my $link = \%::Stacks;
  FRAME: foreach my $frame (@$stack) {
      last FRAME unless $count++ < $::opt_depth;

      $link->{'#bytes#'} += $object->{'size'};
      $link->{$frame} = { '#bytes#' => 0 } unless $link->{$frame};
      $link = $link->{$frame};
  }
}

TraceMalloc::read(\&collect_stacks);

# Do a depth-first walk of the inverted stack tree.

sub walk($$) {
    my ($links, $indent) = @_;

    my @keys;
  KEY: foreach my $key (keys %$links) {
      next KEY if $key eq '#bytes#';
      $keys[$#keys + 1] = $key;
  }

    foreach my $key (sort { $links->{$b}->{'#bytes#'} <=> $links->{$a}->{'#bytes#'} } @keys) {
        for (my $i = 0; $i < $indent; ++$i) {
            print "  ";
        }

        print "($links->{$key}->{'#bytes#'}) $key\n";

        walk($links->{$key}, $indent + 1);
    }
}

walk(\%::Stacks, 0);

