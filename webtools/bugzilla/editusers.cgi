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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Holger
# Schurig. Portions created by Holger Schurig are
# Copyright (C) 1999 Holger Schurig. All
# Rights Reserved.
#
# Contributor(s): Holger Schurig <holgerschurig@nikocity.de>
#                 Dave Miller <justdave@syndicomm.com>
#                 Joe Robins <jmrobins@tgix.com>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joel Peshkin <bugreport@peshkin.net>
#                 Erik Stambaugh <erik@dasbistro.com>
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>

use strict;
use lib ".";

require "CGI.pl";
require "globals.pl";

use Bugzilla;
use Bugzilla::User;
use Bugzilla::Constants;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::userid;
}

my $editall;



# TestUser:  just returns if the specified user does exists
# CheckUser: same check, optionally  emit an error text

sub TestUser ($)
{
    my $user = shift;

    # does the product exist?
    SendSQL("SELECT login_name
             FROM profiles
             WHERE login_name=" . SqlQuote($user));
    return FetchOneColumn();
}

sub CheckUser ($)
{
    my $user = shift;

    # do we have a user?
    unless ($user) {
        print "Sorry, you haven't specified a user.";
        PutTrailer();
        exit;
    }

    unless (TestUser $user) {
        print "Sorry, user '$user' does not exist.";
        PutTrailer();
        exit;
    }
}



sub EmitElement ($$)
{
    my ($name, $value) = (@_);
    $value = value_quote($value);
    if ($editall) {
        print qq{<TD><INPUT SIZE=64 MAXLENGTH=255 NAME="$name" VALUE="$value"></TD>\n};
    } else {
        print qq{<TD>$value<INPUT TYPE=HIDDEN  NAME="$name" VALUE="$value"></TD>\n};
    }
}


#
# Displays the form to edit a user parameters
#

sub EmitFormElements ($$$$)
{
    my ($user_id, $user, $realname, $disabledtext) = @_;

    print "  <TH ALIGN=\"right\">Login name:</TH>\n";
    EmitElement("user", $user);

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Real name:</TH>\n";
    EmitElement("realname", $realname);

    if ($editall) {
        print "</TR><TR>\n";
        print "  <TH ALIGN=\"right\">Password:</TH>\n";
          print qq|
            <TD><INPUT TYPE="PASSWORD" SIZE="16" MAXLENGTH="16" NAME="password" VALUE=""><br>
                (enter new password to change)
            </TD>
          |;
        print "</TR><TR>\n";

        print "  <TH ALIGN=\"right\">Disable text:</TH>\n";
        print "  <TD ROWSPAN=2><TEXTAREA NAME=\"disabledtext\" ROWS=10 COLS=60>" .
            value_quote($disabledtext) . "</TEXTAREA>\n";
        print "  </TD>\n";
        print "</TR><TR>\n";
        print "  <TD VALIGN=\"top\">If non-empty, then the account will\n";
        print "be disabled, and this text should explain why.</TD>\n";
    }
        
    
    if($user ne "") {
        print "</TR><TR><TH VALIGN=TOP ALIGN=RIGHT>Group Access:</TH><TD><TABLE><TR>";
        SendSQL("SELECT groups.id, groups.name, groups.description, " .
                "MAX(IF(grant_type = " . GRANT_DIRECT . ", 1, 0))," .
                "MAX(IF(grant_type = " . GRANT_DERIVED . ", 1, 0))," .
                "MAX(IF(grant_type = " . GRANT_REGEXP . ", 1, 0))" .
                "FROM groups " .
                "LEFT JOIN user_group_map " .
                "ON user_group_map.group_id = groups.id " .
                "AND isbless = 0 " .
                "AND user_id = $user_id " .
                "GROUP BY groups.name ");
        if (MoreSQLData()) {
            if ($editall) {
                print "<TD COLSPAN=3 ALIGN=LEFT><B>Can turn this bit on for other users</B></TD>\n";
                print "</TR><TR>\n<TD ALIGN=CENTER><B>|</B></TD>\n";
            }
            print "<TD COLSPAN=2 ALIGN=LEFT><B>User is a member of these groups</B></TD>\n";
            while (MoreSQLData()) {
                my ($groupid, $name, $description, $checked, $isderived, $isregexp) = FetchSQLData();
                next unless ($editall || UserCanBlessGroup($name));
                PushGlobalSQLState();
                SendSQL("SELECT user_id " .
                        "FROM user_group_map " .
                        "WHERE isbless = 1 " .
                        "AND user_id = $user_id " .
                        "AND group_id = $groupid");
                my ($blchecked) = FetchSQLData() ? 1 : 0;
                SendSQL("SELECT grantor_id FROM user_group_map, 
                    group_group_map 
                    WHERE $groupid = grantor_id 
                    AND user_group_map.user_id = $user_id
                    AND user_group_map.isbless = 0
                    AND group_group_map.isbless = 1
                    AND user_group_map.group_id = member_id");
                my $derivedbless = FetchOneColumn();
                PopGlobalSQLState();
                print "</TR><TR";
                print ' bgcolor=#cccccc' if ($isderived || $isregexp);
                print ">\n";
                print "<INPUT TYPE=HIDDEN NAME=\"oldgroup_$groupid\" VALUE=\"$checked\">\n";
                print "<INPUT TYPE=HIDDEN NAME=\"oldbless_$groupid\" VALUE=\"$blchecked\">\n";
                if ($editall) {
                    $blchecked = ($blchecked) ? "CHECKED" : "";
                    print "<TD ALIGN=CENTER>";
                    print "[" if $derivedbless;
                    print "<INPUT TYPE=CHECKBOX NAME=\"bless_$groupid\" $blchecked VALUE=\"$groupid\">";
                    print "]" if $derivedbless;
                    print "</TD>\n";
                }
                $checked = ($checked) ? "CHECKED" : "";
                print "<TD ALIGN=CENTER>";
                print '[' if ($isderived);
                print '*' if ($isregexp);
                print "<INPUT TYPE=CHECKBOX NAME=\"group_$groupid\" $checked VALUE=\"$groupid\">";
                print ']' if ($isderived);
                print '*' if ($isregexp);
                print "</TD><TD><B>";
                print ucfirst($name) . "</B>: $description</TD>\n";
            }
        }
        print "</TR></TABLE></TD>\n";
    }
}



#
# Displays a text like "a.", "a or b.", "a, b or c.", "a, b, c or d."
#

sub PutTrailer (@)
{
    my (@links) = ("Back to the <a href=\"./\">index</a>");
    if($editall) {
          push(@links,
              "<a href=\"editusers.cgi?action=add\">add</a> a new user");
    }
    push(@links, @_);

    my $count = $#links;
    my $num = 0;
    print "<P>\n";
    foreach (@links) {
        print $_;
        if ($num == $count) {
            print ".\n";
        }
        elsif ($num == $count-1) {
            print " or ";
        }
        else {
            print ", ";
        }
        $num++;
    }
    PutFooter();
}



#
# Preliminary checks:
#

Bugzilla->login(LOGIN_REQUIRED);

print Bugzilla->cgi->header();

$editall = UserInGroup("editusers");

if (!$editall) {
    if (!Bugzilla->user->can_bless) {
        PutHeader("Not allowed");
        print "Sorry, you aren't a member of the 'editusers' group, and you\n";
        print "don't have permissions to put people in or out of any group.\n";
        print "And so, you aren't allowed to add, modify or delete users.\n";
        PutTrailer();
        exit;
    }
}



#
# often used variables
#
my $user    = trim($::FORM{user}   || '');
my $action  = trim($::FORM{action} || '');
my $localtrailer = '<a href="editusers.cgi?">edit more users</a>';
my $candelete = Param('allowuserdeletion');



#
# action='' -> Ask for match string for users.
#

unless ($action) {
    PutHeader("Select match string");
    print qq{
<FORM METHOD=GET ACTION="editusers.cgi">
<INPUT TYPE=HIDDEN NAME="action" VALUE="list">
List users with login name matching: 
<INPUT SIZE=32 NAME="matchstr">
<SELECT NAME="matchtype">
<OPTION VALUE="substr" SELECTED>case-insensitive substring
<OPTION VALUE="regexp">case-sensitive regexp
<OPTION VALUE="notregexp">not (case-sensitive regexp)
</SELECT>
<BR>
<INPUT TYPE=SUBMIT VALUE="Submit">
</FORM>
};
    PutTrailer();
    exit;
}


#
# action='list' -> Show nice list of matching users
#

if ($action eq 'list') {
    PutHeader("Select user");
    my $query = "";
    my $matchstr = $::FORM{'matchstr'};
    if (exists $::FORM{'matchtype'}) {
      $query = "SELECT login_name,realname,disabledtext " .
          "FROM profiles WHERE login_name ";
      if ($::FORM{'matchtype'} eq 'substr') {
          $query .= "like";
          $matchstr = '%' . $matchstr . '%';
      } elsif ($::FORM{'matchtype'} eq 'regexp') {
          $query .= "regexp";
          $matchstr = '.'
                unless $matchstr;
      } elsif ($::FORM{'matchtype'} eq 'notregexp') {
          $query .= "not regexp";
          $matchstr = '.'
                unless $matchstr;
      } else {
          die "Unknown match type";
      }
      $query .= SqlQuote($matchstr) . " ORDER BY login_name";
    } elsif (exists $::FORM{'group'}) {
      detaint_natural($::FORM{'group'});
      $query = "SELECT DISTINCT login_name,realname,disabledtext " .
          "FROM profiles, user_group_map WHERE profiles.userid = user_group_map.user_id
           AND group_id=" . $::FORM{'group'} . " ORDER BY login_name";
    } else {
      die "Missing parameters";
    }

    SendSQL($query);
    my $count = 0;
    my $header = "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0><TR BGCOLOR=\"#6666FF\">
<TH ALIGN=\"left\">Edit user ...</TH>
<TH ALIGN=\"left\">Real name</TH>
";
    if ($candelete) {
        $header .= "<TH ALIGN=\"left\">Action</TH>\n";
    }
    $header .= "</TR>\n";
    print $header;
    while ( MoreSQLData() ) {
        $count++;
        if ($count % 100 == 0) {
            print "</table>$header";
        }
        my ($user, $realname, $disabledtext) = FetchSQLData();
        my $s = "";
        my $e = "";
        if ($disabledtext) {
            $s = '<span class="bz_inactive">';
            $e = '</span>';
        }
        $realname = ($realname ? html_quote($realname) : "<FONT COLOR=\"red\">missing</FONT>");
        print "<TR>\n";
        print "  <TD VALIGN=\"top\"><A HREF=\"editusers.cgi?action=edit&user=", url_quote($user), "\"><B>$s", html_quote($user), "$e</B></A></TD>\n";
        print "  <TD VALIGN=\"top\">$s$realname$e</TD>\n";
        if ($candelete) {
            print "  <TD VALIGN=\"top\"><A HREF=\"editusers.cgi?action=del&user=", url_quote($user), "\">Delete</A></TD>\n";
        }
        print "</TR>";
    }
    if ($editall) {
        print "<TR>\n";
        my $span = $candelete ? 3 : 2;
        print qq{
<TD VALIGN="top" COLSPAN=$span ALIGN="right">
    <A HREF=\"editusers.cgi?action=add\">add a new user</A>
</TD>
};
        print "</TR>";
    }
    print "</TABLE>\n";
    print "$count users found.\n";

    PutTrailer($localtrailer);
    exit;
}




#
# action='add' -> present form for parameters for new user
#
# (next action will be 'new')
#

if ($action eq 'add') {
    PutHeader("Add user");
    if (!$editall) {
        print "Sorry, you don't have permissions to add new users.";
        PutTrailer();
        exit;
    }

    print "<FORM METHOD=POST ACTION=editusers.cgi>\n";
    print "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=0><TR>\n";

    EmitFormElements(0, '', '', '');

    print "</TR></TABLE>\n<HR>\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Add\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"new\">\n";
    print "</FORM>";

    my $other = $localtrailer;
    $other =~ s/more/other/;
    PutTrailer($other);
    exit;
}



#
# action='new' -> add user entered in the 'action=add' screen
#

if ($action eq 'new') {
    PutHeader("Adding new user");

    if (!$editall) {
        print "Sorry, you don't have permissions to add new users.";
        PutTrailer();
        exit;
    }

    # Cleanups and valididy checks
    my $realname = trim($::FORM{realname} || '');
    # We don't trim the password since that could falsely lead the user
    # to believe a password with a space was accepted even though a space 
    # is an illegal character in a Bugzilla password.
    my $password = $::FORM{'password'};
    my $disabledtext = trim($::FORM{disabledtext} || '');
    my $emailregexp = Param("emailregexp");

    unless ($user) {
        print "You must enter a name for the new user. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
    unless ($user =~ m/$emailregexp/) {
        print "The user name entered must be a valid e-mail address. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
    if (!ValidateNewUser($user)) {
        print "The user '$user' does already exist. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
    my $passworderror = ValidatePassword($password);
    if ( $passworderror ) {
        print $passworderror;
        PutTrailer($localtrailer);
        exit;
    }

    # Add the new user
    SendSQL("INSERT INTO profiles ( " .
            "login_name, cryptpassword, realname,  " .
            "disabledtext" .
            " ) VALUES ( " .
            SqlQuote($user) . "," .
            SqlQuote(Crypt($password)) . "," .
            SqlQuote($realname) . "," .
            SqlQuote($disabledtext) . ")" );

    #+++ send e-mail away

    print "OK, done.<br>\n";
    SendSQL("SELECT last_insert_id()");
    my ($newuserid) = FetchSQLData();

    print "To change ${user}'s permissions, go back and " .
        "<a href=\"editusers.cgi?action=edit&user=" . url_quote($user) .
        "\">edit</a> this user.";
    print "<p>\n";
    PutTrailer($localtrailer);
    exit;

}



#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    PutHeader("Delete user $user");
    if (!$candelete) {
        print "Sorry, deleting users isn't allowed.";
        PutTrailer();
        exit;
    }
    if (!$editall) {
        print "Sorry, you don't have permissions to delete users.";
        PutTrailer();
        exit;
    }
    CheckUser($user);

    # display some data about the user
    SendSQL("SELECT userid, realname FROM profiles
             WHERE login_name=" . SqlQuote($user));
    my ($thisuserid, $realname) = 
      FetchSQLData();
    $realname = ($realname ? html_quote($realname) : "<FONT COLOR=\"red\">missing</FONT>");
    
    print "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0>\n";
    print "<TR BGCOLOR=\"#6666FF\">\n";
    print "  <TH VALIGN=\"top\" ALIGN=\"left\">Part</TH>\n";
    print "  <TH VALIGN=\"top\" ALIGN=\"left\">Value</TH>\n";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Login name:</TD>\n";
    print "  <TD VALIGN=\"top\">$user</TD>\n";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Real name:</TD>\n";
    print "  <TD VALIGN=\"top\">$realname</TD>\n";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Group set:</TD>\n";
    print "  <TD VALIGN=\"top\">";
    SendSQL("SELECT name
             FROM groups, user_group_map
             WHERE groups.id = user_group_map.group_id
             AND user_group_map.user_id = $thisuserid
             AND isbless = 0
             ORDER BY name");
    my $found = 0;
    while ( MoreSQLData() ) {
        my ($name) = FetchSQLData();
        print "<br>\n" if $found;
        print ucfirst $name;
        $found = 1;
    }
    print "none" unless $found;
    print "</TD>\n</TR>";


    # Check if the user is an initialowner
    my $nodelete = '';

    SendSQL("SELECT products.name, components.name " .
            "FROM products, components " .
            "WHERE products.id = components.product_id " .
            " AND initialowner=" . DBname_to_id($user));
    $found = 0;
    while (MoreSQLData()) {
        if ($found) {
            print "<BR>\n";
        } else {
            print "<TR>\n";
            print "  <TD VALIGN=\"top\">Initial owner:</TD>\n";
            print "  <TD VALIGN=\"top\">";
        }
        my ($product, $component) = FetchSQLData();
        print "<a href=\"editcomponents.cgi?product=", url_quote($product),
                "&component=", url_quote($component),
                "&action=edit\">$product: $component</a>";
        $found    = 1;
        $nodelete = 'initial bug owner';
    }
    print "</TD>\n</TR>" if $found;


    # Check if the user is an initialqacontact

    SendSQL("SELECT products.name, components.name " .
            "FROM products, components " .
            "WHERE products.id = components.id " .
            " AND initialqacontact=" . DBname_to_id($user));
    $found = 0;
    while (MoreSQLData()) {
        if ($found) {
            print "<BR>\n";
        } else {
            print "<TR>\n";
            print "  <TD VALIGN=\"top\">Initial QA contact:</TD>\n";
            print "  <TD VALIGN=\"top\">";
        }
        my ($product, $component) = FetchSQLData();
        print "<a href=\"editcomponents.cgi?product=", url_quote($product),
                "&component=", url_quote($component),
                "&action=edit\">$product: $component</a>";
        $found    = 1;
        $nodelete = 'initial QA contact';
    }
    print "</TD>\n</TR>" if $found;

    print "</TABLE>\n";


    if ($nodelete) {
        print "<P>You can't delete this user because '$user' is an $nodelete ",
              "for at least one product.";
        PutTrailer($localtrailer);
        exit;
    }


    print "<H2>Confirmation</H2>\n";
    print "<P>Do you really want to delete this user?<P>\n";

    print "<FORM METHOD=POST ACTION=editusers.cgi>\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Yes, delete\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"delete\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"user\" VALUE=\"$user\">\n";
    print "</FORM>";

    PutTrailer($localtrailer);
    exit;
}



#
# action='delete' -> really delete the user
#

if ($action eq 'delete') {
    PutHeader("Deleting user");
    if (!$candelete) {
        print "Sorry, deleting users isn't allowed.";
        PutTrailer();
        exit;
    }
    if (!$editall) {
        print "Sorry, you don't have permissions to delete users.";
        PutTrailer();
        exit;
    }
    CheckUser($user);

    SendSQL("SELECT userid
             FROM profiles
             WHERE login_name=" . SqlQuote($user));
    my $userid = FetchOneColumn();

    SendSQL("DELETE FROM profiles
             WHERE login_name=" . SqlQuote($user));
    Bugzilla->logout_user_by_id($userid);
    print "User deleted.<BR>\n";

    PutTrailer($localtrailer);
    exit;
}



#
# action='edit' -> present the user edit from
#
# (next action would be 'update')
#

if ($action eq 'edit') {
    PutHeader("Edit user $user");
    CheckUser($user);

    # get data of user
    SendSQL("SELECT userid, realname, disabledtext
             FROM profiles
             WHERE login_name=" . SqlQuote($user));
    my ($thisuserid, $realname, $disabledtext) = FetchSQLData();

    if ($thisuserid > 0) {
        # Force groups to be up to date
        my $changeduser = new Bugzilla::User($thisuserid);
        $changeduser->derive_groups();
    }
    print "<FORM METHOD=POST ACTION=editusers.cgi>\n";
    print "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=0><TR>\n";

    EmitFormElements($thisuserid, $user, $realname, $disabledtext);
    
    print "</TR></TABLE>\n";
    print "<INPUT TYPE=HIDDEN NAME=\"userold\" VALUE=\"$user\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"realnameold\" VALUE=\"$realname\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"disabledtextold\" VALUE=\"" .
        value_quote($disabledtext) . "\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"update\">\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Update\">\n";
    print "<BR>User is a member of any groups shown with a check or grey bar.
           A grey bar indicates indirect membership, either derived from other
           groups (marked with square brackets) or via regular expression
           (marked with '*').<p> 
           Square brackets around the bless checkbox indicate the ability
           to bless users (grant them membership in the group) as a result
           of membership in another group.
       <BR>";

    print "</FORM>";
    if ($candelete) {
        print "<FORM METHOD=POST ACTION=editusers.cgi>\n";
        print "<INPUT TYPE=SUBMIT VALUE=\"Delete User\">\n";
        print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"del\">\n";
        print "<INPUT TYPE=HIDDEN NAME=\"user\" VALUE=\"$user\">\n";
        print "</FORM>";
    }

    my $x = $localtrailer;
    $x =~ s/more/other/;
    PutTrailer($x);
    exit;
}

#
# action='update' -> update the user
#

if ($action eq 'update') {
    my $userold               = trim($::FORM{userold}              || '');
    my $realname              = trim($::FORM{realname}             || '');
    my $realnameold           = trim($::FORM{realnameold}          || '');
    my $password              = $::FORM{password}                  || '';
    my $disabledtext          = trim($::FORM{disabledtext}         || '');
    my $disabledtextold       = trim($::FORM{disabledtextold}      || '');
    my @localtrailers         = ($localtrailer);
    $localtrailer = qq|<a href="editusers.cgi?action=edit&user=XXX">edit user again</a>|;
    PutHeader("Updating user $userold" . ($realnameold && " ($realnameold)"));

    CheckUser($userold);
    SendSQL("SELECT userid FROM profiles
             WHERE login_name=" . SqlQuote($userold));
    my ($thisuserid) = FetchSQLData();

    my $emailregexp = Param("emailregexp");
    unless ($user =~ m/$emailregexp/) {
        print "The user name entered must be a valid e-mail address. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }

    my @grpadd = ();
    my @grpdel = ();
    my $chggrp = 0;
    SendSQL("SELECT id, name FROM groups");
    while (my ($groupid, $name) = FetchSQLData()) {
        next unless ($editall || UserCanBlessGroup($name));
        if ($::FORM{"oldgroup_$groupid"} != ($::FORM{"group_$groupid"} ? 1 : 0)) {
            # group membership changed
            PushGlobalSQLState();
            $chggrp = 1;
            SendSQL("DELETE FROM user_group_map 
                     WHERE user_id = $thisuserid
                     AND group_id = $groupid
                     AND isbless = 0
                     AND grant_type = " . GRANT_DIRECT);
            if ($::FORM{"group_$groupid"}) {
                SendSQL("INSERT INTO user_group_map 
                         (user_id, group_id, isbless, grant_type)
                         VALUES ($thisuserid, $groupid, 0," . GRANT_DIRECT . ")");
                print "Added user to group $name<BR>\n";
                push(@grpadd, $name);
            } else {
                print "Dropped user from group $name<BR>\n";
                push(@grpdel, $name);
            }
            PopGlobalSQLState();
        }
        if ($editall && ($::FORM{"oldbless_$groupid"} != ($::FORM{"bless_$groupid"} ? 1 : 0))) {
            # group membership changed
            PushGlobalSQLState();
            SendSQL("DELETE FROM user_group_map 
                     WHERE user_id = $thisuserid
                     AND group_id = $groupid
                     AND isbless = 1
                     AND grant_type = " . GRANT_DIRECT);
            if ($::FORM{"bless_$groupid"}) {
                SendSQL("INSERT INTO user_group_map 
                         (user_id, group_id, isbless, grant_type)
                         VALUES ($thisuserid, $groupid, 1," . GRANT_DIRECT . ")");
                print "Granted user permission to bless group $name<BR>\n";
            } else {
                print "Revoked user's permission to bless group $name<BR>\n";
            }
            PopGlobalSQLState();
            
        }
    }
    my $fieldid = GetFieldID("bug_group");
    if ($chggrp) {
        SendSQL("INSERT INTO profiles_activity " .  
                "(userid, who, profiles_when, fieldid, oldvalue, newvalue) " .  
                "VALUES " .  "($thisuserid, $::userid, now(), $fieldid, " .  
                SqlQuote(join(", ",@grpdel)) . ", " .
                SqlQuote(join(", ",@grpadd)) . ")");
        SendSQL("UPDATE profiles SET refreshed_when='1900-01-01 00:00:00' " .
                "WHERE userid = $thisuserid");
    }


    # Update the database with the user's new password if they changed it.
    if ( $editall && $password ) {
        my $passworderror = ValidatePassword($password);
        if ( !$passworderror ) {
            my $cryptpassword = SqlQuote(Crypt($password));
            my $loginname = SqlQuote($userold);
            SendSQL("UPDATE  profiles
                     SET     cryptpassword = $cryptpassword
                     WHERE   login_name = $loginname");
            SendSQL("SELECT userid
                     FROM profiles
                     WHERE login_name=" . SqlQuote($userold));
            my $userid = FetchOneColumn();
            Bugzilla->logout_user_by_id($userid);
            print "Updated password.<BR>\n";
        } else {
            print "Did not update password: $passworderror<br>\n";
        }
    }
    if ($editall && $realname ne $realnameold) {
        SendSQL("UPDATE profiles
                 SET realname=" . SqlQuote($realname) . "
                 WHERE login_name=" . SqlQuote($userold));
        print 'Updated real name to <q>' . html_quote($realname) . "</q>.<BR>\n";
    }
    if ($editall && $disabledtext ne $disabledtextold) {
        SendSQL("UPDATE profiles
                 SET disabledtext=" . SqlQuote($disabledtext) . "
                 WHERE login_name=" . SqlQuote($userold));
        SendSQL("SELECT userid
                 FROM profiles
                 WHERE login_name=" . SqlQuote($userold));
        my $userid = FetchOneColumn();
        Bugzilla->logout_user_by_id($userid);
        print "Updated disabled text.<BR>\n";
    }
    if ($editall && $user ne $userold) {
        unless ($user) {
            print "Sorry, I can't delete the user's name.";
            $userold = url_quote($userold);
            $localtrailer =~ s/XXX/$userold/;
            push @localtrailers, $localtrailer;
            PutTrailer(@localtrailers);
            exit;
        }
        if (TestUser($user)) {
            print "Sorry, user name '$user' is already in use.";
            $userold = url_quote($userold);
            $localtrailer =~ s/XXX/$userold/;
            push @localtrailers, $localtrailer;
            PutTrailer($localtrailer);
            exit;
        }

        SendSQL("UPDATE profiles
                 SET login_name=" . SqlQuote($user) . "
                 WHERE login_name=" . SqlQuote($userold));

        print q|Updated user's name to <a href="mailto:| .
              url_quote($user) . '">' . html_quote($user) . "</a>.<BR>\n";
    }
    my $changeduser = new Bugzilla::User($thisuserid);
    $changeduser->derive_groups();

    $user = url_quote($user);
    $localtrailer =~ s/XXX/$user/;
    push @localtrailers, $localtrailer;
    PutTrailer(@localtrailers);
    exit;
}



#
# No valid action found
#

PutHeader("Error");
print "I don't have a clue what you want.<BR>\n";
