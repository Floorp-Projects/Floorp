#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>

use diagnostics;
use strict;

require "CGI.pl";

confirm_login();

print "Content-type: text/html\n\n";

ConnectToDatabase();
GetVersionTable();

my $who = DBNameToIdAndCheck($::COOKIE{'Bugzilla_login'});

if ($who ne $::FORM{'who'}) {
    PutHeader("Wrong login.");
    print "The login info got confused.  If you want to adjust the votes\n";
    print "for <tt>$::COOKIE{'Bugzilla_login'}</tt>, then please\n";
    print "<a href=showvotes.cgi?user=$who>click here</a>.<hr>\n";
    navigation_header();
    exit();
}

my @buglist = grep {/^\d+$/} keys(%::FORM);

if (0 == @buglist) {
    PutHeader("Oops?");
    print "Something got confused.  Please click <b>Back</b> and try again.";
    navigation_header();
    exit();
}

foreach my $id (@buglist) {
    $::FORM{$id} = trim($::FORM{$id});
    if ($::FORM{$id} !~ /\d+/ || $::FORM{$id} < 0) {
        PutHeader("Numbers only, please");
        print "Only use numeric values for your bug votes.\n";
        print "Please click <b>Back</b> and try again.<hr>\n";
        navigation_header();
        exit();
    }
}

SendSQL("select bug_id, product from bugs where bug_id = " .
        join(" or bug_id = ", @buglist));

my %prodcount;

while (MoreSQLData()) {
    my ($id, $prod) = (FetchSQLData());
    if (!defined $prodcount{$prod}) {
        $prodcount{$prod} = 0;
    }
    $prodcount{$prod} += $::FORM{$id};
}

foreach my $prod (keys(%prodcount)) {
    if ($prodcount{$prod} > $::prodmaxvotes{$prod}) {
        PutHeader("Don't overstuff!", "Illegal vote");
        print "You may only use $::prodmaxvotes{$prod} votes for bugs in the\n";
        print "<tt>$prod</tt> product, but you are using $prodcount{$prod}.\n";
        print "Please click <b>Back</b> and try again.<hr>\n";
        navigation_header();
        exit();
    }
}

my %affected;
SendSQL("lock tables bugs write, votes write");
SendSQL("select bug_id from votes where who = $who");
while (MoreSQLData()) {
    my $id = FetchOneColumn();
    $affected{$id} = 1;
}
SendSQL("delete from votes where who = $who");
foreach my $id (@buglist) {
    if ($::FORM{$id} > 0) {
        SendSQL("insert into votes (who, bug_id, count) values ($who, $id, $::FORM{$id})");
    }
    $affected{$id} = 1;
}
foreach my $id (keys %affected) {
    SendSQL("select sum(count) from votes where bug_id = $id");
    my $v = FetchOneColumn();
    SendSQL("update bugs set votes = $v where bug_id = $id");
}
SendSQL("unlock tables");



PutHeader("Voting tabulated", "Voting tabulated", $::COOKIE{'Bugzilla_login'});
print "Your votes have been recorded.\n";
print qq{<p><a href="showvotes.cgi?user=$who">Review your votes</a><hr>\n};
navigation_header();
exit();
    

