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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Alan Raetz <al_raetz@yahoo.com>
#                 David Miller <justdave@syndicomm.com>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>

use diagnostics;
use strict;

use lib qw(.);

require "CGI.pl";

use RelationSet;

# Shut up misguided -w warnings about "used only once". "use vars" just
# doesn't work for me.
sub sillyness {
    my $zz;
    $zz = $::defaultqueryname;
    $zz = $::usergroupset;
}

# Use global template variables.
use vars qw($template $vars);

my $userid;

# The default email flags leave all email on.
my $defaultflagstring = "ExcludeSelf~on~";

my @roles = ("Owner", "Reporter", "QAcontact", "CClist", "Voter");
my @reasons = ("Removeme", "Comments", "Attachments", "Status", "Resolved", 
               "Keywords", "CC", "Other");

foreach my $role (@roles) {
    foreach my $reason (@reasons) {
        $defaultflagstring .= "email$role$reason~on~";
    }
}

# Remove final "~".
chop $defaultflagstring;

###############################################################################
# Each panel has two functions - panel Foo has a DoFoo, to get the data 
# necessary for displaying the panel, and a SaveFoo, to save the panel's 
# contents from the form data (if appropriate.) 
# SaveFoo may be called before DoFoo.    
###############################################################################
sub DoAccount {
    SendSQL("SELECT realname FROM profiles WHERE userid = $userid");
    $vars->{'realname'} = FetchSQLData();
}

