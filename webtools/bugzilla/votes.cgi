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
#                 Stephan Niemz  <st.n@gmx.net>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>

use strict;
use lib ".";

require "CGI.pl";


# Use global template variables
use vars qw($template $vars);

ConnectToDatabase();

# If the action is show_bug, you need a bug_id.
# If the action is show_user, you can supply a userid to show the votes for
# another user, otherwise you see your own.
# If the action is vote, your votes are set to those encoded in the URL as 
# <bug_id>=<votes>.
#
# If no action is defined, we default to show_bug if a bug_id is given,
# otherwise to show_user.
my $action = $::FORM{'action'} || 
                                 ($::FORM{'bug_id'} ? "show_bug" : "show_user");

if ($action eq "show_bug" ||
    ($action eq "show_user" && defined($::FORM{'user'}))) 
{
    quietly_check_login();
}
else {
    confirm_login();
}

################################################################################
# Begin Data/Security Validation
################################################################################

# Make sure the bug ID is a positive integer representing an existing
# bug that the user is authorized to access.
if (defined $::FORM{'bug_id'}) {
  ValidateBugID($::FORM{'bug_id'});
}

################################################################################
# End Data/Security Validation
################################################################################

if ($action eq "show_bug") {
    show_bug();
} 
elsif ($action eq "show_user") {
    show_user();
}
elsif ($action eq "vote") {
    record_votes();
    show_user();
}
else {
    ThrowCodeError("unknown_action", {action => $action});
}

exit;

