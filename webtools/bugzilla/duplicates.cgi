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


use strict;

use AnyDBM_File;

use lib qw(.);

require "globals.pl";
require "CGI.pl";

use vars qw($buffer);

use Bugzilla;
use Bugzilla::Search;
use Bugzilla::CGI;

# Go directly to the XUL version of the duplicates report (duplicates.xul)
# if the user specified ctype=xul.  Adds params if they exist, and directs
# the user to a signed copy of the script in duplicates.jar if it exists.
if ($::FORM{'ctype'} && $::FORM{'ctype'} eq "xul") {
    my $params = CanonicaliseParams($::buffer, ["format", "ctype"]);
    print "Location: " . (-e "duplicates.jar" ? "duplicates.jar!/" : "") . 
          "duplicates.xul" . ($params ? "?$params" : "") . "\n\n";
    exit;
}

# Use global templatisation variables.
use vars qw($template $vars);

ConnectToDatabase();
GetVersionTable();

quietly_check_login();

Bugzilla->switch_to_shadow_db();

use vars qw (%FORM $userid @legal_product);

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

my $product_id;
if ($product) {
    $product_id = get_product_id($product);
    if (!$product_id) {
        $vars->{'product'} = $product;
        ThrowUserError("invalid_product_name");
    }
}

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
            $vars->{'today'} = $today;
            if ($!{ENOENT}) {
                ThrowUserError("no_dupe_stats");
            } else {
                $vars->{'error_msg'} = $!;
                ThrowUserError("no_dupe_stats_error_yesterday");
            }
        }
    } else {
        $vars->{'error_msg'} = $!;
        ThrowUserError("no_dupe_stats_error_today");
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
        $vars->{'error_msg'} = $!;
        $vars->{'changedsince'} = $changedsince;
        $vars->{'whenever'} = $whenever;    
        ThrowUserError("no_dupe_stats_error_whenever");
    }
} else {
    # Calculate the deltas
    ($delta{$_} = $count{$_} - ($before{$_} || 0)) foreach (keys(%count));

    $dobefore = 1;
}

detaint_natural($maxrows)
  || ThrowUserError("invalid_maxrows", { maxrows => $maxrows});

my @bugs;
my @bug_ids; 

if (scalar(%count)) {
    # use Bugzilla::Search so that we get the security checking
    my $params = new Bugzilla::CGI({ 'bug_id' => [keys %count] });

    if ($openonly) {
        $params->param('resolution', '---');
    } else {
        # We want to show bugs which:
        # a) Aren't CLOSED; and
        # b)  i) Aren't VERIFIED; OR
        #    ii) Were resolved INVALID/WONTFIX

        # The rationale behind this is that people will eventually stop
        # reporting fixed bugs when they get newer versions of the software,
        # but if the bug is determined to be erroneous, people will still
        # keep reporting it, so we do need to show it here.

        # a)
        $params->param('field0-0-0', 'bug_status');
        $params->param('type0-0-0', 'notequals');
        $params->param('value0-0-0', 'CLOSED');

        # b) i)
        $params->param('field0-1-0', 'bug_status');
        $params->param('type0-1-0', 'notequals');
        $params->param('value0-1-0', 'VERIFIED');

        # b) ii)
        $params->param('field0-1-1', 'resolution');
        $params->param('type0-1-1', 'anyexact');
        $params->param('value0-1-1', 'INVALID,WONTFIX');
    }

    # Restrict to product if requested
    if ($::FORM{'product'}) {
        $params->param('product', $::FORM{'product'});
    }

    my $query = new Bugzilla::Search('fields' => [qw(bugs.bug_id
                                                     map_components.name
                                                     bugs.bug_severity
                                                     bugs.op_sys
                                                     bugs.target_milestone
                                                     bugs.short_desc
                                                     bugs.bug_status
                                                     bugs.resolution
                                                    )
                                                 ],
                                     'params' => $params,
                                    );

    SendSQL($query->getSQL());

    while (MoreSQLData()) {
        # Note: maximum row count is dealt with in the template.

        my ($id, $component, $bug_severity, $op_sys, $target_milestone, 
            $short_desc, $bug_status, $resolution) = FetchSQLData();

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


my $format = 
  GetFormat("reports/duplicates", $::FORM{'format'}, $::FORM{'ctype'});
 
print "Content-Type: $format->{'ctype'}\n\n";

# Generate and return the UI (HTML page) from the appropriate template.
$template->process($format->{'template'}, $vars)
  || ThrowTemplateError($template->error());


sub days_ago {
    my ($dom, $mon, $year) = (localtime(time - ($_[0]*24*60*60)))[3, 4, 5];
    return sprintf "%04d-%02d-%02d", 1900 + $year, ++$mon, $dom;
}
