#!/usr/bonsaitools/bin/perl -wT
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Andreas Franke <afranke@mathweb.org>
#                 Christian Reis <kiko@async.com.br>

use diagnostics;
use strict;

use lib qw(.);
require "CGI.pl";

# Shut up misguided -w warnings about "used only once":

use vars %::FORM;

ConnectToDatabase();

quietly_check_login();

$::usergroupset = $::usergroupset; # More warning suppression silliness.

######################################################################
# Begin Data/Security Validation
######################################################################

# Make sure the bug ID is a positive integer representing an existing
# bug that the user is authorized to access
ValidateBugID($::FORM{'id'});

my $id = $::FORM{'id'};
my $hide_resolved = $::FORM{'hide_resolved'} || 0;
my $maxdepth = $::FORM{'maxdepth'} || 0;

if ($maxdepth !~ /^\d+$/) { $maxdepth = 0 };
if ($hide_resolved !~ /^\d+$/ || $hide_resolved != 1) { $hide_resolved = 0 };

######################################################################
# End Data/Security Validation
######################################################################

#
# Globals

# A hash to count visited bugs, and also to avoid processing repeated bugs
my %seen;

# HTML output generated in the parse of the dependency tree. This is a
# global only to avoid excessive complication in the recursion invocation
my $html;

# Saves the largest of the two actual depths of the trees
my $realdepth = 0;

# The scriptname for use as FORM ACTION.
my $scriptname = $::ENV{'SCRIPT_NAME'}; # showdependencytree.cgi

#
# Functions

# DumpKids recurses through the bug hierarchy starting at bug i, and
# appends the bug information found to the html global variable. The
# parameters are not straightforward, so look at the examples.
#
# DumpKids(i, target [, depth])
#
# Params
#   i: The bug id to analyze
#   target: The type we are looking for; either "blocked" or "dependson"
# Optional
#   depth: The current dependency depth we are analyzing, used during
#          recursion
# Globals Modified
#   html: Bug descriptions are appended here
#   realdepth: We set the maximum depth of recursion reached
#   seen: We store the bugs analyzed so far
# Globals Referenced
#   maxdepth
#   hide_resolved
#
# Examples:
#   DumpKids(163, "blocked");
#       will look for bugs that depend on bug 163
#   DumpKids(163, "dependson");
#       will look for bugs on which bug 163 depends

sub DumpKids {
    my ($i, $target, $depth) = (@_);
    my $bgcolor = "#d9d9d9";
    my $fgcolor = "#000000";
    my $me;
    if (! defined $depth) { $depth = 1; }
    if (exists $seen{$i}) { return; }
    $seen{$i} = 1;
    if ($target eq "blocked") {
        $me = "dependson";
    } else {
        $me = "blocked";
    }
    SendSQL("select $target from dependencies where $me=$i order by $target");
    my @list;
    while (MoreSQLData()) {
        push(@list, FetchOneColumn());
    }
    if (@list) {
        my $list_started = 0;
        foreach my $kid (@list) {
            my ($bugid, $stat, $milestone) = ("", "", "");
            my ($userid, $short_desc) = ("", "");
            if (Param('usetargetmilestone')) {
                SendSQL("select bug_id, bug_status, target_milestone, assigned_to, short_desc from bugs where bug_id = $kid and bugs.groupset & $::usergroupset = bugs.groupset");
                ($bugid, $stat, $milestone, $userid, $short_desc) = (FetchSQLData());
            } else {
                SendSQL("select bug_id, bug_status, assigned_to, short_desc from bugs where bug_id = $kid and bugs.groupset & $::usergroupset = bugs.groupset");
                ($bugid, $stat, $userid, $short_desc) = (FetchSQLData());

            }
            if (! defined $bugid) { next; }
            my $opened = IsOpenedState($stat);
            if ($hide_resolved && ! $opened) { next; }

            # If we specify a maximum depth, we hide the output when
            # that depth has occured, but continue recursing so we know
            # the real maximum depth of the tree.
            if (! $maxdepth || $depth <= $maxdepth) {
                if (! $list_started) { $html .= "<ul>"; $list_started = 1 }
                $html .= "<li>"; 
                if (! $opened) {
                    $html .= qq|<strike><span style="color: $fgcolor; background-color: $bgcolor;">
                    |;
                }
                $short_desc = html_quote($short_desc);
                SendSQL("select login_name from profiles where userid = $userid");
                my ($owner) = (FetchSQLData());
                if ((Param('usetargetmilestone')) && ($milestone)) {
                    $html .= qq|
                    <a href="show_bug.cgi?id=$kid">$kid [$milestone, $owner]
                        - $short_desc.</a>
                    |;  
                } else {
                    $html .= qq|
                    <a href="show_bug.cgi?id=$kid">$kid [$owner] -
                        $short_desc.</a>\n|; 
                }
                if (! $opened) { $html .= "</span></strike>"; }
            } # End hideable output

            # Store the maximum depth so far
            $realdepth = $realdepth < $depth ? $depth : $realdepth;
            DumpKids($kid, $target, $depth + 1);
        }
        if ($list_started) { $html .= "</ul>"; }
    }
}

