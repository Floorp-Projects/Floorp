#!/usr/bin/perl -w
#
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
# The Original Code is mozilla.org Code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

use 5.004;
use strict;
use Getopt::Long;

$::opt_prune_depth = 0;
$::opt_subtree_size = 0;
$::opt_reverse = 0;

# GetOption will create $opt_object & $opt_exclude, so ignore the
# warning that gets spit out about those vbls.
GetOptions("object=s", "exclude=s", "comptrs=s", "ignore-balanced", "subtree-size=i", "prune-depth=i",
            "collapse-to-method", "collapse-to-class", "old-style", "reverse");

$::opt_object ||
     die qq{
usage: leak.pl < logfile
  --object <obj>         The address of the object to examine (required)
  --exclude <file>       Exclude routines listed in <file>
  --comptrs <file>       Subtract all the data in the balanced COMPtr log <file>
  --ignore-balanced      Ignore balanced subtrees
  --subtree-size <n>     Print subtrees with more than <n> nodes separately
  --prune-depth <depth>  Prune the tree to <depth>
  --collapse-to-method   Aggregate data by method
  --collapse-to-class    Aggregate data by class (subsumes --collapse-to-method)
  --reverse              Reverse call stacks, showing leaves first
  --old-style            Old-style formatting
};

$::opt_prune_depth = 0 if $::opt_prune_depth < 0;
$::opt_subtree_size = 0 if $::opt_subtree_size < 0;

warn "object $::opt_object\n";
warn "ignoring balanced subtrees\n" if $::opt_ignore_balanced;
warn "prune depth $::opt_prune_depth\n" if $::opt_prune_depth;
warn "collapsing to class\n" if $::opt_collapse_to_class;
warn "collapsing to method\n" if $::opt_collapse_to_method && !$::opt_collapse_to_class;
warn "reversing call stacks\n" if $::opt_reverse;


# The 'excludes' are functions that, if detected in a particular call
# stack, will cause the _entire_ call stack to be ignored. You might,
# for example, explicitly exclude two functions that have a matching
# AddRef/Release pair.

my %excludes;

if ($::opt_exclude) {
     open(EXCLUDE, "<".$::opt_exclude)
         || die "unable to open $::opt_exclude";
 
     while (<EXCLUDE>) {
         chomp $_;
         warn "excluding $_\n";
         $excludes{$_} = 1;
     }
}

# Each entry in the tree rooted by callGraphRoot contains the following:
#   #name#      This call's name+offset string
#   #refcount#  The net reference count of this call
#   #label#     The label used for this subtree; only defined for labeled nodes
#   #children#  List of children in alphabetical order
# zero or more children indexed by method name+offset strings.

my $callGraphRoot;
$callGraphRoot = { '#name#' => '.root', '#refcount#' => 'n/a' };

# The 'imbalance' is a gross count of how balanced a particular
# callsite is. It is used to prune away callsites that are detected to
# be balanced; that is, that have matching AddRef/Release() pairs.

my %imbalance;
$imbalance{'.root'} = 'n/a';

# The main read loop.

