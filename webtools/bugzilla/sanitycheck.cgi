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
#                 Matthew Tuck <matty@chariot.net.au>

use strict;

use lib qw(.);

require "CGI.pl";

use vars qw(%FORM $unconfirmedstate);

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

my $offervotecacherebuild = 0;

sub Status {
    my ($str) = (@_);
    print "$str <p>\n";
}

sub Alert {
    my ($str) = (@_);
    Status("<font color=\"red\">$str</font>");
}

sub BugLink {
    my ($id) = (@_);
    return "<a href=\"show_bug.cgi?id=$id\">$id</a>";
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

sub DateCheck {
    my $table = shift @_;
    my $field = shift @_;
    Status("Checking dates in $table.$field");
    SendSQL("SELECT COUNT( $field ) FROM $table WHERE $field > NOW()");
    my $c = FetchOneColumn();
    if ($c) {
        Alert("Found $c dates in future");
    }
}

    
my @badbugs;


my @row;
my @checklist;

PutHeader("Bugzilla Sanity Check");

###########################################################################
# Fix vote cache
###########################################################################

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

if (exists $::FORM{'rederivegroups'}) {
    Status("OK, All users' inherited permissions will be rechecked when " .
           "they next access Bugzilla.");
    SendSQL("UPDATE groups SET last_changed = NOW() LIMIT 1");
}

# rederivegroupsnow is REALLY only for testing.
if (exists $::FORM{'rederivegroupsnow'}) {
    Status("OK, now rederiving groups.");
    SendSQL("SELECT userid FROM profiles");
    while ((my $id) = FetchSQLData()) {
        DeriveGroup($id);
        Status("Group $id");
    }
}

if (exists $::FORM{'cleangroupsnow'}) {
    Status("OK, now cleaning stale groups.");
    # Only users that were out of date already long ago should be cleaned
    # and the cleaning is done with tables locked.  This is require in order
    # to keep another session from proceeding with permission checks
    # after the groups have been cleaned unless it first had an opportunity
    # to get the groups up to date.
    # If any page starts taking longer than one hour to load, this interval
    # should be revised.
    SendSQL("SELECT MAX(last_changed) FROM groups WHERE last_changed < NOW() - INTERVAL 1 HOUR");
    (my $cutoff) = FetchSQLData();
    Status("Cutoff is $cutoff");
    SendSQL("SELECT COUNT(*) FROM user_group_map");
    (my $before) = FetchSQLData();
    SendSQL("LOCK TABLES user_group_map WRITE, profiles WRITE");
    SendSQL("SELECT userid FROM profiles " .
            "WHERE refreshed_when > 0 " .
            "AND refreshed_when < " . SqlQuote($cutoff) .
            " LIMIT 1000");
    my $count = 0;
    while ((my $id) = FetchSQLData()) {
        $count++;
        PushGlobalSQLState();
        SendSQL("DELETE FROM user_group_map WHERE " .
            "user_id = $id AND isderived = 1 AND isbless = 0");
        SendSQL("UPDATE profiles SET refreshed_when = 0 WHERE userid = $id");
        PopGlobalSQLState();
    }
    SendSQL("UNLOCK TABLES");
    SendSQL("SELECT COUNT(*) FROM user_group_map");
    (my $after) = FetchSQLData();
    Status("Cleaned table for $count users " .
           "- reduced from $before records to $after records");
}

print "OK, now running sanity checks.<p>\n";

# This one goes first, because if this is wrong, then the below tests
# will probably fail too

# This isn't extensible. Thats OK; we're not adding any more enum fields
Status("Checking for invalid enumeration values");
foreach my $field (("bug_severity", "bug_status", "op_sys",
                    "priority", "rep_platform", "resolution")) {
    # undefined enum values in mysql are an empty string which equals 0
    SendSQL("SELECT bug_id FROM bugs WHERE $field=0 ORDER BY bug_id");
    my @invalid;
    while (MoreSQLData()) {
        push (@invalid, FetchOneColumn());
    }
    if (@invalid) {
        Alert("Bug(s) found with invalid $field value: ".join(', ',@invalid));
    }
}

###########################################################################
# Perform referential (cross) checks
###########################################################################

CrossCheck("keyworddefs", "id",
           ["keywords", "keywordid"]);

CrossCheck("fielddefs", "fieldid",
           ["bugs_activity", "fieldid"]);

CrossCheck("attachments", "attach_id",
           ["flags", "attach_id"],
           ["bugs_activity", "attach_id"]);

CrossCheck("flagtypes", "id",
           ["flags", "type_id"]);

CrossCheck("bugs", "bug_id",
           ["bugs_activity", "bug_id"],
           ["bug_group_map", "bug_id"],
           ["attachments", "bug_id"],
           ["cc", "bug_id"],
           ["longdescs", "bug_id"],
           ["dependencies", "blocked"],
           ["dependencies", "dependson"],
           ["votes", "bug_id"],
           ["keywords", "bug_id"],
           ["duplicates", "dupe_of", "dupe"],
           ["duplicates", "dupe", "dupe_of"]);

CrossCheck("groups", "id",
           ["bug_group_map", "group_id"],
           ["group_group_map", "grantor_id"],
           ["group_group_map", "member_id"],
           ["user_group_map", "group_id"]);

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
           ["components", "initialowner", "name"],
           ["user_group_map", "user_id"],
           ["components", "initialqacontact", "name", ["0"]]);