# Display the names of all the people voting for this one bug.
sub show_bug {
    my $bug_id = $::FORM{'bug_id'} 
      || ThrowCodeError("missing_bug_id");
      
    my $total = 0;
    my @users;
    
    SendSQL("SELECT profiles.login_name, votes.who, votes.count 
             FROM votes, profiles 
             WHERE votes.bug_id = $bug_id 
               AND profiles.userid = votes.who");
                   
    while (MoreSQLData()) {
        my ($name, $userid, $count) = (FetchSQLData());
        push (@users, { name => $name, id => $userid, count => $count });
        $total += $count;
    }
    
    $vars->{'bug_id'} = $bug_id;
    $vars->{'users'} = \@users;
    $vars->{'total'} = $total;
    
    print "Content-type: text/html\n\n";
    $template->process("bug/votes/list-for-bug.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# Display all the votes for a particular user. If it's the user
# doing the viewing, give them the option to edit them too.
sub show_user {
    GetVersionTable();
    
    # If a bug_id is given, and we're editing, we'll add it to the votes list.
    my $bug_id = $::FORM{'bug_id'} || "";
        
    my $name = $::FORM{'user'} || $::COOKIE{'Bugzilla_login'};
    my $who = DBname_to_id($name);
    
    # After DBNameToIdAndCheck is templatised and prints a Content-Type, 
    # the above should revert to a call to that function, and this 
    # special error handling should go away.
    $who || ThrowUserError("invalid_username", {name => $name});
    
    my $canedit = 1 if ($name eq $::COOKIE{'Bugzilla_login'});
    
    SendSQL("LOCK TABLES bugs READ, products READ, votes WRITE,
             cc READ, bug_group_map READ, user_group_map READ,
             cc AS selectVisible_cc READ");
    
    if ($canedit && $bug_id) {
        # Make sure there is an entry for this bug
        # in the vote table, just so that things display right.
        SendSQL("SELECT votes.count FROM votes 
                 WHERE votes.bug_id = $bug_id AND votes.who = $who");
        if (!FetchOneColumn()) {
            SendSQL("INSERT INTO votes (who, bug_id, count) 
                     VALUES ($who, $bug_id, 0)");
        }
    }
    
    # Calculate the max votes per bug for each product; doing it here means
    # we can do it all in one query.
    my %maxvotesperbug;
    if($canedit) {
        SendSQL("SELECT products.name, products.maxvotesperbug 
                 FROM products");
        while (MoreSQLData()) {
            my ($prod, $max) = FetchSQLData();
            $maxvotesperbug{$prod} = $max;
        }
    }
    
    my @products;
    
    # Read the votes data for this user for each product
    foreach my $product (sort(keys(%::prodmaxvotes))) {
        next if $::prodmaxvotes{$product} <= 0;
        
        my @bugs;
        my $total = 0;
        my $onevoteonly = 0;
        
        SendSQL("SELECT votes.bug_id, votes.count, bugs.short_desc,
                        bugs.bug_status 
                  FROM  votes, bugs, products
                  WHERE votes.who = $who 
                    AND votes.bug_id = bugs.bug_id 
                    AND bugs.product_id = products.id 
                    AND products.name = " . SqlQuote($product) . 
                 "ORDER BY votes.bug_id");        
        
        while (MoreSQLData()) {
            my ($id, $count, $summary, $status) = FetchSQLData();
            next if !defined($status);
            $total += $count;
             
            # Next if user can't see this bug. So, the totals will be correct
            # and they can see there are votes 'missing', but not on what bug
            # they are. This seems a reasonable compromise; the alternative is
            # to lie in the totals.
            next if !CanSeeBug($id, $who);            
            
            push (@bugs, { id => $id, 
                           summary => $summary,
                           count => $count,
                           opened => IsOpenedState($status) });
        }
        
        $onevoteonly = 1 if (min($::prodmaxvotes{$product},
                                 $maxvotesperbug{$product}) == 1);
        
        # Only add the product for display if there are any bugs in it.
        if ($#bugs > -1) {                         
            push (@products, { name => $product,
                               bugs => \@bugs,
                               onevoteonly => $onevoteonly,
                               total => $total,
                               maxvotes => $::prodmaxvotes{$product},
                               maxperbug => $maxvotesperbug{$product} });
        }
    }

    SendSQL("DELETE FROM votes WHERE count <= 0");
    SendSQL("UNLOCK TABLES");
    
    $vars->{'voting_user'} = { "login" => $name };
    $vars->{'products'} = \@products;

    print "Content-type: text/html\n\n";
    $template->process("bug/votes/list-for-user.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# Update the user's votes in the database.
sub record_votes {
    ############################################################################
    # Begin Data/Security Validation
    ############################################################################

    # Build a list of bug IDs for which votes have been submitted.  Votes
    # are submitted in form fields in which the field names are the bug 
    # IDs and the field values are the number of votes.
    my @buglist = grep {/^[1-9][0-9]*$/} keys(%::FORM);

    # If no bugs are in the buglist, let's make sure the user gets notified
    # that their votes will get nuked if they continue.
    if (scalar(@buglist) == 0) {
        if (!defined($::FORM{'delete_all_votes'})) {
            print "Content-type: text/html\n\n";
            $template->process("bug/votes/delete-all.html.tmpl", $vars)
              || ThrowTemplateError($template->error());
            exit();
        }
        elsif ($::FORM{'delete_all_votes'} == 0) {
            print "Location: votes.cgi\n\n";
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
      detaint_natural($::FORM{$id})
        || ThrowUserError("votes_must_be_nonnegative");
    }

    ############################################################################
    # End Data/Security Validation
    ############################################################################

    GetVersionTable();

    my $who = DBNameToIdAndCheck($::COOKIE{'Bugzilla_login'});

    # If the user is voting for bugs, make sure they aren't overstuffing
    # the ballot box.
    if (scalar(@buglist)) {
        SendSQL("SELECT bugs.bug_id, products.name, products.maxvotesperbug
                 FROM bugs, products
                 WHERE products.id = bugs.product_id
                   AND bugs.bug_id IN (" . join(", ", @buglist) . ")");

        my %prodcount;

        while (MoreSQLData()) {
            my ($id, $prod, $max) = FetchSQLData();
            $prodcount{$prod} ||= 0;
            $prodcount{$prod} += $::FORM{$id};
            
            # Make sure we haven't broken the votes-per-bug limit
            ($::FORM{$id} <= $max)               
              || ThrowUserError("too_many_votes_for_bug",
                                {max => $max, 
                                 product => $prod, 
                                 votes => $::FORM{$id}});
        }

        # Make sure we haven't broken the votes-per-product limit
        foreach my $prod (keys(%prodcount)) {
            ($prodcount{$prod} <= $::prodmaxvotes{$prod})
              || ThrowUserError("too_many_votes_for_product",
                                {max => $::prodmaxvotes{$prod}, 
                                 product => $prod, 
                                 votes => $prodcount{$prod}});
        }
    }

    # Update the user's votes in the database.  If the user did not submit 
    # any votes, they may be using a form with checkboxes to remove all their
    # votes (checkboxes are not submitted along with other form data when
    # they are not checked, and Bugzilla uses them to represent single votes
    # for products that only allow one vote per bug).  In that case, we still
    # need to clear the user's votes from the database.
    my %affected;
    SendSQL("LOCK TABLES bugs write, votes write, products read, cc read,
             user_group_map read, bug_group_map read");
    
    # Take note of, and delete the user's old votes from the database.
    SendSQL("SELECT bug_id FROM votes WHERE who = $who");
    while (MoreSQLData()) {
        my $id = FetchOneColumn();
        $affected{$id} = 1;
    }
    
    SendSQL("DELETE FROM votes WHERE who = $who");
    
    # Insert the new values in their place
    foreach my $id (@buglist) {
        if ($::FORM{$id} > 0) {
            SendSQL("INSERT INTO votes (who, bug_id, count) 
                     VALUES ($who, $id, $::FORM{$id})");
        }
        
        $affected{$id} = 1;
    }
    
    # Update the cached values in the bugs table
    foreach my $id (keys %affected) {
        SendSQL("SELECT sum(count) FROM votes WHERE bug_id = $id");
        my $v = FetchOneColumn();
        $v ||= 0;
        SendSQL("UPDATE bugs SET votes = $v, delta_ts=delta_ts 
                 WHERE bug_id = $id");
        CheckIfVotedConfirmed($id, $who);
    }

    SendSQL("UNLOCK TABLES");

    $vars->{'votes_recorded'} = 1;
}