# makeTreeHTML calls DumpKids and generates the HTML output for a
# dependency/blocker section.
#
# makeTreeHTML(i, linked_id, target);
#
# Params
#   i: Bug id
#   linked_id: Linkified bug_id used to linkify the bug
#   target: The type we are looking for; either "blocked" or "dependson"
# Globals modified
#   html [Also modified in our call to DumpKids]
# Globals referenced
#   seen [Also modified by DumpKids]
#   maxdepth
#   realdepth
#
# Example:
#   $depend_html = makeTreeHTML(83058, <A HREF="...">83058</A>, "dependson");
#       Will generate HTML for bugs that depend on bug 83058

sub makeTreeHTML {
    my ($i, $linked_id, $target) = @_;

    # Clean up globals for this run
    $html = "";
    %seen = ();

    DumpKids($i, $target);
    my $tmphtml = $html;

    # Output correct heading
    $html = "<h3>Bugs that bug $linked_id ".($target eq "blocked" ? 
        "blocks" : "depends on");

    # Provide feedback for omitted bugs
    if ($maxdepth || $hide_resolved) {
        $html .= " <small><b>(Only ";
        if ($hide_resolved) { $html .= "open "; }
        $html .= "bugs ";
        if ($maxdepth) { $html .= "whose depth is less than $maxdepth "; }
        $html .= "will be shown)</b></small>";
    }

    $html .= "</h3>";
    $html .= $tmphtml;

    # If no bugs were found, say so
    if ((scalar keys %seen) < 2) {
        $html .= "&nbsp;&nbsp;&nbsp;&nbsp;None<p>\n";
    }

    return $html;
}

# Draw the actual form controls that make up the hide/show resolved and
# depth control toolbar. 
#
# drawDepForm()
#
# Params
#   none
# Globals modified
#   none
# Globals referenced
#   hide_resolved
#   maxdepth
#   realdepth

