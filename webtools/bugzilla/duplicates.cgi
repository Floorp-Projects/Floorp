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
# Contributor(s): Gervase Markham <gerv@gerv.net>
#
# Generates mostfreq list from data collected by collectstats.pl.


use diagnostics;
use strict;

use AnyDBM_File;

use lib qw(.);

require "globals.pl";
require "CGI.pl";

# Use global templatisation variables.
use vars qw($template $vars);

ConnectToDatabase(1);
GetVersionTable();

quietly_check_login();

use vars qw (%FORM $userid $usergroupset @legal_product);

my %dbmcount;
my %count;
my %before;

# Get params from URL
sub formvalue {
    my ($name, $default) = (@_);
    return $FORM{$name} || $default || "";
}

my $sortby = formvalue("sortby");
my $changedsince = formvalue("changedsince", 7);
my $maxrows = formvalue("maxrows", 100);
my $openonly = formvalue("openonly");
my $reverse = formvalue("reverse");
my $product = formvalue("product");
my $sortvisible = formvalue("sortvisible");
my @buglist = (split(/[:,]/, formvalue("bug_id")));

# Small backwards-compatibility hack, dated 2002-04-10.
$sortby = "count" if $sortby eq "dup_count";

# Open today's record of dupes
my $today = days_ago(0);
my $yesterday = days_ago(1);

# We don't know the exact file name, because the extention depends on the
# underlying dbm library, which could be anything. We can't glob, because
# perl < 5.6 considers if (<*>) { ... } to be tainted
# Instead, just check the return value for today's data and yesterday's,
# and ignore file not found errors

use Errno;
use Fcntl;

if (!tie(%dbmcount, 'AnyDBM_File', "data/duplicates/dupes$today",
         O_RDONLY, 0644)) {
    if ($!{ENOENT}) {
        if (!tie(%dbmcount, 'AnyDBM_File', "data/duplicates/dupes$yesterday",
                 O_RDONLY, 0644)) {
            if ($!{ENOENT}) {
                ThrowUserError("There are no duplicate statistics for today " .
                               "($today) or yesterday.",
                               "Cannot find duplicate statistics");
            } else {
                ThrowUserError("There are no duplicate statistics for today " .
                               "($today), and an error occurred when " .
                               "accessing yesterday's dupes file: $!.",
                               "Error reading yesterday's dupes file");
            }
        }
    } else {
        ThrowUserError("An error occurred when accessing today ($today)'s " .
                       "dupes file: $!.",
                       "Error reading today's dupes file");
    }
}

# Copy hash (so we don't mess up the on-disk file when we remove entries)
%count = %dbmcount;

# Remove all those dupes under the threshold parameter. 
# We do this, before the sorting, for performance reasons.
my $threshold = Param("mostfreqthreshold");

while (my ($key, $value) = each %count) {
    delete $count{$key} if ($value < $threshold);
    
    # If there's a buglist, restrict the bugs to that list.
    delete $count{$key} if $sortvisible && (lsearch(\@buglist, $key) == -1);
}

# Try and open the database from "changedsince" days ago
my $dobefore = 0;
my %delta;
my $whenever = days_ago($changedsince);    

if (!tie(%before, 'AnyDBM_File', "data/duplicates/dupes$whenever",
         O_RDONLY, 0644)) {
    # Ignore file not found errors
    if (!$!{ENOENT}) {
        ThrowUserError("Can't open $changedsince days ago ($whenever)'s " .
                       "dupes file: $!",
                       "Error reading previous dupes file");
    }
} else {
    # Calculate the deltas
    ($delta{$_} = $count{$_} - $before{$_}) foreach (keys(%count));

    $dobefore = 1;
}

# Don't add CLOSED, and don't add VERIFIED unless they are INVALID or 
# WONTFIX. We want to see VERIFIED INVALID and WONTFIX because common 
# "bugs" which aren't bugs end up in this state.
my $query = "
  SELECT bugs.bug_id, component, bug_severity, op_sys, target_milestone,
         short_desc, bug_status, resolution
  FROM bugs 
  WHERE (bug_status != 'CLOSED') 
  AND   ((bug_status = 'VERIFIED' AND resolution IN ('INVALID', 'WONTFIX')) 
         OR (bug_status != 'VERIFIED'))
  AND bugs.bug_id IN (" . join(", ", keys %count) . ")";

# Limit to a single product if requested             
$query .= (" AND product = " . SqlQuote($product)) if $product;
 
SendSQL(SelectVisible($query, 
                      $userid, 
                      $usergroupset));
                       
my @bugs;
my @bug_ids; 

while (MoreSQLData()) {
    # Note: maximum row count is dealt with in the template.

    my ($id, $component, $bug_severity, $op_sys, $target_milestone, 
        $short_desc, $bug_status, $resolution) = FetchSQLData();

    # Limit to open bugs only if requested
    next if $openonly && ($resolution ne "");

    push (@bugs, { id => $id,
                   count => $count{$id},
                   delta => $delta{$id}, 
                   component => $component,
                   bug_severity => $bug_severity,
                   op_sys => $op_sys,
                   target_milestone => $target_milestone,
                   short_desc => $short_desc,
                   bug_status => $bug_status, 
                   resolution => $resolution });
    push (@bug_ids, $id); 
}

$vars->{'bugs'} = \@bugs;
$vars->{'bug_ids'} = \@bug_ids;

$vars->{'dobefore'} = $dobefore;
$vars->{'sortby'} = $sortby;
$vars->{'sortvisible'} = $sortvisible;
$vars->{'changedsince'} = $changedsince;
$vars->{'maxrows'} = $maxrows;
$vars->{'openonly'} = $openonly;
$vars->{'reverse'} = $reverse;
$vars->{'format'} = $::FORM{'format'};
$vars->{'product'} = $product;
$vars->{'products'} = \@::legal_product;


my $format = ValidateOutputFormat($::FORM{'format'}, "duplicates", "reports");
 
print "Content-Type: $format->{'contenttype'}\n\n";

# Generate and return the UI (HTML page) from the appropriate template.
$template->process("reports/$format->{'template'}", $vars)
  || ThrowTemplateError($template->error());


sub days_ago {
    my ($dom, $mon, $year) = (localtime(time - ($_[0]*24*60*60)))[3, 4, 5];
    return sprintf "%04d-%02d-%02d", 1900 + $year, ++$mon, $dom;
}