sub SaveAccount {
    if ($::FORM{'Bugzilla_password'} ne "" || 
        $::FORM{'new_password1'} ne "" || 
        $::FORM{'new_password2'} ne "") 
    {
        my $old = SqlQuote($::FORM{'Bugzilla_password'});
        my $pwd1 = SqlQuote($::FORM{'new_password1'});
        my $pwd2 = SqlQuote($::FORM{'new_password2'});
        SendSQL("SELECT cryptpassword FROM profiles WHERE userid = $userid");
        my $oldcryptedpwd = FetchOneColumn();
        if (!$oldcryptedpwd) {
            DisplayError("I was unable to retrieve your old password from the database.");
            exit;
        }
        if (crypt($::FORM{'Bugzilla_password'}, $oldcryptedpwd) ne 
                  $oldcryptedpwd) 
        {
            DisplayError("You did not enter your old password correctly.");
            exit;
        }
        if ($pwd1 ne $pwd2) {
            DisplayError("The two passwords you entered did not match.");
            exit;
        }
        if ($::FORM{'new_password1'} eq '') {
            DisplayError("You must enter a new password.");
            exit;
        }
        my $passworderror = ValidatePassword($::FORM{'new_password1'});
        (DisplayError($passworderror) && exit) if $passworderror;
        
        my $cryptedpassword = SqlQuote(Crypt($::FORM{'new_password1'}));
        SendSQL("UPDATE profiles 
                 SET    cryptpassword = $cryptedpassword 
                 WHERE  userid = $userid");
        # Invalidate all logins except for the current one
        InvalidateLogins($userid, $::COOKIE{"Bugzilla_logincookie"});
    }

    SendSQL("UPDATE profiles SET " .
            "realname = " . SqlQuote(trim($::FORM{'realname'})) .
            " WHERE userid = $userid");
}


sub DoEmail {
    if (Param("supportwatchers")) {
        my $watcheduserSet = new RelationSet;
        $watcheduserSet->mergeFromDB("SELECT watched FROM watch WHERE" .
                                    " watcher=$userid");
        $vars->{'watchedusers'} = $watcheduserSet->toString();
    }

    SendSQL("SELECT emailflags FROM profiles WHERE userid = $userid");

    my ($flagstring) = FetchSQLData();

    # If the emailflags haven't been set before, that means that this user 
    # hasn't been to the email pane of userprefs.cgi since the change to 
    # use emailflags. Create a default flagset for them, based on
    # static defaults. 
    if (!$flagstring) {
        $flagstring = $defaultflagstring;
        SendSQL("UPDATE profiles SET emailflags = " .
                SqlQuote($flagstring) . " WHERE userid = $userid");
    }

    # The 255 param is here, because without a third param, split will
    # trim any trailing null fields, which causes Perl to eject lots of
    # warnings. Any suitably large number would do.
    my %emailflags = split(/~/, $flagstring, 255);

    $vars->{'excludeself'} = 0;

    # Parse the info into a hash of hashes; the first hash keyed by role,
    # the second by reason, and the value being 1 or 0 (undef).
    foreach my $key (keys %emailflags) {
        # ExcludeSelf is special.
        if ($key eq 'ExcludeSelf' && $emailflags{$key} eq 'on') {
            $vars->{'excludeself'} = 1;
            next;
        }

        # All other keys match this regexp.
        $key =~ /email([A-Z]+[a-z]+)([A-Z]+[a-z]*)/;
        
        # Create a new hash if we don't have one...
        if (!defined($vars->{$1})) {
            $vars->{$1} = {};
        }
        
        if ($emailflags{$key} eq "on") {
            $vars->{$1}{$2} = 1;
        }            
    }
}

# Note: we no longer store "off" values in the database.
sub SaveEmail {
    my $updateString = "";
    
    if (defined $::FORM{'ExcludeSelf'}) {
        $updateString .= 'ExcludeSelf~on';
    } else {
        $updateString .= 'ExcludeSelf~';
    }
    
    foreach my $role (@roles) {
        foreach my $reason (@reasons) {
            # Add this preference to the list without giving it a value,
            # which is the equivalent of setting the value to "off."
            $updateString .= "~email$role$reason~";
            
            # If the form field for this preference is defined, then we
            # know the checkbox was checked, so set the value to "on".
            $updateString .= "on" if defined $::FORM{"email$role$reason"};
        }
    }
            
    SendSQL("UPDATE profiles SET emailflags = " . SqlQuote($updateString) . 
            " WHERE userid = $userid");

    if (Param("supportwatchers") && exists $::FORM{'watchedusers'}) {
        # Just in case.  Note that this much locking is actually overkill:
        # we don't really care if anyone reads the watch table.  So 
        # some small amount of contention could be gotten rid of by
        # using user-defined locks rather than table locking.
        SendSQL("LOCK TABLES watch WRITE, profiles READ");

        # what the db looks like now
        my $origWatchedUsers = new RelationSet;
        $origWatchedUsers->mergeFromDB("SELECT watched FROM watch WHERE" .
                                       " watcher=$userid");

        # Update the database to look like the form
        my $newWatchedUsers = new RelationSet($::FORM{'watchedusers'});
        my @CCDELTAS = $origWatchedUsers->generateSqlDeltas(
                                                         $newWatchedUsers, 
                                                         "watch", 
                                                         "watcher", 
                                                         $userid,
                                                         "watched");
        ($CCDELTAS[0] eq "") || SendSQL($CCDELTAS[0]);
        ($CCDELTAS[1] eq "") || SendSQL($CCDELTAS[1]);

        SendSQL("UNLOCK TABLES");       
    }
}


sub DoFooter {
    SendSQL("SELECT mybugslink FROM profiles " .
            "WHERE userid = $userid");
    $vars->{'mybugslink'} = FetchSQLData();
    
    SendSQL("SELECT name, linkinfooter FROM namedqueries " .
            "WHERE userid = $userid");
    
    my @queries;        
    while (MoreSQLData()) {
        my ($name, $footer) = (FetchSQLData());
        next if ($name eq $::defaultqueryname);
        
        push (@queries, { name => $name, footer => $footer });        
    }
    
    $vars->{'queries'} = \@queries;
}
              
sub SaveFooter {
    my %old;
    SendSQL("SELECT name, linkinfooter FROM namedqueries " .
            "WHERE userid = $userid");
    while (MoreSQLData()) {
        my ($name, $footer) = (FetchSQLData());
        $old{$name} = $footer;
    }
    
    for (my $c = 0; $c < $::FORM{'numqueries'}; $c++) {
        my $name = $::FORM{"name-$c"};
        if (exists $old{$name}) {
            my $new = $::FORM{"query-$c"};
            if ($new ne $old{$name}) {
                detaint_natural($new);
                SendSQL("UPDATE namedqueries SET linkinfooter = $new " .
                        "WHERE userid = $userid " .
                        "AND name = " . SqlQuote($name));
            }
        } else {
            DisplayError("Hmm, the $name query seems to have gone away.");
        }
    }
    SendSQL("UPDATE profiles SET mybugslink = " . 
            SqlQuote($::FORM{'mybugslink'}) . " WHERE userid = $userid");
}
    
    
sub DoPermissions {
    my (@has_bits, @set_bits);
    
    SendSQL("SELECT description FROM groups " .
            "WHERE bit & $::usergroupset != 0 " .
            "ORDER BY bit");
    while (MoreSQLData()) {
        push(@has_bits, FetchSQLData());
    }
    
    SendSQL("SELECT blessgroupset FROM profiles WHERE userid = $userid");
    my $blessgroupset = FetchOneColumn();
    if ($blessgroupset) {
        SendSQL("SELECT description FROM groups " .
                "WHERE bit & $blessgroupset != 0 " .
                "ORDER BY bit");
        while (MoreSQLData()) {
            push(@set_bits, FetchSQLData());
        }
    }
    
    $vars->{'has_bits'} = \@has_bits;
    $vars->{'set_bits'} = \@set_bits;    
}

# No SavePermissions() because this panel has no changeable fields.

###############################################################################
# Live code (not subroutine definitions) starts here
###############################################################################
confirm_login();

GetVersionTable();

$userid = DBNameToIdAndCheck($::COOKIE{'Bugzilla_login'});

$vars->{'login'} = $::COOKIE{'Bugzilla_login'};
$vars->{'changes_saved'} = $::FORM{'dosave'};

my $current_tab_name = $::FORM{'tab'} || "account";

my @tabs = ( { name => "account", description => "Account settings", 
               saveable => "1" },
             { name => "email", description => "Email settings", 
               saveable => "1" },
             { name => "footer", description => "Page footer", 
               saveable => "1" },
             { name => "permissions", description => "Permissions", 
               saveable => "0" } );

# Work out the current tab
foreach my $tab (@tabs) {
    if ($tab->{'name'} eq $current_tab_name) {
        $vars->{'current_tab'} = $tab;
        last;
    }
}

$vars->{'tabs'} = \@tabs;

# Do any saving, and then display the current tab.
SWITCH: for ($current_tab_name) {
    /^account$/ && do {
        SaveAccount() if $::FORM{'dosave'};
        DoAccount();
        last SWITCH;
    };
    /^email$/ && do {
        SaveEmail() if $::FORM{'dosave'};
        DoEmail();
        last SWITCH;
    };
    /^footer$/ && do {
        SaveFooter() if $::FORM{'dosave'};
        DoFooter();
        last SWITCH;
    };
    /^permissions$/ && do {
        DoPermissions();
        last SWITCH;
    };
}

# Generate and return the UI (HTML page) from the appropriate template.
print "Content-type: text/html\n\n";
$template->process("prefs/userprefs.tmpl", $vars)
  || DisplayError("Template process failed: " . $template->error())
  && exit;