CrossCheck("products", "id",
           ["bugs", "product_id", "bug_id"],
           ["components", "product_id", "name"],
           ["milestones", "product_id", "value"],
           ["versions", "product_id", "value"],
           ["flaginclusions", "product_id", "type_id"],
           ["flagexclusions", "product_id", "type_id"]);

DateCheck("groups", "last_changed");
DateCheck("profiles", "refreshed_when");


###########################################################################
# Perform product specific field checks
###########################################################################

Status("Checking version/products");

SendSQL("select distinct product_id, version from bugs");
while (@row = FetchSQLData()) {
    my @copy = @row;
    push(@checklist, \@copy);
}

foreach my $ref (@checklist) {
    my ($product_id, $version) = (@$ref);
    SendSQL("select count(*) from versions where product_id = $product_id and value = " . SqlQuote($version));
    if (FetchOneColumn() != 1) {
        Alert("Bug(s) found with invalid product ID/version: $product_id/$version");
    }
}

# Adding check for Target Milestones / products - matthew@zeroknowledge.com
Status("Checking milestone/products");

@checklist = ();
SendSQL("select distinct product_id, target_milestone from bugs");
while (@row = FetchSQLData()) {
    my @copy = @row;
    push(@checklist, \@copy);
}

foreach my $ref (@checklist) {
    my ($product_id, $milestone) = (@$ref);
    SendSQL("SELECT count(*) FROM milestones WHERE product_id = $product_id AND value = " . SqlQuote($milestone));
    if(FetchOneColumn() != 1) {
        Alert("Bug(s) found with invalid product ID/milestone: $product_id/$milestone");
    }
}


Status("Checking default milestone/products");

@checklist = ();
SendSQL("select id, defaultmilestone from products");
while (@row = FetchSQLData()) {
    my @copy = @row;
    push(@checklist, \@copy);
}

foreach my $ref (@checklist) {
    my ($product_id, $milestone) = (@$ref);
    SendSQL("SELECT count(*) FROM milestones WHERE product_id = $product_id AND value = " . SqlQuote($milestone));
    if(FetchOneColumn() != 1) {
        Alert("Product(s) found with invalid default milestone: $product_id/$milestone");
    }
}


Status("Checking components/products");

@checklist = ();
SendSQL("select distinct product_id, component_id from bugs");
while (@row = FetchSQLData()) {
    my @copy = @row;
    push(@checklist, \@copy);
}

foreach my $ref (@checklist) {
    my ($product_id, $component_id) = (@$ref);
    SendSQL("select count(*) from components where product_id = $product_id and id = $component_id");
    if (FetchOneColumn() != 1) {
        Alert(qq{Bug(s) found with invalid product/component ID: $product_id/$component_id});
    }
}

