#!/usr/bin/perl -w
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla Leak-o-Matic.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corp.  Portions created by Netscape Communucations
# Corp. are Copyright (C) 1999 Netscape Communications Corp.  All
# Rights Reserved.
# 
# Contributor(s):
# Chris Waterson <waterson@netscape.com>
# 
# $Id: balance.cgi,v 1.3 2007/01/02 22:54:24 timeless%mozdev.org Exp $
#

#
# Builds a tree of reference counts
#

use 5.006;
use strict;

use CGI;
use POSIX;
use Zip;

$::query = new CGI();

$::opt_log                = $::query->param('log');
$::opt_class              = $::query->param('class');
$::opt_object             = $::query->param('object');
# &exclude=a&exclude=b
@::opt_exclude            = $::query->param('exclude');
$::opt_show_balanced      = $::query->param('show-balanced');
$::opt_subtree_size       = $::query->param('subtree-size');
$::opt_prune_depth        = $::query->param('prune-depth');
$::opt_reverse            = $::query->param('reverse');
$::opt_collapse_to_method = $::query->param('collapse-to-method');
$::opt_collapse_to_class  = $::query->param('collapse-to-class');
# old style clearly hasn't been supported for a while.
$::opt_old_style          = 0;

defined $::opt_log || die "Must specifiy a log file";
$::opt_log =~ m{^\w[\w\d\._-]*$} || die "Unexpected log file name";
-f $::opt_log    || die "Can't find log file";
$::opt_class =~ m/^\w[\w\d_]*$/i || die "Unexpected format for class name";
$::opt_object =~ m/^\d+$/ || die "Unexpected format for object";

# Make sure that values get initialized properly
$::opt_prune_depth = 0        if (! $::opt_prune_depth);
$::opt_subtree_size = 0       if (! $::opt_subtree_size);
$::opt_reverse = 0            if (! $::opt_reverse);
$::opt_collapse_to_class = 0  unless ($::opt_collapse_to_class || '') =~ /^\s*[01]\s*$/;
$::opt_collapse_to_method = 0 unless ($::opt_collapse_to_method || '') =~ /^\s*[01]\s*$/;

# Sanity checks
$::opt_prune_depth = 0  unless $::opt_prune_depth =~ /^\s*[1-9]\d*\s*$/;
$::opt_subtree_size = 0 unless $::opt_subtree_size =~ /^\s*[1-9]\d*\s*$/;


print $::query->header;

print qq{
<html>
<head>
<title>$::opt_class [$::opt_object]</title>
<script language="JavaScript" src="balance.js"></script>
<style type="text/css" src="balance.css"></style>
</head>
<body>
};

print $::query->h1("$::opt_class [$::opt_object]");

print "<small>\n";
{
    my @statinfo = stat($::opt_log);
    my $when = POSIX::strftime "%a %b %e %H:%M:%S %Y", localtime($statinfo[9]);
    print "$when<br>\n";
}

print "<a href='bloat-log.cgi?log=$::opt_log'>Bloat Log</a>\n";
print "<a href='leaks.cgi?log=$::opt_log'>Back to Overview</a>\n";
print "</small>\n";

# The 'excludes' are functions that, if detected in a particular call
# stack, will cause the _entire_ call stack to be ignored. You might,
# for example, explicitly exclude two functions that have a matching
# AddRef/Release pair.

my %excludes;
{
    my $method;
    foreach $method (@::opt_exclude) {
	$excludes{$method} = 1;
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
my $log = new Zip($::opt_log);
$log || die('unable to open log $::opt_log');

my $logfile = "refcnt-" . $::opt_class . "-" . $::opt_object . ".log";

my $handle = $log->expand($logfile);

LINE: while (<$handle>) {
     next LINE if (! /^</);
     my @fields = split(/ /, $_);

     my $class = shift(@fields);
     my $obj   = shift(@fields);
     my $sno   = shift(@fields);
     next LINE unless ($sno eq $::opt_object);

     my $op  = shift(@fields);
     next LINE unless ($op eq "AddRef" || $op eq "Release");

     my $cnt = shift(@fields);

     # Collect the remaining lines to create a stack trace.
     my @stack;
     CALLSITE: while (<$handle>) {
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

         if ($op eq 'AddRef') {
             ++($site->{'#refcount#'});
             ++($imbalance{$call});
         } elsif ($op eq 'Release') {
             --($site->{'#refcount#'});
             --($imbalance{$call});
         } else {
             die "Bad operation $op";
         }

         $caller = $site;
     }
}

# Given a subtree and its nesting level, return true if that subtree should be pruned.
# If it shouldn't be pruned, destructively attempt to prune its children.
# Also compute the #children# properties of unpruned nodes.
sub prune($$) {
     my ($site, $nest) = @_;

     # If they want us to prune the tree's depth, do so here.
     return 1 if ($::opt_prune_depth && $nest >= $::opt_prune_depth);

     # If the subtree is balanced, ignore it.
     return 1 if (!$::opt_show_balanced && !$site->{'#refcount#'});

     my $name = $site->{'#name#'};

     # If the symbol isn't imbalanced, then prune here (and warn)
     # XXX no symbol-level balancing; this was buggy.
#     if (!$::opt_show_balanced && !$imbalance{$name}) {
#         warn "discarding " . $name . "\n";
#         return 1;
#     }

     my @children;
     foreach my $child (sort(keys(%$site))) {
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
	 print "<tt class='method'>";
         print $site->{'#name#'};
	 print "</tt>\n";
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

     print "<div onclick='toggle(event.target);'><pre>\n";
     list $callGraphRoot, 0, "", 0, 1;
     while (@labeledSubtrees) {
         my $labeledSubtree = shift @labeledSubtrees;
         print "\n<hr>\n", $labeledSubtree->{'#label#'}, "\n";
         list $labeledSubtree, 0, "", 0, 1;
     }
     print "\n<hr>\n" if @labeledSubtrees;
     print "</pre></div>\n";
}

# Now generate the control panel at the bottom. This needs to be "neater".

print qq{
<hr>
<form method='get' action='balance.cgi' onsubmit='onsubmit();'>
<input id='log'    name='log'    type='hidden' value='$::opt_log'></input>
<input id='class'  name='class'  type='hidden' value='$::opt_class'></input>
<input id='object' name='object' type='hidden' value='$::opt_object'></input>

<fieldset>
<legend>Methods to Exclude</legend>
<select id='exclude' name='exclude' style='width:100%;height:10em' size="10" multiple>
};

{
    my $method;
    foreach $method (@::opt_exclude) {
	print "  <option selected>$method</option>\n";
    }
}

print qq{
</select>
<br>

<input type="button" onclick="remove();" value="Remove"></input>
</fieldset>

<fieldset>
<legend>Options</legend>
};

print "<input id='collapse-to-method' name='collapse-to-method' type='checkbox' value='$::opt_collapse_to_method' ";
print "checked" if $::opt_collapse_to_method;
print ">Collapse To Method</input>\n";

print "<input id='collapse-to-class' value='collapse-to-class' type='checkbox' value='$::opt_collapse_to_class' ";
print "checked" if $::opt_collapse_to_class;
print ">Collapse To Class</input>\n";

print qq{
</fieldset>

<input type='submit' value='Rebuild'></input>
</form>
};

print $::query->end_html;


