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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Holger
# Schurig. Portions created by Holger Schurig are
# Copyright (C) 1999 Holger Schurig. All
# Rights Reserved.
#
# Contributor(s): Holger Schurig <holgerschurig@nikocity.de>
#
#
# Direct any questions on this source code to
#
# Holger Schurig <holgerschurig@nikocity.de>

use diagnostics;
use strict;

require "CGI.pl";
require "globals.pl";





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

    # do we have a product?
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



#
# Displays the form to edit a user parameters
#

sub EmitFormElements ($$$$$)
{
    my ($user, $password, $realname, $groupset, $emailnotification) = @_;

    print "  <TH ALIGN=\"right\">Login name:</TH>\n";
    print "  <TD><INPUT SIZE=64 MAXLENGTH=255 NAME=\"user\" VALUE=\"$user\"></TD>\n";

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Real name:</TH>\n";
    print "  <TD><INPUT SIZE=64 MAXLENGTH=255 NAME=\"realname\" VALUE=\"$realname\"></TD>\n";

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Password:</TH>\n";
    print "  <TD><INPUT SIZE=16 MAXLENGTH=16 NAME=\"password\" VALUE=\"$password\"></TD>\n";

    print "</TR><TR>\n";
    print "  <TH ALIGN=\"right\">Email notification:</TH>\n";
    print qq{<TD><SELECT NAME="emailnotification">};
    foreach my $i (["ExcludeSelfChanges", "All qualifying bugs except those which I change"],
                   ["CConly", "Only those bugs which I am listed on the CC line"],
                   ["All", "All qualifying bugs"]) {
        my ($tag, $desc) = (@$i);
        my $selectpart = "";
        if ($tag eq $emailnotification) {
            $selectpart = " SELECTED";
        }
        print qq{<OPTION$selectpart VALUE="$tag">$desc\n};
    }
    print "</SELECT></TD>\n";

    SendSQL("SELECT bit,name,description,bit & $groupset != 0
	     FROM groups
	     ORDER BY name");
    while (MoreSQLData()) {
	my ($bit,$name,$description,$checked) = FetchSQLData();
	print "</TR><TR>\n";
	print "  <TH ALIGN=\"right\">", ucfirst($name), ":</TH>\n";
	$checked = ($checked) ? "CHECKED" : "";
	print "  <TD><INPUT TYPE=CHECKBOX NAME=\"bit_$name\" $checked VALUE=\"$bit\"> $description</TD>\n";
    }

}



#
# Displays a text like "a.", "a or b.", "a, b or c.", "a, b, c or d."
#

sub PutTrailer (@)
{
    my (@links) = ("Back to the <A HREF=\"index.html\">index</A>", @_);

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

confirm_login();

print "Content-type: text/html\n\n";

unless (UserInGroup("editusers")) {
    PutHeader("Not allowed");
    print "Sorry, you aren't a member of the 'editusers' group.\n";
    print "And so, you aren't allowed to add, modify or delete users.\n";
    PutTrailer();
    exit;
}



#
# often used variables
#
my $user    = trim($::FORM{user}   || '');
my $action  = trim($::FORM{action} || '');
my $localtrailer = "<A HREF=\"editusers.cgi\">edit</A> more users";
my $candelete = Param('allowuserdeletion');



#
# action='' -> Ask for match string for users.
#

unless ($action) {
    PutHeader("Select match string");
    print qq{
<FORM METHOD=POST ACTION="editusers.cgi">
<INPUT TYPE=HIDDEN NAME="action" VALUE="list">
List users with login name matching: 
<INPUT SIZE=32 NAME="matchstr">
<SELECT NAME="matchtype">
<OPTION VALUE="substr" SELECTED>case-insensitive substring
<OPTION VALUE="regexp" SELECTED>case-sensitive regexp
<OPTION VALUE="notregexp" SELECTED>not (case-sensitive regexp)
</SELECT>
<BR>
<INPUT TYPE=SUBMIT VALUE="Submit">
};
    PutTrailer();
    exit;
}


#
# action='list' -> Show nice list of matching users
#

if ($action eq 'list') {
    PutHeader("Select user");
    my $query = "SELECT login_name,realname FROM profiles WHERE login_name ";
    if ($::FORM{'matchtype'} eq 'substr') {
        $query .= "like";
        $::FORM{'matchstr'} = '%' . $::FORM{'matchstr'} . '%';
    } elsif ($::FORM{'matchtype'} eq 'regexp') {
        $query .= "regexp";
    } elsif ($::FORM{'matchtype'} eq 'notregexp') {
        $query .= "not regexp";
    } else {
        die "Unknown match type";
    }
    $query .= SqlQuote($::FORM{'matchstr'}) . " ORDER BY login_name";

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
	my ($user, $realname) = FetchSQLData();
	$realname ||= "<FONT COLOR=\"red\">missing</FONT>";
	print "<TR>\n";
	print "  <TD VALIGN=\"top\"><A HREF=\"editusers.cgi?action=edit&user=", url_quote($user), "\"><B>$user</B></A></TD>\n";
	print "  <TD VALIGN=\"top\">$realname</TD>\n";
        if ($candelete) {
            print "  <TD VALIGN=\"top\"><A HREF=\"editusers.cgi?action=del&user=", url_quote($user), "\">Delete</A></TD>\n";
        }
	print "</TR>";
    }
    print "<TR>\n";
    my $span = $candelete ? 3 : 2;
    print qq{
<TD VALIGN="top" COLSPAN=$span ALIGN="right">
    <A HREF=\"editusers.cgi?action=add\">Add a new user</A>
</TD>
};
    print "</TR></TABLE>\n";
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

    print "<FORM METHOD=POST ACTION=editusers.cgi>\n";
    print "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=0><TR>\n";

    EmitFormElements('', '', '', 0, 'ExcludeSelfChanges');

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

    # Cleanups and valididy checks
    my $realname = trim($::FORM{realname} || '');
    my $password = trim($::FORM{password} || '');

    unless ($user) {
        print "You must enter a name for the new user. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
    unless ($user =~ /^[^\@]+\@[^\@]+$/) {
        print "The user name entered must be a valid e-mail address. Please press\n";
        print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }
    if (TestUser($user)) {
	print "The user '$user' does already exist. Please press\n";
	print "<b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
	exit;
    }
    if ($password !~ /^[a-zA-Z0-9-_]*$/ || length($password) < 3 || length($password) > 16) {
        print "The new user must have a password. The password must be between ",
	      "3 and 16 characters long and must contain only numbers, letters, ",
	      "hyphens and underlines. Press <b>Back</b> and try again.\n";
        PutTrailer($localtrailer);
        exit;
    }

    my $bits = "0";
    foreach (keys %::FORM) {
	next unless /^bit_/;
	#print "$_=$::FORM{$_}<br>\n";
	$bits .= "+ $::FORM{$_}";
    }
    

    # Add the new user
    SendSQL("INSERT INTO profiles ( " .
          "login_name, password, cryptpassword, realname, groupset" .
          " ) VALUES ( " .
          SqlQuote($user) . "," .
          SqlQuote($password) . "," .
          "encrypt(" . SqlQuote($password) . ")," .
          SqlQuote($realname) . "," .
          $bits . ")" );

    #+++ send e-mail away

    print "OK, done.<p>\n";
    PutTrailer($localtrailer,
	"<a href=\"editusers.cgi?action=add\">add</a> another user.");
    exit;

}



#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    PutHeader("Delete user");
    if (!$candelete) {
        print "Sorry, deleting users isn't allowed.";
        PutTrailer();
    }
    CheckUser($user);

    # display some data about the user
    SendSQL("SELECT realname, groupset, emailnotification, login_name
	     FROM profiles
	     WHERE login_name=" . SqlQuote($user));
    my ($realname, $groupset, $emailnotification) = FetchSQLData();
    $realname ||= "<FONT COLOR=\"red\">missing</FONT>";
    
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
    print "  <TD VALIGN=\"top\">E-Mail notification:</TD>\n";
    print "  <TD VALIGN=\"top\">$emailnotification</TD>\n";

    print "</TR><TR>\n";
    print "  <TD VALIGN=\"top\">Group set:</TD>\n";
    print "  <TD VALIGN=\"top\">";
    SendSQL("SELECT bit, name
	     FROM groups
	     ORDER BY name");
    my $found = 0;
    while ( MoreSQLData() ) {
	my ($bit,$name) = FetchSQLData();
	if ($bit & $groupset) {
	    print "<br>\n" if $found;
	    print ucfirst $name;
	    $found = 1;
	}
    }
    print "none" unless $found;
    print "</TD>\n</TR>";


    # Check if the user is an initialowner
    my $nodelete = '';

    SendSQL("SELECT program, value
	     FROM components
	     WHERE initialowner=" . SqlQuote($user));
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

    SendSQL("SELECT program, value
	     FROM components
	     WHERE initialqacontact=" . SqlQuote($user));
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
    }
    CheckUser($user);

    SendSQL("SELECT userid
	     FROM profiles
	     WHERE login_name=" . SqlQuote($user));
    my $userid = FetchOneColumn();

    SendSQL("DELETE FROM profiles
	     WHERE login_name=" . SqlQuote($user));
    SendSQL("DELETE FROM logincookies
	     WHERE userid=" . $userid);
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
    PutHeader("Edit user");
    CheckUser($user);

    # get data of user
    SendSQL("SELECT password, realname, groupset, emailnotification
	     FROM profiles
	     WHERE login_name=" . SqlQuote($user));
    my ($password, $realname, $groupset, $emailnotification) = FetchSQLData();

    print "<FORM METHOD=POST ACTION=editusers.cgi>\n";
    print "<TABLE BORDER=0 CELLPADDING=4 CELLSPACING=0><TR>\n";

    EmitFormElements($user, $password, $realname, $groupset,
                     $emailnotification);
    
    print "</TR></TABLE>\n";

    print "<INPUT TYPE=HIDDEN NAME=\"userold\" VALUE=\"$user\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"passwordold\" VALUE=\"$password\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"realnameold\" VALUE=\"$realname\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"groupsetold\" VALUE=\"$groupset\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"emailnotificationold\" VALUE=\"$emailnotification\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"update\">\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Update\">\n";

    print "</FORM>";

    my $x = $localtrailer;
    $x =~ s/more/other/;
    PutTrailer($x);
    exit;
}

#
# action='update' -> update the user
#

if ($action eq 'update') {
    PutHeader("Update User");

    my $userold               = trim($::FORM{userold}              || '');
    my $realname              = trim($::FORM{realname}             || '');
    my $realnameold           = trim($::FORM{realnameold}          || '');
    my $password              = trim($::FORM{password}             || '');
    my $passwordold           = trim($::FORM{passwordold}          || '');
    my $emailnotification     = trim($::FORM{emailnotification}    || '');
    my $emailnotificationold  = trim($::FORM{emailnotificationold} || '');
    my $groupsetold           = trim($::FORM{groupsetold}          || '');

    my $groupset = "0";
    foreach (keys %::FORM) {
	next unless /^bit_/;
	#print "$_=$::FORM{$_}<br>\n";
	$groupset .= "+ $::FORM{$_}";
    }

    CheckUser($userold);

    # Note that the order of this tests is important. If you change
    # them, be sure to test for WHERE='$product' or WHERE='$productold'

    if ($groupset != $groupsetold) {
        SendSQL("UPDATE profiles
		 SET groupset=" . $groupset . "
		 WHERE login_name=" . SqlQuote($userold));
	print "Updated permissions.\n";
    }

    if ($emailnotification ne $emailnotificationold) {
        SendSQL("UPDATE profiles
		 SET emailnotification=" . SqlQuote($emailnotification) . "
		 WHERE login_name=" . SqlQuote($userold));
	print "Updated email notification.<BR>\n";
    }

    if ($password ne $passwordold) {
        my $q = SqlQuote($password);
        SendSQL("UPDATE profiles
		 SET password= $q, cryptpassword = ENCRYPT($q)
		 WHERE login_name=" . SqlQuote($userold));
	print "Updated password.<BR>\n";
    }
    if ($realname ne $realnameold) {
        SendSQL("UPDATE profiles
		 SET realname=" . SqlQuote($realname) . "
		 WHERE login_name=" . SqlQuote($userold));
	print "Updated real name.<BR>\n";
    }
    if ($user ne $userold) {
	unless ($user) {
	    print "Sorry, I can't delete the user's name.";
            PutTrailer($localtrailer);
	    exit;
        }
	if (TestUser($user)) {
	    print "Sorry, user name '$user' is already in use.";
            PutTrailer($localtrailer);
	    exit;
        }

        SendSQL("UPDATE profiles
		 SET login_name=" . SqlQuote($user) . "
		 WHERE login_name=" . SqlQuote($userold));

	print "Updated user's name.<BR>\n";
    }

    PutTrailer($localtrailer);
    exit;
}



#
# No valid action found
#

PutHeader("Error");
print "I don't have a clue what you want.<BR>\n";

foreach ( sort keys %::FORM) {
    print "$_: $::FORM{$_}<BR>\n";
}
