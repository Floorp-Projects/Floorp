#!/usr/bin/perl -wT
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
use Bugzilla::Constants;

use vars qw($unconfirmedstate);

###########################################################################
# General subs
###########################################################################

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

#
# Parameter is a list of bug ids.
#
# Return is a string containing a list of all the bugids, as hrefs,
# followed by a link to them all as a buglist
sub BugListLinks {
    my @bugs = @_;

    # Historically, GetBugLink() wasn't used here. I'm guessing this
    # was because it didn't exist or is too performance heavy, or just
    # plain unnecessary
    my @bug_links = map(BugLink($_), @bugs);

    return join(', ',@bug_links) . " <a href=\"buglist.cgi?bug_id=" .
        join(',',@bugs) . "\">(as buglist)</a>";
}

###########################################################################
# Start
###########################################################################

Bugzilla->login(LOGIN_REQUIRED);

my $cgi = Bugzilla->cgi;

# Make sure the user is authorized to access sanitycheck.cgi.  Access
# is restricted to logged-in users who have "editbugs" privileges,
# which is a reasonable compromise between allowing all users to access
# the script (creating the potential for denial of service attacks)
# and restricting access to this installation's administrators (which
# prevents users with a legitimate interest in Bugzilla integrity
# from accessing the script).
UserInGroup("editbugs")
  || ThrowUserError("sanity_check_access_denied");

print "Content-type: text/html\n";
print "\n";

my @row;

PutHeader("Bugzilla Sanity Check");

###########################################################################
# Fix vote cache
###########################################################################

if (defined $cgi->param('rebuildvotecache')) {
    Status("OK, now rebuilding vote cache.");
    SendSQL("LOCK TABLES bugs WRITE, votes READ");
    SendSQL("UPDATE bugs SET votes = 0, delta_ts = delta_ts");
    SendSQL("SELECT bug_id, SUM(vote_count) FROM votes GROUP BY bug_id");
    my %votes;
    while (@row = FetchSQLData()) {
        my ($id, $v) = (@row);
        $votes{$id} = $v;
    }
    foreach my $id (keys %votes) {
        SendSQL("UPDATE bugs SET votes = $votes{$id}, delta_ts = delta_ts WHERE bug_id = $id");
    }
    SendSQL("UNLOCK TABLES");
    Status("Vote cache has been rebuilt.");
}

###########################################################################
# Fix group derivations
###########################################################################

if (defined $cgi->param('rederivegroups')) {
    Status("OK, All users' inherited permissions will be rechecked when " .
           "they next access Bugzilla.");
    SendSQL("UPDATE groups SET last_changed = NOW() LIMIT 1");
}

# rederivegroupsnow is REALLY only for testing.
# If it wasn't, then we'd do this the faster way as a per-group
# thing rather than per-user for group inheritance
if (defined $cgi->param('rederivegroupsnow')) {
    require Bugzilla::User;
    Status("OK, now rederiving groups.");
    SendSQL("SELECT userid FROM profiles");
    while ((my $id) = FetchSQLData()) {
        my $user = new Bugzilla::User($id);
        $user->derive_groups();
        Status("User $id");
    }
}

if (defined $cgi->param('cleangroupsnow')) {
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

if (defined $cgi->param('rescanallBugMail')) {
    require Bugzilla::BugMail;

    Status("OK, now attempting to send unsent mail");
    SendSQL("SELECT bug_id FROM bugs WHERE lastdiffed < delta_ts AND 
             delta_ts < date_sub(now(), INTERVAL 30 minute) ORDER BY bug_id");
    my @list;
    while (MoreSQLData()) {
        push (@list, FetchOneColumn());
    }

    Status(scalar(@list) . ' bugs found with possibly unsent mail.');

    foreach my $bugid (@list) {
        Bugzilla::BugMail::Send($bugid);
    }

    if (scalar(@list) > 0) {
        Status("Unsent mail has been sent.");
    }

    PutFooter();
    exit;
}

print "OK, now running sanity checks.<p>\n";

###########################################################################
# Check enumeration values
###########################################################################

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
        Alert("Bug(s) found with invalid $field value: ".
              BugListLinks(@invalid));
    }
}

###########################################################################
# Perform referential (cross) checks
###########################################################################

# This checks that a simple foreign key has a valid primary key value.  NULL
# references are acceptable and cause no problem.
#
# The first parameter is the primary key table name.
# The second parameter is the primary key field name.
# Each successive parameter represents a foreign key, it must be a list
# reference, where the list has:
#   the first value is the foreign key table name.
#   the second value is the foreign key field name.
#   the third value is optional and represents a field on the foreign key
#     table to display when the check fails.
#   the fourth value is optional and is a list reference to values that
#     are excluded from checking.
#
# FIXME: The excluded values parameter should go away - the QA contact
#        fields should use NULL instead - see bug #109474.

