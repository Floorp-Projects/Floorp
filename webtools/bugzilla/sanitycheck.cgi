#!/usr/bonsaitools/bin/perl -w
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
#                 Matthew Tuck <matty@chariot.net.au>

use diagnostics;
use strict;

require "CGI.pl";

use vars %::FORM;

ConnectToDatabase();

confirm_login();

# Make sure the user is authorized to access sanitycheck.cgi.  Access
# is restricted to logged-in users who have "editbugs" privileges,
# which is a reasonable compromise between allowing all users to access
# the script (creating the potential for denial of service attacks)
# and restricting access to this installation's administrators (which
# prevents users with a legitimate interest in Bugzilla integrity
# from accessing the script).
UserInGroup("editbugs")
  || DisplayError("You are not authorized to access this script,
                   which is reserved for users with the ability to edit bugs.")
  && exit;

print "Content-type: text/html\n";
print "\n";

SendSQL("set SQL_BIG_TABLES=1");

my $offervotecacherebuild = 0;

sub Status {
    my ($str) = (@_);
    print "$str <P>\n";
}

sub Alert {
    my ($str) = (@_);
    Status("<font color=red>$str</font>");
}

sub BugLink {
    my ($id) = (@_);
    return "<a href='show_bug.cgi?id=$id'>$id</a>";
}

sub AlertBadVoteCache {
    my ($id) = (@_);
    Alert("Bad vote cache for bug " . BugLink($id));
    $offervotecacherebuild = 1;
}

sub CrossCheck {
    my $table = shift @_;
    my $field = shift @_;
    Status("Checking references to $table.$field");
    SendSQL("SELECT DISTINCT $field FROM $table");
    my %valid;
    while (MoreSQLData()) {
        $valid{FetchOneColumn()} = 1;
    }
    while (@_) {
        my $ref = shift @_;
        my ($t2, $f2, $key2, $exceptions) = @$ref;

        $exceptions ||= [];
        my %exceptions = map { $_ => 1 } @$exceptions;

        Status("... from $t2.$f2");
        
        SendSQL("SELECT DISTINCT $f2" . ($key2 ? ", $key2" : '') ." FROM $t2 "
                . "WHERE $f2 IS NOT NULL");
        while (MoreSQLData()) {
            my ($value, $key) = FetchSQLData();
            if (!$valid{$value} && !$exceptions{$value}) {
                my $alert = "Bad value $value found in $t2.$f2";
                if ($key2) {
                    if ($key2 eq 'bug_id') {
                        $alert .= qq{ (<a href="show_bug.cgi?id=$key">bug $key</a>)};
                    }
                    else {
                        $alert .= " ($key2 == '$key')";
                    }
                }
                Alert($alert);
            }
        }
    }
}

    
my @badbugs;


my @row;
my @checklist;

PutHeader("Bugzilla Sanity Check");

if (exists $::FORM{'rebuildvotecache'}) {
    Status("OK, now rebuilding vote cache.");
    SendSQL("lock tables bugs write, votes read");
    SendSQL("update bugs set votes = 0, delta_ts=delta_ts");
    SendSQL("select bug_id, sum(count) from votes group by bug_id");
    my %votes;
    while (@row = FetchSQLData()) {
        my ($id, $v) = (@row);
        $votes{$id} = $v;
    }
    foreach my $id (keys %votes) {
        SendSQL("update bugs set votes = $votes{$id}, delta_ts=delta_ts where bug_id = $id");
    }
    SendSQL("unlock tables");
    Status("Vote cache has been rebuilt.");
}

print "OK, now running sanity checks.<P>\n";

CrossCheck("keyworddefs", "id",
           ["keywords", "keywordid"]);

CrossCheck("fielddefs", "fieldid",
           ["bugs_activity", "fieldid"]);

CrossCheck("attachments", "attach_id",
           ["attachstatuses", "attach_id"],
           ["bugs_activity", "attach_id"]);

CrossCheck("attachstatusdefs", "id",
           ["attachstatuses", "statusid"]);

CrossCheck("bugs", "bug_id",
           ["bugs_activity", "bug_id"],
           ["attachments", "bug_id"],
           ["cc", "bug_id"],
           ["longdescs", "bug_id"],
           ["dependencies", "blocked"],
           ["dependencies", "dependson"],
           ["votes", "bug_id"],
           ["keywords", "bug_id"],
           ["duplicates", "dupe_of", "dupe"],
           ["duplicates", "dupe", "dupe_of"]);

CrossCheck("profiles", "userid",
           ["bugs", "reporter", "bug_id"],
           ["bugs", "assigned_to", "bug_id"],
           ["bugs", "qa_contact", "bug_id", ["0"]],
           ["attachments", "submitter_id", "bug_id"],
           ["bugs_activity", "who", "bug_id"],
           ["cc", "who", "bug_id"],
           ["votes", "who", "bug_id"],
           ["longdescs", "who", "bug_id"],
           ["logincookies", "userid"],
           ["namedqueries", "userid"],
           ["watch", "watcher"],
           ["watch", "watched"],
           ["tokens", "userid"],
           ["components", "initialowner", "value"],
           ["components", "initialqacontact", "value", ["0"]]);

CrossCheck("products", "product",
           ["bugs", "product", "bug_id"],
           ["components", "program", "value"],
           ["milestones", "product", "value"],
           ["versions", "program", "value"],
           ["attachstatusdefs", "product", "name"]);


Status("Checking groups");
SendSQL("select bit from groups where bit != pow(2, round(log(bit) / log(2)))");
while (my $bit = FetchOneColumn()) {
    Alert("Illegal bit number found in group table: $bit");
}
    
SendSQL("select sum(bit) from groups where isbuggroup != 0");
my $buggroupset = FetchOneColumn();
if (!defined $buggroupset || $buggroupset eq "") {
    $buggroupset = 0;
}
SendSQL("select bug_id, groupset from bugs where groupset & $buggroupset != groupset");
while (@row = FetchSQLData()) {
    Alert("Bad groupset $row[1] found in bug " . BugLink($row[0]));
}




Status("Checking version/products");

SendSQL("select distinct product, version from bugs");
while (@row = FetchSQLData()) {
    my @copy = @row;
    push(@checklist, \@copy);
}

foreach my $ref (@checklist) {
    my ($product, $version) = (@$ref);
    SendSQL("select count(*) from versions where program = " . SqlQuote($product) . " and value = " . SqlQuote($version));
    if (FetchOneColumn() != 1) {
        Alert("Bug(s) found with invalid product/version: $product/$version");
    }
}

# Adding check for Target Milestones / products - matthew@zeroknowledge.com
Status("Checking milestone/products");

@checklist = ();
SendSQL("select distinct product, target_milestone from bugs");
while (@row = FetchSQLData()) {
    my @copy = @row;
    push(@checklist, \@copy);
}

foreach my $ref (@checklist) {
    my ($product, $milestone) = (@$ref);
    SendSQL("SELECT count(*) FROM milestones WHERE product = " . SqlQuote($product) . " AND value = " . SqlQuote($milestone));
    if(FetchOneColumn() != 1) {
        Alert("Bug(s) found with invalid product/milestone: $product/$milestone");
    }
}


Status("Checking default milestone/products");

@checklist = ();
SendSQL("select product, defaultmilestone from products");
while (@row = FetchSQLData()) {
    my @copy = @row;
    push(@checklist, \@copy);
}

foreach my $ref (@checklist) {
    my ($product, $milestone) = (@$ref);
    SendSQL("SELECT count(*) FROM milestones WHERE product = " . SqlQuote($product) . " AND value = " . SqlQuote($milestone));
    if(FetchOneColumn() != 1) {
        Alert("Product(s) found with invalid default milestone: $product/$milestone");
    }
}


Status("Checking components/products");

@checklist = ();
SendSQL("select distinct product, component from bugs");
while (@row = FetchSQLData()) {
    my @copy = @row;
    push(@checklist, \@copy);
}

foreach my $ref (@checklist) {
    my ($product, $component) = (@$ref);
    SendSQL("select count(*) from components where program = " . SqlQuote($product) . " and value = " . SqlQuote($component));
    if (FetchOneColumn() != 1) {
        my $link = "buglist.cgi?product=" . url_quote($product) .
            "&component=" . url_quote($component);
        Alert(qq{Bug(s) found with invalid product/component: $product/$component (<a href="$link">bug list</a>)});
    }
}


Status("Checking profile logins");

my $emailregexp = Param("emailregexp");
$emailregexp =~ s/'/\\'/g;
SendSQL("SELECT userid, login_name FROM profiles " .
        "WHERE login_name NOT REGEXP '" . $emailregexp . "'");


while (my ($id,$email) = (FetchSQLData())) {
    Alert "Bad profile email address, id=$id,  &lt;$email&gt;."
}


SendSQL("SELECT bug_id,votes,keywords FROM bugs " .
        "WHERE votes != 0 OR keywords != ''");

my %votes;
my %bugid;
my %keyword;

while (@row = FetchSQLData()) {
    my($id, $v, $k) = (@row);
    if ($v != 0) {
        $votes{$id} = $v;
    }
    if ($k) {
        $keyword{$id} = $k;
    }
}

Status("Checking cached vote counts");
SendSQL("select bug_id, sum(count) from votes group by bug_id");

while (@row = FetchSQLData()) {
    my ($id, $v) = (@row);
    if ($v <= 0) {
        Alert("Bad vote sum for bug $id");
    } else {
        if (!defined $votes{$id} || $votes{$id} != $v) {
            AlertBadVoteCache($id);
        }
        delete $votes{$id};
    }
}
foreach my $id (keys %votes) {
    AlertBadVoteCache($id);
}

if ($offervotecacherebuild) {
    print qq{<a href="sanitycheck.cgi?rebuildvotecache=1">Click here to rebuild the vote cache</a><p>\n};
}


Status("Checking keywords table");

my %keywordids;
SendSQL("SELECT id, name FROM keyworddefs");
while (@row = FetchSQLData()) {
    my ($id, $name) = (@row);
    if ($keywordids{$id}) {
        Alert("Duplicate entry in keyworddefs for id $id");
    }
    $keywordids{$id} = 1;
    if ($name =~ /[\s,]/) {
        Alert("Bogus name in keyworddefs for id $id");
    }
}


SendSQL("SELECT bug_id, keywordid FROM keywords ORDER BY bug_id, keywordid");
my $lastid;
my $lastk;
while (@row = FetchSQLData()) {
    my ($id, $k) = (@row);
    if (!$keywordids{$k}) {
        Alert("Bogus keywordids $k found in keywords table");
    }
    if (defined $lastid && $id eq $lastid && $k eq $lastk) {
        Alert("Duplicate keyword ids found in bug " . BugLink($id));
    }
    $lastid = $id;
    $lastk = $k;
}

Status("Checking cached keywords");

my %realk;

if (exists $::FORM{'rebuildkeywordcache'}) {
    SendSQL("LOCK TABLES bugs write, keywords read, keyworddefs read");
}

SendSQL("SELECT keywords.bug_id, keyworddefs.name " .
        "FROM keywords, keyworddefs, bugs " .
        "WHERE keyworddefs.id = keywords.keywordid " .
        "  AND keywords.bug_id = bugs.bug_id " .
        "ORDER BY keywords.bug_id, keyworddefs.name");

my $lastb;
my @list;
while (1) {
    my ($b, $k) = (FetchSQLData());
    if (!defined $b || $b ne $lastb) {
        if (@list) {
            $realk{$lastb} = join(', ', @list);
        }
        if (!$b) {
            last;
        }
        $lastb = $b;
        @list = ();
    }
    push(@list, $k);
}

@badbugs = ();

foreach my $b (keys(%keyword)) {
    if (!exists $realk{$b} || $realk{$b} ne $keyword{$b}) {
        push(@badbugs, $b);
    }
}
foreach my $b (keys(%realk)) {
    if (!exists $keyword{$b}) {
        push(@badbugs, $b);
    }
}
if (@badbugs) {
    @badbugs = sort {$a <=> $b} @badbugs;
    Alert("Bug(s) found with incorrect keyword cache: " .
          join(', ', @badbugs));
    if (exists $::FORM{'rebuildkeywordcache'}) {
        Status("OK, now fixing keyword cache.");
        foreach my $b (@badbugs) {
            my $k = '';
            if (exists($realk{$b})) {
                $k = $realk{$b};
            }
            SendSQL("UPDATE bugs SET delta_ts = delta_ts, keywords = " .
                    SqlQuote($k) .
                    " WHERE bug_id = $b");
        }
        Status("Keyword cache fixed.");
    } else {
        print qq{<a href="sanitycheck.cgi?rebuildkeywordcache=1">Click here to rebuild the keyword cache</a><p>\n};
    }
}

if (exists $::FORM{'rebuildkeywordcache'}) {
    SendSQL("UNLOCK TABLES");
}

###########################################################################
# Perform duplicates table checks
###########################################################################

Status("Checking duplicates table");

SendSQL("SELECT bugs.bug_id " .
        "FROM bugs, duplicates " .
        "WHERE bugs.resolution != 'DUPLICATE' " .
        "  AND bugs.bug_id = duplicates.dupe " .
        "ORDER BY bugs.bug_id");

@badbugs = ();

while (@row = FetchSQLData()) {
    my ($id) = (@row);
    push (@badbugs, $id);
}

if (@badbugs) {
    Alert("Bug(s) found on duplicates table that are not marked duplicate: " .
          join(', ', @badbugs));
}

SendSQL("SELECT bugs.bug_id " .
        "FROM bugs LEFT JOIN duplicates ON bugs.bug_id = duplicates.dupe " .
        "WHERE bugs.resolution = 'DUPLICATE' AND duplicates.dupe IS NULL " .
        "ORDER BY bugs.bug_id");

@badbugs = ();

while (@row = FetchSQLData()) {
    my ($id) = (@row);
    push (@badbugs, $id);
}

if (@badbugs) {
    Alert("Bug(s) found marked resolved duplicate and not on duplicates " .
          "table: " . join(', ', @badbugs));
}

###########################################################################
# Perform status/resolution checks
###########################################################################

Status("Checking statuses/resolutions");

my @open_states = map(SqlQuote($_), OpenStates());
my $open_states = join(', ', @open_states);

@badbugs = ();

SendSQL("SELECT   bug_id FROM bugs " .
        "WHERE    bug_status IN ($open_states) " .
        "AND      resolution != '' " .
        "ORDER BY bug_id");

while (@row = FetchSQLData()) {
    my ($id) = (@row);
    push(@badbugs, $id);
}

if (@badbugs > 0) {
    Alert("Bugs with open status and a resolution: " .
          join (", ", @badbugs));
}

@badbugs = ();

SendSQL("SELECT   bug_id FROM bugs " .
        "WHERE    bug_status NOT IN ($open_states) " .
        "AND      resolution = '' " .
        "ORDER BY bug_id");

while (@row = FetchSQLData()) {
    my ($id) = (@row);
    push(@badbugs, $id);
}

if (@badbugs > 0) {
    Alert("Bugs with non-open status and no resolution: " .
          join (", ", @badbugs));
}

###########################################################################
# Perform status/everconfirmed checks
###########################################################################

Status("Checking statuses/everconfirmed");

@badbugs = ();

SendSQL("SELECT   bug_id FROM bugs " .
        "WHERE    bug_status = " . SqlQuote($::unconfirmedstate) . ' ' .
        "AND      everconfirmed = 1 " .
        "ORDER BY bug_id");

while (@row = FetchSQLData()) {
    my ($id) = (@row);
    push(@badbugs, $id);
}

if (@badbugs > 0) {
    Alert("Bugs that are UNCONFIRMED but have everconfirmed set: " .
          join (", ", @badbugs));
}

@badbugs = ();

SendSQL("SELECT   bug_id FROM bugs " .
        "WHERE    bug_status IN ('NEW', 'ASSIGNED', 'REOPENED') " .
        "AND      everconfirmed = 0 " .
        "ORDER BY bug_id");

while (@row = FetchSQLData()) {
    my ($id) = (@row);
    push(@badbugs, $id);
}

if (@badbugs > 0) {
    Alert("Bugs with confirmed status but don't have everconfirmed set: " .
          join (", ", @badbugs));
}

###########################################################################
# Perform vote/everconfirmed checks
###########################################################################

Status("Checking votes/everconfirmed");

@badbugs = ();

SendSQL("SELECT   bug_id FROM bugs, products " .
        "WHERE    bugs.product = products.product " .
        "AND      bug_status = " . SqlQuote($::unconfirmedstate) . ' ' .
        "AND      votestoconfirm <= votes " .
        "ORDER BY bug_id");

while (@row = FetchSQLData()) {
    my ($id) = (@row);
    push(@badbugs, $id);
}

if (@badbugs > 0) {
    Alert("Bugs that have enough votes to be confirmed but haven't been: " .
          join (", ", @badbugs));
}

###########################################################################
# End
###########################################################################

Status("Sanity check completed.");
PutFooter();
