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

use strict;

use lib qw(.);

require "CGI.pl";

use RelationSet;

# Shut up misguided -w warnings about "used only once". "use vars" just
# doesn't work for me.
sub sillyness {
    my $zz;
    $zz = $::defaultqueryname;
}

# Use global template variables.
use vars qw($template $vars $userid);

# The default email flags leave all email on.
my $defaultflagstring = "ExcludeSelf~on~";

my @roles = ("Owner", "Reporter", "QAcontact", "CClist", "Voter");
my @reasons = ("Removeme", "Comments", "Attachments", "Status", "Resolved", 
               "Keywords", "CC", "Other", "Unconfirmed");

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

    if(Param('allowemailchange')) {
        SendSQL("SELECT tokentype, issuedate + INTERVAL 3 DAY, eventdata 
                    FROM tokens
                    WHERE userid = $userid
                    AND tokentype LIKE 'email%' 
                    ORDER BY tokentype ASC LIMIT 1");
        if(MoreSQLData()) {
            my ($tokentype, $change_date, $eventdata) = &::FetchSQLData();
            $vars->{'login_change_date'} = $change_date;

            if($tokentype eq 'emailnew') {
                my ($oldemail,$newemail) = split(/:/,$eventdata);
                $vars->{'new_login_name'} = $newemail;
            }
        }
    }
}

sub SaveAccount {
    my $pwd1 = $::FORM{'new_password1'};
    my $pwd2 = $::FORM{'new_password2'};

    if ($::FORM{'Bugzilla_password'} ne "" || 
        $pwd1 ne "" || $pwd2 ne "") 
    {
        my $old = SqlQuote($::FORM{'Bugzilla_password'});
        SendSQL("SELECT cryptpassword FROM profiles WHERE userid = $userid");
        my $oldcryptedpwd = FetchOneColumn();
        $oldcryptedpwd || ThrowCodeError("unable_to_retrieve_password");

        if (crypt($::FORM{'Bugzilla_password'}, $oldcryptedpwd) ne 
                  $oldcryptedpwd) 
        {
            ThrowUserError("old_password_incorrect");
        }

        if ($pwd1 ne "" || $pwd2 ne "")
        {
            ($pwd1 eq $pwd2) || ThrowUserError("passwords_dont_match");
            $::FORM{'new_password1'} || ThrowUserError("new_password_missing");
            ValidatePassword($pwd1);
        
            my $cryptedpassword = SqlQuote(Crypt($pwd1));
            SendSQL("UPDATE profiles 
                     SET    cryptpassword = $cryptedpassword 
                     WHERE  userid = $userid");
            # Invalidate all logins except for the current one
            InvalidateLogins($userid, $::COOKIE{"Bugzilla_logincookie"});
        }
    }

    if(Param("allowemailchange") && $::FORM{'new_login_name'}) {
        my $old_login_name = $::FORM{'Bugzilla_login'};
        my $new_login_name = trim($::FORM{'new_login_name'});

        if($old_login_name ne $new_login_name) {
            $::FORM{'Bugzilla_password'} 
              || ThrowCodeError("old_password_required");

            use Token;
            # Block multiple email changes for the same user.
            if (Token::HasEmailChangeToken($userid)) {
                ThrowUserError("email_change_in_progress");
            }

            # Before changing an email address, confirm one does not exist.
            CheckEmailSyntax($new_login_name);
            trick_taint($new_login_name);
            ValidateNewUser($new_login_name)
              || ThrowUserError("account_exists", {email => $new_login_name});

            Token::IssueEmailChangeToken($userid,$old_login_name,
                                                 $new_login_name);

            $vars->{'email_changes_saved'} = 1;
        }
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

    # Determine the value of the "excludeself" global email preference.
    # Note that the value of "excludeself" is assumed to be off if the
    # preference does not exist in the user's list, unlike other 
    # preferences whose value is assumed to be on if they do not exist.
    if (exists($emailflags{'ExcludeSelf'}) 
        && $emailflags{'ExcludeSelf'} eq 'on')
    {
        $vars->{'excludeself'} = 1;
    }
    else {
        $vars->{'excludeself'} = 0;
    }
    
    foreach my $flag (qw(FlagRequestee FlagRequester)) {
        $vars->{$flag} = 
          !exists($emailflags{$flag}) || $emailflags{$flag} eq 'on';
    }
    
    # Parse the info into a hash of hashes; the first hash keyed by role,
    # the second by reason, and the value being 1 or 0 for (on or off).
    # Preferences not existing in the user's list are assumed to be on.
    foreach my $role (@roles) {
        $vars->{$role} = {};
        foreach my $reason (@reasons) {
            my $key = "email$role$reason";
            if (!exists($emailflags{$key}) || $emailflags{$key} eq 'on') {
                $vars->{$role}{$reason} = 1;
            }
            else {
                $vars->{$role}{$reason} = 0;
            }
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
    
    foreach my $flag (qw(FlagRequestee FlagRequester)) {
        $updateString .= "~$flag~" . (defined($::FORM{$flag}) ? "on" : "");
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
            ThrowUserError("missing_query", {queryname => $name});
        }
    }
    SendSQL("UPDATE profiles SET mybugslink = " . 
            SqlQuote($::FORM{'mybugslink'}) . " WHERE userid = $userid");

    # Regenerate cached info about queries in footer.            
    $vars->{'user'} = GetUserInfo($::userid);
}
    
    
sub DoPermissions {
    my (@has_bits, @set_bits);
    
    SendSQL("SELECT DISTINCT name, description FROM groups, user_group_map " .
            "WHERE user_group_map.group_id = groups.id " .
            "AND user_id = $::userid " .
            "AND isbless = 0 " .
            "ORDER BY name");
    while (MoreSQLData()) {
        my ($nam, $desc) = FetchSQLData();
        push(@has_bits, {"desc" => $desc, "name" => $nam});
    }
    my @set_ids = ();
    SendSQL("SELECT DISTINCT name, description FROM groups " .
            "ORDER BY name");
    while (MoreSQLData()) {
        my ($nam, $desc) = FetchSQLData();
        if (UserCanBlessGroup($nam)) {
            push(@set_bits, {"desc" => $desc, "name" => $nam});
        }
    }
    
    $vars->{'has_bits'} = \@has_bits;
    $vars->{'set_bits'} = \@set_bits;    
}

# No SavePermissions() because this panel has no changeable fields.

###############################################################################
# Live code (not subroutine definitions) starts here
###############################################################################

ConnectToDatabase();
confirm_login();

GetVersionTable();

$vars->{'login'} = $::COOKIE{'Bugzilla_login'};
$vars->{'changes_saved'} = $::FORM{'dosave'};

my $current_tab_name = $::FORM{'tab'} || "account";

# The SWITCH below makes sure that this is valid
trick_taint($current_tab_name);

$vars->{'current_tab_name'} = $current_tab_name;

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
    ThrowUserError("current_tab_name");
}

# Generate and return the UI (HTML page) from the appropriate template.
print "Content-type: text/html\n\n";
$template->process("account/prefs/prefs.html.tmpl", $vars)
  || ThrowTemplateError($template->error());

