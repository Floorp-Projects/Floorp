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
#                 Christopher Aillon <christopher@aillon.com>

use diagnostics;
use strict;

require "CGI.pl";

ConnectToDatabase();

confirm_login();

######################################################################
# Begin Data/Security Validation
######################################################################

# Build a list of bug IDs for which votes have been submitted.  Votes
# are submitted in form fields in which the field names are the bug 
# IDs and the field values are the number of votes.
my @buglist = grep {/^[1-9][0-9]*$/} keys(%::FORM);

# If no bugs are in the buglist, let's make sure the user gets notified
# that their votes will get nuked if they continue.
if (0 == @buglist) {
    if (! defined $::FORM{'delete_all_votes'}) {
        print "Content-type: text/html\n\n";
        PutHeader("Remove your votes?");
        print "<p>You are about to remove all of your bug votes. Are you sure you wish to remove your vote from every bug you've voted on?</p>";
        print qq{<form action="doeditvotes.cgi" method="post">\n};
        print qq{<p><input type="radio" name="delete_all_votes" value="1"> Yes</p>\n};
        print qq{<p><input type="radio" name="delete_all_votes" value="0" checked="checked"> No</p>\n};
        print qq{<p><a href="showvotes.cgi">Review your votes</a></p>\n};
        print qq{<p><input type="submit" value="Submit"></p></form>\n};
        PutFooter();
        exit();
    }
    elsif ($::FORM{'delete_all_votes'} == 0) {
        print "Location: showvotes.cgi\n\n";
        exit();
    }
}

# Call ValidateBugID on each bug ID to make sure it is a positive
# integer representing an existing bug that the user is authorized 
# to access, and make sure the number of votes submitted is also
# a non-negative integer (a series of digits not preceded by a
# minus sign).
foreach my $id (@buglist) {
  ValidateBugID($id);
  ($::FORM{$id} =~ /^\d+$/)
    || DisplayError("Only use non-negative numbers for your bug votes.")
    && exit;
}

######################################################################
# End Data/Security Validation
######################################################################

print "Content-type: text/html\n\n";

GetVersionTable();

my $who = DBNameToIdAndCheck($::COOKIE{'Bugzilla_login'});

if ( (! defined $who) || (!$who) ) {
    PutHeader("Bad login.");
    print qq|
    The login info got confused. Please <a href="query.cgi?GoAheadAndLogIn=1">log 
    in</a> (again) and try again.\n|;
    PutFooter();
    exit();
}

# If the user is voting for bugs, make sure they aren't overstuffing
# the ballot box.
if (scalar(@buglist)) {
    SendSQL("SELECT bugs.bug_id, bugs.product, products.maxvotesperbug " .
            "FROM bugs, products " .
            "WHERE products.product = bugs.product ".
            " AND bugs.bug_id IN (" . join(", ", @buglist) . ")");

    my %prodcount;

    while (MoreSQLData()) {
        my ($id, $prod, $max) = (FetchSQLData());
        if (!defined $prodcount{$prod}) {
            $prodcount{$prod} = 0;
        }
        $prodcount{$prod} += $::FORM{$id};
        if ($::FORM{$id} > $max) {
            PutHeader("Don't overstuff!", "Illegal vote");
            print "You may only use at most $max votes for a single bug in the\n";
            print "<tt>$prod</tt> product, but you are trying to use $::FORM{$id}.\n";
            print "<P>Please click <b>Back</b> and try again.<hr>\n";
            PutFooter();
            exit();
        }
    }

    foreach my $prod (keys(%prodcount)) {
        if ($prodcount{$prod} > $::prodmaxvotes{$prod}) {
            PutHeader("Don't overstuff!", "Illegal vote");
            print "You may only use $::prodmaxvotes{$prod} votes for bugs in the\n";
            print "<tt>$prod</tt> product, but you are trying to use $prodcount{$prod}.\n";
            print "<P>Please click <b>Back</b> and try again.<hr>\n";
            PutFooter();
            exit();
        }
    }
}

# Update the user's votes in the database.  If the user did not submit 
# any votes, they may be using a form with checkboxes to remove all their
# votes (checkboxes are not submitted along with other form data when
# they are not checked, and Bugzilla uses them to represent single votes
# for products that only allow one vote per bug).  In that case, we still
# need to clear the user's votes from the database.
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
    $v ||= 0;
    SendSQL("update bugs set votes = $v, delta_ts=delta_ts where bug_id = $id");
}
SendSQL("unlock tables");


PutHeader("Voting tabulated", "Voting tabulated", $::COOKIE{'Bugzilla_login'});
print "Your votes have been recorded.\n";
print qq{<p><a href="showvotes.cgi?user=$who">Review your votes</a><hr>\n};
foreach my $id (keys %affected) {
    CheckIfVotedConfirmed($id, $who);
}
PutFooter();
exit();
    