sub read_data($$$) {
     my ($INFILE, $plus, $minus) = @_;

     LINE: while (<$INFILE>) {
          next LINE if (! /^</);
          my @fields = split(/ /, $_);
     
          my $class = shift(@fields);
          my $obj   = shift(@fields);
          my $sno   = shift(@fields);
          next LINE unless ($obj eq $::opt_object);
     
          my $op  = shift(@fields);
          next LINE unless ($op eq $plus || $op eq $minus);
     
          my $cnt = shift(@fields);
     
          # Collect the remaining lines to create a stack trace.
          my @stack;
          CALLSITE: while (<$INFILE>) {
              chomp;
              last CALLSITE if (/^$/);
              $stack[++$#stack] = $_;
          }
     
          # Reverse the remaining fields to produce the call stack, with the
          # oldest frame at the front of the array.
          if (! $::opt_reverse) {
              @stack = reverse(@stack);
          }
     
          my $call;
     
          # If any of the functions in the stack are supposed to be excluded,
          # march on to the next line.
          foreach $call (@stack) {
              next LINE if exists($excludes{$call});
          }
     
     
          # Add the callstack as a path through the call graph, updating
          # refcounts at each node.
     
          my $caller = $callGraphRoot;
     
          foreach $call (@stack) {
     
              # Chop the method offset if we're 'collapsing to method' or
              # 'collapsing to class'.
              $call =~ s/\+0x.*$//g if ($::opt_collapse_to_method || $::opt_collapse_to_class);
     
              # Chop the method name if we're 'collapsing to class'.
              $call =~ s/::.*$//g if ($::opt_collapse_to_class);
     
              my $site = $caller->{$call};
              if (!$site) {
                  # This is the first time we've seen this callsite. Add a
                  # new entry to the call tree.
     
                  $site = { '#name#' => $call, '#refcount#' => 0 };
                  $caller->{$call} = $site;
              }
     
              if ($op eq $plus) {
                  ++($site->{'#refcount#'});
                  ++($imbalance{$call});
              } elsif ($op eq $minus) {
                  --($site->{'#refcount#'});
                  --($imbalance{$call});
              } else {
                  die "Bad operation $op";
              }
     
              $caller = $site;
          }
     }
}

read_data(*STDIN, "AddRef", "Release");

if ($::opt_comptrs) {
     warn "Subtracting comptr log ". $::opt_comptrs . "\n";
     open(COMPTRS, "<".$::opt_comptrs)
         || die "unable to open $::opt_comptrs";

     # read backwards to subtract
     read_data(*COMPTRS, "nsCOMPtrRelease", "nsCOMPtrAddRef");
}

sub num_alpha {
     my ($aN, $aS, $bN, $bS);
     ($aN, $aS) = ($1, $2) if $a =~ /^(\d+) (.+)$/;
     ($bN, $bS) = ($1, $2) if $b =~ /^(\d+) (.+)$/;
     return $a cmp $b unless defined $aN && defined $bN;
     return $aN <=> $bN unless $aN == $bN;
     return $aS cmp $bS;
}

# Given a subtree and its nesting level, return true if that subtree should be pruned.
# If it shouldn't be pruned, destructively attempt to prune its children.
# Also compute the #children# properties of unpruned nodes.
sub prune($$) {
     my ($site, $nest) = @_;

     # If they want us to prune the tree's depth, do so here.
     return 1 if ($::opt_prune_depth && $nest >= $::opt_prune_depth);

     # If the subtree is balanced, ignore it.
     return 1 if ($::opt_ignore_balanced && !$site->{'#refcount#'});

     my $name = $site->{'#name#'};

     # If the symbol isn't imbalanced, then prune here (and warn)
     if ($::opt_ignore_balanced && !$imbalance{$name}) {
         warn "discarding " . $name . "\n";
#         return 1;
     }

     my @children;
     foreach my $child (sort num_alpha keys(%$site)) {
         if (substr($child, 0, 1) ne '#') {
             if (prune($site->{$child}, $nest + 1)) {
                 delete $site->{$child};
             } else {
                 push @children, $site->{$child};
             }
         }
     }
     $site->{'#children#'} = \@children;
     return 0;
}


# Compute the #label# properties of this subtree.
# Return the subtree's number of nodes, not counting nodes reachable 
# through a labeled node.
sub createLabels($) {
     my ($site) = @_;
     my @children = @{$site->{'#children#'}};
     my $nChildren = @children;
     my $nDescendants = 0;

     foreach my $child (@children) {
         my $childDescendants = createLabels($child);
         if ($nChildren > 1 && $childDescendants > $::opt_subtree_size) {
             die "Internal error" if defined($child->{'#label#'});
             $child->{'#label#'} = "__label__";
             $childDescendants = 1;
         }
         $nDescendants += $childDescendants;
     }
     return $nDescendants + 1;
}


my $nextLabel = 0;
my @labeledSubtrees;

sub list($$$$$) {
     my ($site, $nest, $nestStr, $childrenLeft, $root) = @_;
     my $label = !$root && $site->{'#label#'};

     # Assign a unique number to the label.
     if ($label) {
         die unless $label eq "__label__";
         $label = "__" . ++$nextLabel . "__";
         $site->{'#label#'} = $label;
         push @labeledSubtrees, $site;
     }

     print $nestStr;
     if ($::opt_old_style) {
         print $label, " " if $label;
         print $site->{'#name#'}, ": bal=", $site->{'#refcount#'}, "\n";
     } else {
         my $refcount = $site->{'#refcount#'};
         my $l = 8 - length $refcount;
         $l = 1 if $l < 1;
         print $refcount, " " x $l;
         print $label, " " if $label;
         print $site->{'#name#'}, "\n";
     }

     $nestStr .= $childrenLeft && !$::opt_old_style ? "| " : "  ";
     if (!$label) {
         my @children = @{$site->{'#children#'}};
         $childrenLeft = @children;
         foreach my $child (@children) {
             $childrenLeft--;
             list($child, $nest + 1, $nestStr, $childrenLeft);
         }
     }
}


if (!prune($callGraphRoot, 0)) {
     createLabels $callGraphRoot if ($::opt_subtree_size);
     list $callGraphRoot, 0, "", 0, 1;
     while (@labeledSubtrees) {
         my $labeledSubtree = shift @labeledSubtrees;
         print "\n------------------------------\n", 
$labeledSubtree->{'#label#'}, "\n";
         list $labeledSubtree, 0, "", 0, 1;
     }
     print "\n------------------------------\n" if @labeledSubtrees;
}

print qq{
Imbalance
---------
};

foreach my $call (sort num_alpha keys(%imbalance)) {
     print $call . " " . $imbalance{$call} . "\n";
}