sub drawDepForm {
    my $bgcolor = "#d0d0d0";
    my ($hide_msg, $hide_input);

    # Set the text and action for the hide resolved button.
    if ($hide_resolved) {
        $hide_input = '<input type="hidden" name="hide_resolved" value="0">';
        $hide_msg = "Show Resolved";
    } else {
        $hide_input = '<input type="hidden" name="hide_resolved" value="1">';
        $hide_msg = "Hide Resolved";
    }

    print qq|
    <table cellpadding="3" border="0" cellspacing="0">
    <tr>

    <!-- Hide/show resolved button 
         Swaps text depending on the state of hide_resolved -->
    <td bgcolor="$bgcolor" align="center"> 
    <form method="get" action="$scriptname" 
        style="display: inline; margin: 0px;">
    <input name="id" type="hidden" value="$id">
    | . ( $maxdepth ? 
        qq|<input name="maxdepth" type="hidden" value="$maxdepth">|
        : "" ) . qq|
    $hide_input
    <input type="submit" value="$hide_msg">

    </form>
    </td>
    <td bgcolor="$bgcolor">

    <!-- depth section -->
    Depth:
    
    </td>
    <td bgcolor="$bgcolor">
    <form method="get" action="$scriptname" 
        style="display: inline; margin: 0px;">
    
    <!-- Unlimited button -->
    <input name="id" type="hidden" value="$id">
    <input name="hide_resolved" type="hidden" value="$hide_resolved">
    <input type="submit" value="&nbsp;Unlimited&nbsp;">

    </form>
    </td>
    <td bgcolor="$bgcolor">

    Limit to:
    
    </td>
    <td bgcolor="$bgcolor">
    <form method="get" action="$scriptname" 
        style="display: inline; margin: 0px;">

    <!-- Limit entry form: the button can't do anything when total depth
         is less than two, so disable it -->
    <input name="maxdepth" size="4" maxlength="4" value="| 
        . ( $maxdepth > 0 ? $maxdepth : "" ) . qq|">
    <input name="id" type="hidden" value="$id">
    <input name="hide_resolved" type="hidden"
        value="$hide_resolved">
    <input type="submit" value="Change" | 
        . ( $realdepth < 2 ? "disabled" : "" ) . qq|>

    </form>
    </td>
    <td bgcolor="$bgcolor">
    <form method="get" action="$scriptname" 
        style="display: inline; margin: 0px;">

    <!-- Minus one (-1) form  
         Allow subtracting only when realdepth and maxdepth > 1 -->
    <input name="id" type="hidden" value="$id">
    <input name="maxdepth" type="hidden" value="| 
        .  ( $maxdepth == 1 ? 1 : ( $maxdepth ? 
            $maxdepth-1 : $realdepth-1 ) ) . qq|">
    <input name="hide_resolved" type="hidden" value="$hide_resolved">
    <input type="submit" value="&nbsp;-1&nbsp;" | 
        . ( $realdepth < 2 || ( $maxdepth && $maxdepth < 2 ) ?
            "disabled" : "" ) . qq|>

    </form>
    </td>
    <td bgcolor="$bgcolor">
    <form method="get" action="$scriptname" 
        style="display: inline; margin: 0px;">
    
    <!-- plus one form +1 
        Disable button if total depth < 2, or if depth set to unlimited -->
    <input name="id" type="hidden" value="$id">
    | . ( $maxdepth ? qq|
    <input name="maxdepth" type="hidden" value="|.($maxdepth+1).qq|">| : "" ) 
    . qq| 
    <input name="hide_resolved" type="hidden" value="$hide_resolved">
    <input type="submit" value="&nbsp;+1&nbsp;" | 
        .  ( $realdepth < 2 || ! $maxdepth || $maxdepth >= $realdepth ?
            "disabled" : "" ) . qq|>

    </form>
    </td>
    <td bgcolor="$bgcolor">
    <form method="get" action="$scriptname" 
        style="display: inline; margin: 0px;">
    
    <!-- set to one form -->
    <input type="submit" value="Set to 1" | 
        . ( $realdepth < 2 || $maxdepth == 1 ? "disabled" : "" ) . qq|>
    <input name="id" type="hidden" value="$id">
    <input name="maxdepth" type="hidden" value="1">
    <input name="hide_resolved" type="hidden" value="$hide_resolved">
    </form>
    </td>
    </tr></table>
    |;
}

######################################################################
# Main Section
######################################################################

my $linked_id = qq|<a href="show_bug.cgi?id=$id">$id</a>|;

# Start the tree walk and save results. The tree walk generates HTML but
# needs to be called before the page output starts so we have the
# realdepth, which is necessary for generating the control toolbar.

# Get bugs we depend on
my $depend_html = makeTreeHTML($id, $linked_id, "dependson");

my $tmpdepth = $realdepth; 
$realdepth = 0;

# Get bugs we block
my $block_html = makeTreeHTML($id, $linked_id, "blocked");

# Select maximum depth found for use in the toolbar
$realdepth = $realdepth < $tmpdepth ? $tmpdepth : $realdepth;

#
# Actual page output happens here

print "Content-type: text/html\n\n";
PutHeader("Dependency tree for Bug $id", "Dependency tree for Bug $linked_id");
drawDepForm();
print $depend_html;
print $block_html;
drawDepForm();
PutFooter();
