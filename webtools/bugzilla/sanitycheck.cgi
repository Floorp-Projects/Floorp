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

use diagnostics;
use strict;

require "CGI.pl";

use vars %::FORM;

print "Content-type: text/html\n";
print "\n";

ConnectToDatabase();

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
        my $t2 = shift @$ref;
        my $f2 = shift @$ref;
        my %exceptions;
        foreach my $v (@$ref) {
            $exceptions{$v} = 1;
        }
        Status("... from $t2.$f2");
        SendSQL("SELECT DISTINCT $f2 FROM $t2");
        while (MoreSQLData()) {
            my $value = FetchOneColumn();
            if (!$valid{$value} && !$exceptions{$value}) {
                Alert("Bad value $value found in $t2.$f2");
            }
        }
    }
}

    


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
           

CrossCheck("bugs", "bug_id",
           ["bugs_activity", "bug_id"],
           ["attachments", "bug_id"],
           ["cc", "bug_id"],
           ["longdescs", "bug_id"],
           ["dependencies", "blocked"],
           ["dependencies", "dependson"],
           ["votes", "bug_id"],
           ["keywords", "bug_id"]);

CrossCheck("profiles", "userid",
           ["bugs", "reporter"],
           ["bugs", "assigned_to"],
           ["bugs", "qa_contact", 0],
           ["attachments", "submitter_id"],
           ["bugs_activity", "who"],
           ["cc", "who"],
           ["votes", "who"],
           ["longdescs", "who"],
           ["namedqueries", "userid"]);


Status("Checking passwords");
SendSQL("SELECT COUNT(*) FROM profiles WHERE cryptpassword != ENCRYPT(password, left(cryptpassword, 2))");
my $count = FetchOneColumn();
if ($count) {
    Alert("$count entries have problems in their crypted password.");
    if ($::FORM{'rebuildpasswords'}) {
        Status("Rebuilding passwords");
        SendSQL("UPDATE profiles
                 SET cryptpassword = ENCRYPT(password,
                                            left(cryptpassword, 2))
                 WHERE cryptpassword != ENCRYPT(password,
                                                left(cryptpassword, 2))");
        Status("Passwords have been rebuilt.");
    } else {
        print qq{<a href="sanitycheck.cgi?rebuildpasswords=1">Click here to rebuild the crypted passwords</a><p>\n};
    }
}



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
    SendSQL("select count(*) from versions where program = '$product' and value = '$version'");
    if (FetchOneColumn() != 1) {
        Alert("Bug(s) found with invalid product/version: $product/$version");
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
    SendSQL("select count(*) from components where program = '$product' and value = '$component'");
    if (FetchOneColumn() != 1) {
        Alert("Bug(s) found with invalid product/component: $product/$component");
    }
}


Status("Checking profile logins");

my $emailregexp = Param("emailregexp");

SendSQL("SELECT userid, login_name FROM profiles " .
        "WHERE login_name NOT REGEXP " . SqlQuote($emailregexp));


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
        "FROM keywords, keyworddefs " .
        "WHERE keyworddefs.id = keywords.keywordid " .
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

my @fixlist;
foreach my $b (keys(%keyword)) {
    if (!exists $realk{$b} || $realk{$b} ne $keyword{$b}) {
        push(@fixlist, $b);
    }
}
foreach my $b (keys(%realk)) {
    if (!exists $keyword{$b}) {
        push(@fixlist, $b);
    }
}
if (@fixlist) {
    @fixlist = sort {$a <=> $b} @fixlist;
    Alert("Bug(s) found with incorrect keyword cache: " .
          join(', ', @fixlist));
    if (exists $::FORM{'rebuildkeywordcache'}) {
        Status("OK, now fixing keyword cache.");
        foreach my $b (@fixlist) {
            my $k = '';
            if (exists($realk{$b})) {
                $k = $realk{$b};
            }
            SendSQL("UPDATE bugs SET delta_ts = delta_ts, keywords = " .
                    SqlQuote($k) .
                    " WHERE bug_id = $b");
        }
        SendSQL("UNLOCK TABLES");
        Status("Keyword cache fixed.");
    } else {
        print qq{<a href="sanitycheck.cgi?rebuildkeywordcache=1">Click here to rebuild the keyword cache</a><p>\n};
    }
}




Status("Sanity check completed.");
PutFooter();