###########################################################################
# Perform login checks
###########################################################################
 
Status("Checking profile logins");

my $emailregexp = Param("emailregexp");
$emailregexp =~ s/'/\\'/g;
SendSQL("SELECT userid, login_name FROM profiles " .
        "WHERE login_name NOT REGEXP '" . $emailregexp . "'");


while (my ($id,$email) = (FetchSQLData())) {
    Alert "Bad profile email address, id=$id,  &lt;$email&gt;."
}

###########################################################################
# Perform vote/keyword cache checks
###########################################################################

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
# General bug checks
###########################################################################

sub BugCheck ($$) {
    my ($middlesql, $errortext) = @_;
    
    SendSQL("SELECT DISTINCT bugs.bug_id " .
            "FROM $middlesql " .
            "ORDER BY bugs.bug_id");
    
    my @badbugs = ();
    
    while (@row = FetchSQLData()) {
        my ($id) = (@row);
        push (@badbugs, $id);
    }
    
    @badbugs = map(BugLink($_), @badbugs);
    
    if (@badbugs) {
        Alert("$errortext: " . join(', ', @badbugs));
    }
}

Status("Checking resolution/duplicates");

BugCheck("bugs, duplicates WHERE " .
         "bugs.resolution != 'DUPLICATE' AND " .
         "bugs.bug_id = duplicates.dupe",
         "Bug(s) found on duplicates table that are not marked duplicate");

BugCheck("bugs LEFT JOIN duplicates ON bugs.bug_id = duplicates.dupe WHERE " .
         "bugs.resolution = 'DUPLICATE' AND " .
         "duplicates.dupe IS NULL",
         "Bug(s) found marked resolved duplicate and not on duplicates table");

Status("Checking statuses/resolutions");

my @open_states = map(SqlQuote($_), OpenStates());
my $open_states = join(', ', @open_states);

BugCheck("bugs WHERE bug_status IN ($open_states) AND resolution != ''",
         "Bugs with open status and a resolution");
BugCheck("bugs WHERE bug_status NOT IN ($open_states) AND resolution = ''",
         "Bugs with non-open status and no resolution");

Status("Checking statuses/everconfirmed");

BugCheck("bugs WHERE bug_status = $unconfirmedstate AND everconfirmed = 1",
         "Bugs that are UNCONFIRMED but have everconfirmed set");
# The below list of resolutions is hardcoded because we don't know if future
# resolutions will be confirmed, unconfirmed or maybeconfirmed.  I suspect
# they will be maybeconfirmed, eg ASLEEP and REMIND.  This hardcoding should
# disappear when we have customised statuses.
BugCheck("bugs WHERE bug_status IN ('NEW', 'ASSIGNED', 'REOPENED') AND everconfirmed = 0",
         "Bugs with confirmed status but don't have everconfirmed set"); 

Status("Checking votes/everconfirmed");

BugCheck("bugs, products WHERE " .
         "bugs.product = products.product AND " .
         "everconfirmed = 0 AND " .
         "votestoconfirm <= votes",
         "Bugs that have enough votes to be confirmed but haven't been");

###########################################################################
# Unsent mail
###########################################################################

Status("Checking for unsent mail");

@badbugs = ();

SendSQL("SELECT bug_id " .
        "FROM bugs WHERE lastdiffed < delta_ts AND ".
        "delta_ts < date_sub(now(), INTERVAL 30 minute) ".
        "ORDER BY bug_id");

while (@row = FetchSQLData()) {
    my ($id) = (@row);
    push(@badbugs, $id);
}

if (@badbugs > 0) {
    Alert("Bugs that have changes but no mail sent for at least half an hour: " .
          join (", ", @badbugs));
    print("Run <code>processmail rescanall</code> to fix this<p>\n");
}

###########################################################################
# End
###########################################################################

Status("Sanity check completed.");
PutFooter();