sub CrossCheck {
    my $table = shift @_;
    my $field = shift @_;

    Status("Checking references to $table.$field");

    while (@_) {
        my $ref = shift @_;
        my ($refertable, $referfield, $keyname, $exceptions) = @$ref;

        $exceptions ||= [];
        my %exceptions = map { $_ => 1 } @$exceptions;

        Status("... from $refertable.$referfield");
        
        SendSQL("SELECT DISTINCT $refertable.$referfield" . ($keyname ? ", $refertable.$keyname" : '') . " " .
                "FROM   $refertable LEFT JOIN $table " .
                "  ON   $refertable.$referfield = $table.$field " .
                "WHERE  $table.$field IS NULL " .
                "  AND  $refertable.$referfield IS NOT NULL");

        while (MoreSQLData()) {
            my ($value, $key) = FetchSQLData();
            if (!$exceptions{$value}) {
                my $alert = "Bad value $value found in $refertable.$referfield";
                if ($keyname) {
                    if ($keyname eq 'bug_id') {
                        $alert .= ' (bug ' . BugLink($key) . ')';
                    }
                    else {
                        $alert .= " ($keyname == '$key')";
                    }
                }
                Alert($alert);
            }
        }
    }
}

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
           ["group_control_map", "group_id"],
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
           ["group_control_map", "product_id"],
           ["flaginclusions", "product_id", "type_id"],
           ["flagexclusions", "product_id", "type_id"]);

###########################################################################
# Perform double field referential (cross) checks
###########################################################################
 
# This checks that a compound two-field foreign key has a valid primary key
# value.  NULL references are acceptable and cause no problem.
#
# The first parameter is the primary key table name.
# The second parameter is the primary key first field name.
# The third parameter is the primary key second field name.
# Each successive parameter represents a foreign key, it must be a list
# reference, where the list has:
#   the first value is the foreign key table name
#   the second value is the foreign key first field name.
#   the third value is the foreign key second field name.
#   the fourth value is optional and represents a field on the foreign key
#     table to display when the check fails

sub DoubleCrossCheck {
    my $table = shift @_;
    my $field1 = shift @_;
    my $field2 = shift @_;
 
    Status("Checking references to $table.$field1 / $table.$field2");
 
    while (@_) {
        my $ref = shift @_;
        my ($refertable, $referfield1, $referfield2, $keyname) = @$ref;
 
        Status("... from $refertable.$referfield1 / $refertable.$referfield2");
        
        SendSQL("SELECT DISTINCT $refertable.$referfield1, $refertable.$referfield2" . ($keyname ? ", $refertable.$keyname" : '') . " " .
                "FROM   $refertable LEFT JOIN $table " .
                "  ON   $refertable.$referfield1 = $table.$field1 " .
                " AND   $refertable.$referfield2 = $table.$field2 " .
                "WHERE  $table.$field1 IS NULL " .
                "  AND  $table.$field2 IS NULL " .
                "  AND  $refertable.$referfield1 IS NOT NULL " .
                "  AND  $refertable.$referfield2 IS NOT NULL");
 
        while (MoreSQLData()) {
            my ($value1, $value2, $key) = FetchSQLData();
 
            my $alert = "Bad values $value1, $value2 found in " .
                "$refertable.$referfield1 / $refertable.$referfield2";
            if ($keyname) {
                if ($keyname eq 'bug_id') {
                   $alert .= ' (bug ' . BugLink($key) . ')';
                }
                else {
                    $alert .= " ($keyname == '$key')";
                }
            }
            Alert($alert);
        }
    }
}

DoubleCrossCheck("components", "product_id", "id",
                 ["bugs", "product_id", "component_id", "bug_id"]);

DoubleCrossCheck("versions", "product_id", "value",
                 ["bugs", "product_id", "version", "bug_id"]);
 
DoubleCrossCheck("milestones", "product_id", "value",
                 ["bugs", "product_id", "target_milestone", "bug_id"],
                 ["products", "id", "defaultmilestone", "name"]);

###########################################################################
# Perform login checks
###########################################################################
 
Status("Checking profile logins");

my $emailregexp = Param("emailregexp");
SendSQL("SELECT userid, login_name FROM profiles");

while (my ($id,$email) = (FetchSQLData())) {
    unless ($email =~ m/$emailregexp/) {
        Alert "Bad profile email address, id=$id,  &lt;$email&gt;."
    }
}

###########################################################################
# Perform vote/keyword cache checks
###########################################################################

my $offervotecacherebuild = 0;

sub AlertBadVoteCache {
    my ($id) = (@_);
    Alert("Bad vote cache for bug " . BugLink($id));
    $offervotecacherebuild = 1;
}

SendSQL("SELECT bug_id, votes, keywords FROM bugs " .
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
SendSQL("SELECT bug_id, SUM(vote_count) FROM votes GROUP BY bug_id");

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

if (defined $cgi->param('rebuildkeywordcache')) {
    SendSQL("LOCK TABLES bugs write, keywords read, keyworddefs read");
}

SendSQL("SELECT keywords.bug_id, keyworddefs.name " .
        "FROM keywords, keyworddefs, bugs " .
        "WHERE keyworddefs.id = keywords.keywordid " .
        "  AND keywords.bug_id = bugs.bug_id " .
        "ORDER BY keywords.bug_id, keyworddefs.name");

my $lastb = 0;
my @list;
while (1) {
    my ($b, $k) = FetchSQLData();
    if (!defined $b || $b != $lastb) {
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

my @badbugs = ();

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
    Alert(scalar(@badbugs) . " bug(s) found with incorrect keyword cache: " .
          BugListLinks(@badbugs));
    if (defined $cgi->param('rebuildkeywordcache')) {
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

if (defined $cgi->param('rebuildkeywordcache')) {
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

    if (@badbugs) {
        Alert("$errortext: " . BugListLinks(@badbugs));
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

my $sqlunconfirmed = SqlQuote($unconfirmedstate);                            

BugCheck("bugs WHERE bug_status = $sqlunconfirmed AND everconfirmed = 1",
         "Bugs that are UNCONFIRMED but have everconfirmed set");
# The below list of resolutions is hardcoded because we don't know if future
# resolutions will be confirmed, unconfirmed or maybeconfirmed.  I suspect
# they will be maybeconfirmed, eg ASLEEP and REMIND.  This hardcoding should
# disappear when we have customised statuses.
BugCheck("bugs WHERE bug_status IN ('NEW', 'ASSIGNED', 'REOPENED') AND everconfirmed = 0",
         "Bugs with confirmed status but don't have everconfirmed set"); 

Status("Checking votes/everconfirmed");

BugCheck("bugs, products WHERE " .
         "bugs.product_id = products.id AND " .
         "everconfirmed = 0 AND " .
         "votestoconfirm <= votes",
         "Bugs that have enough votes to be confirmed but haven't been");

###########################################################################
# Date checks
###########################################################################

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
    
DateCheck("groups", "last_changed");
DateCheck("profiles", "refreshed_when");

###########################################################################
# Control Values
###########################################################################

# Checks for values that are invalid OR
# not among the 9 valid combinations
Status("Checking for bad values in group_control_map");
SendSQL("SELECT COUNT(product_id) FROM group_control_map WHERE " .
        "membercontrol NOT IN(" . CONTROLMAPNA . "," . CONTROLMAPSHOWN .
        "," . CONTROLMAPDEFAULT . "," . CONTROLMAPMANDATORY . ")" .
        " OR " .
        "othercontrol NOT IN(" . CONTROLMAPNA . "," . CONTROLMAPSHOWN .
        "," . CONTROLMAPDEFAULT . "," . CONTROLMAPMANDATORY . ")" .
        " OR " .
        "( (membercontrol != othercontrol) " .
          "AND (membercontrol != " . CONTROLMAPSHOWN . ") " .
          "AND ((membercontrol != " . CONTROLMAPDEFAULT . ") " .
            "OR (othercontrol = " . CONTROLMAPSHOWN . ")))");
my $c = FetchOneColumn();
if ($c) {
    Alert("Found $c bad group_control_map entries");
}

Status("Checking for bugs with groups violating their product's group controls");
BugCheck("bugs
         INNER JOIN bug_group_map ON bugs.bug_id = bug_group_map.bug_id
         INNER JOIN groups ON bug_group_map.group_id = groups.id
         LEFT JOIN group_control_map ON bugs.product_id = group_control_map.product_id
                                     AND bug_group_map.group_id = group_control_map.group_id
         WHERE groups.isactive != 0
         AND ((group_control_map.membercontrol = " . CONTROLMAPNA . ")
         OR (group_control_map.membercontrol IS NULL))",
         "Have groups not permitted for their products");

BugCheck("bugs
         INNER JOIN bug_group_map ON bugs.bug_id = bug_group_map.bug_id
         INNER JOIN groups ON bug_group_map.group_id = groups.id
         LEFT JOIN group_control_map ON bugs.product_id = group_control_map.product_id
                                     AND bug_group_map.group_id = group_control_map.group_id
         WHERE groups.isactive != 0
         AND group_control_map.membercontrol = " . CONTROLMAPMANDATORY . "
         AND bug_group_map.group_id IS NULL",
         "Are missing groups required for their products");


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
          BugListLinks(@badbugs));

    print qq{<a href="sanitycheck.cgi?rescanallBugMail=1">Send these mails</a>.<p>\n};
}

###########################################################################
# End
###########################################################################

Status("Sanity check completed.");
PutFooter();
