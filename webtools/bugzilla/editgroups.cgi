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
# Contributor(s): Dave Miller <justdave@syndicomm.com>
#                 Joel Peshkin <bugreport@peshkin.net>
#                 Jacob Steenhagen <jake@bugzilla.org>

# Code derived from editowners.cgi and editusers.cgi

use strict;
use lib ".";

use Bugzilla::Constants;
require "CGI.pl";

ConnectToDatabase();
confirm_login();

print "Content-type: text/html\n\n";

if (!UserInGroup("creategroups")) {
    PutHeader("Not Authorized","Edit Groups","","Not Authorized for this function!");
    print "<H1>Sorry, you aren't a member of the 'creategroups' group.</H1>\n";
    print "And so, you aren't allowed to edit the groups.\n";
    print "<p>\n";
    PutFooter();
    exit;
}

my $action  = trim($::FORM{action} || '');

# TestGroup: check if the group name exists
sub TestGroup ($)
{
    my $group = shift;

    # does the group exist?
    SendSQL("SELECT name
             FROM groups
             WHERE name=" . SqlQuote($group));
    return FetchOneColumn();
}

sub ShowError ($)
{
    my $msgtext = shift;
    print "<TABLE BGCOLOR=\"#FF0000\" CELLPADDING=15><TR><TD>";
    print "<B>$msgtext</B>";
    print "</TD></TR></TABLE><P>";
    return 1;
}

#
# Displays a text like "a.", "a or b.", "a, b or c.", "a, b, c or d."
#

sub PutTrailer (@)
{
    my (@links) = ("<a href=\"./\">Back to the Main Bugs Page</a>", @_);

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
# action='' -> No action specified, get a list.
#

unless ($action) {
    PutHeader("Edit Groups","Edit Groups","This lets you edit the groups available to put users in.");

    print "<table border=1>\n";
    print "<tr>";
    print "<th>Name</th>";
    print "<th>Description</th>";
    print "<th>User RegExp</th>";
    print "<th>Use For Bugs</th>";
    print "<th>Type</th>";
    print "<th>Action</th>";
    print "</tr>\n";

    SendSQL("SELECT id,name,description,userregexp,isactive,isbuggroup " .
            "FROM groups " .
            "ORDER BY isbuggroup, name");

    while (MoreSQLData()) {
        my ($groupid, $name, $desc, $regexp, $isactive, $isbuggroup) = FetchSQLData();
        print "<tr>\n";
        print "<td>" . html_quote($name) . "</td>\n";
        print "<td>" . html_quote($desc) . "</td>\n";
        print "<td>" . html_quote($regexp) . "&nbsp</td>\n";
        print "<td align=center>";
        print "X" if (($isactive != 0) && ($isbuggroup != 0));
        print "&nbsp</td>\n";
        print "<td> &nbsp ";
        print (($isbuggroup == 0 ) ? "system" : "user"); 
        print "&nbsp</td>\n";
        print "<td align=center valign=middle>
               <a href=\"editgroups.cgi?action=changeform&group=$groupid\">Edit</a>";
        print " | <a href=\"editgroups.cgi?action=del&group=$groupid\">Delete</a>" if ($isbuggroup != 0);
        print "</td></tr>\n";
    }

    print "<tr>\n";
    print "<td colspan=5></td>\n";
    print "<td><a href=\"editgroups.cgi?action=add\">Add Group</a></td>\n";
    print "</tr>\n";
    print "</table>\n";
    print "<p>";
    print "<b>Name</b> is what is used with the UserInGroup() function in any
customized cgi files you write that use a given group.  It can also be used by
people submitting bugs by email to limit a bug to a certain set of groups. <p>";
    print "<b>Description</b> is what will be shown in the bug reports to
members of the group where they can choose whether the bug will be restricted
to others in the same group.<p>";
    print "<b>User RegExp</b> is optional, and if filled in, will automatically
grant membership to this group to anyone creating a new account with an
email address that matches this perl regular expression. Do not forget the trailing \'\$\'.  Example \'\@mycompany\\.com\$\'<p>";
    print "The <b>Use For Bugs</b> flag determines whether or not the group is eligible to be used for bugs.
If you remove this flag, it will no longer be possible for users to add bugs
to this group, although bugs already in the group will remain in the group.
Doing so is a much less drastic way to stop a group from growing
than deleting the group as well as a way to maintain lists of users without cluttering the lists of groups used for bug restrictions.<p>";
    print "The <b>Type</b> field identifies system groups.<p>";  

    PutFooter();
    exit;
}

#
#
# action='changeform' -> present form for altering an existing group
#
# (next action will be 'postchanges')
#

if ($action eq 'changeform') {
    PutHeader("Change Group");

    my $gid = trim($::FORM{group} || '');
    unless ($gid) {
        ShowError("No group specified.<BR>" .
                  "Click the <b>Back</b> button and try again.");
        PutFooter();
        exit;
    }

    SendSQL("SELECT id, name, description, userregexp, isactive, isbuggroup
             FROM groups WHERE id=" . SqlQuote($gid));
    my ($group_id, $name, $description, $rexp, $isactive, $isbuggroup) 
        = FetchSQLData();

    print "<FORM METHOD=POST ACTION=editgroups.cgi>\n";
    print "<TABLE BORDER=1 CELLPADDING=4>";
    print "<TR><TH>Group:</TH><TD>";
    if ($isbuggroup == 0) {
        print html_quote($name);
    } else {
        print "<INPUT TYPE=HIDDEN NAME=\"oldname\" VALUE=" . 
        html_quote($name) . ">
        <INPUT SIZE=60 NAME=\"name\" VALUE=\"" . html_quote($name) . "\">";
    }
    print "</TD></TR><TR><TH>Description:</TH><TD>";
    if ($isbuggroup == 0) {
        print html_quote($description);
    } else {
        print "<INPUT TYPE=HIDDEN NAME=\"olddesc\" VALUE=\"" .
        html_quote($description) . "\">
        <INPUT SIZE=70 NAME=\"desc\" VALUE=\"" . 
            html_quote($description) . "\">";
    }
    print "</TD></TR><TR>
           <TH>User Regexp:</TH><TD>";
    print "<INPUT TYPE=HIDDEN NAME=\"oldrexp\" VALUE=\"" . 
           html_quote($rexp) . "\">
           <INPUT SIZE=40 NAME=\"rexp\" VALUE=\"" . 
           html_quote($rexp) . "\"></TD></TR>";
    if ($isbuggroup == 1) {
        print "<TR><TH>Use For Bugs:</TH><TD>
        <INPUT TYPE=checkbox NAME =\"isactive\" VALUE=1 " . (($isactive == 1) ? "CHECKED" : "") . ">
        <INPUT TYPE=HIDDEN NAME=\"oldisactive\" VALUE=$isactive>
        </TD>
        </TR>";
    }
    print "</TABLE>
           <BR>
           Users become members of this group in one of three ways:
           <BR>
           - by being explicity included when the user is edited
           <BR>
           - by matching the user regexp above
           <BR>
           - by being a member of one of the groups included in this group
           by checking the boxes  
           below. <P>\n";

    print "<TABLE>";
    print "<TR><TD COLSPAN=4>Members of these groups can grant membership to this group</TD></TR>";
    print "<TR><TD ALIGN=CENTER>|</TD><TD COLSPAN=3>Members of these groups are included in this group</TD></TR>";
    print "<TR><TD ALIGN=CENTER>|</TD><TD ALIGN=CENTER>|</TD><TD COLSPAN=2></TD><TR>";

    # For each group, we use left joins to establish the existence of
    # a record making that group a member of this group
    # and the existence of a record permitting that group to bless
    # this one
    SendSQL("SELECT groups.id, groups.name, groups.description," .
             " group_group_map.member_id IS NOT NULL," .
             " B.member_id IS NOT NULL" .
             " FROM groups" .
             " LEFT JOIN group_group_map" .
             " ON group_group_map.member_id = groups.id" .
             " AND group_group_map.grantor_id = $group_id" .
             " AND group_group_map.isbless = 0" .
             " LEFT JOIN group_group_map as B" .
             " ON B.member_id = groups.id" .
             " AND B.grantor_id = $group_id" .
             " AND B.isbless" .
             " WHERE groups.id != $group_id ORDER by name");

    while (MoreSQLData()) {
        my ($grpid, $grpnam, $grpdesc, $grpmember, $blessmember) = FetchSQLData();
        my $grpchecked = $grpmember ? "CHECKED" : "";
        my $blesschecked = $blessmember ? "CHECKED" : "";
        print "<TR>";
        print "<TD><INPUT TYPE=checkbox NAME=\"bless-$grpid\" $blesschecked VALUE=1>";
        print "<INPUT TYPE=HIDDEN NAME=\"oldbless-$grpid\" VALUE=$blessmember></TD>";
        print "<TD><INPUT TYPE=checkbox NAME=\"grp-$grpid\" $grpchecked VALUE=1>";
        print "<INPUT TYPE=HIDDEN NAME=\"oldgrp-$grpid\" VALUE=$grpmember></TD>";
        print "<TD><B>" . html_quote($grpnam) . "</B></TD>";
        print "<TD>" . html_quote($grpdesc) . "</TD>";
        print "</TR>\n";
    }

    print "</TABLE><BR>";
    print "<INPUT TYPE=SUBMIT VALUE=\"Submit\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"postchanges\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"group\" VALUE=$gid>\n";
    print "</FORM>";



    PutTrailer("<a href=editgroups.cgi>Back to group list</a>");
    exit;
}

#
# action='add' -> present form for parameters for new group
#
# (next action will be 'new')
#

if ($action eq 'add') {
    PutHeader("Add group");

    print "<FORM METHOD=POST ACTION=editgroups.cgi>\n";
    print "<TABLE BORDER=1 CELLPADDING=4 CELLSPACING=0><TR>\n";
    print "<th>New Name</th>";
    print "<th>New Description</th>";
    print "<th>New User RegExp</th>";
    print "<th>Use For Bugs</th>";
    print "</tr><tr>";
    print "<td><input size=20 name=\"name\"></td>\n";
    print "<td><input size=40 name=\"desc\"></td>\n";
    print "<td><input size=30 name=\"regexp\"></td>\n";
    print "<td><input type=\"checkbox\" name=\"isactive\" value=\"1\" checked></td>\n";
    print "</TR></TABLE>\n<HR>\n";
    print "<input type=\"checkbox\" name=\"insertnew\" value=\"1\"";
    print " checked" if Param("makeproductgroups");
    print ">\n";
    print "Insert new group into all existing products.<P>\n";
    print "<INPUT TYPE=SUBMIT VALUE=\"Add\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"new\">\n";
    print "</FORM>";

    print "<p>";
    print "<b>Name</b> is what is used with the UserInGroup() function in any
customized cgi files you write that use a given group.  It can also be used by
people submitting bugs by email to limit a bug to a certain set of groups.  It
may not contain any spaces.<p>";
    print "<b>Description</b> is what will be shown in the bug reports to
members of the group where they can choose whether the bug will be restricted
to others in the same group.<p>";
    print "The <b>Use For Bugs</b> flag determines whether or not the group is eligible to be used for bugs.
If you clear this, it will no longer be possible for users to add bugs
to this group, although bugs already in the group will remain in the group.
Doing so is a much less drastic way to stop a group from growing
than deleting the group would be.  <b>Note: If you are creating a group, you
probably want it to be usable for bugs, in which case you should leave this checked.</b><p>";
    print "<b>User RegExp</b> is optional, and if filled in, will ";
    print "automatically grant membership to this group to anyone with an ";
    print "email address that matches this regular expression.<p>\n";
    print "By default, the new group will be associated with existing ";
    print "products. Unchecking the \"Insert new group into all existing ";
    print "products\" option will prevent this and make the group become ";
    print "visible only when its controls have been added to a product.<P>\n";

    PutTrailer("<a href=editgroups.cgi>Back to the group list</a>");
    exit;
}



#
# action='new' -> add group entered in the 'action=add' screen
#

if ($action eq 'new') {
    PutHeader("Adding new group");

    # Cleanups and valididy checks
    my $name = trim($::FORM{name} || '');
    my $desc = trim($::FORM{desc} || '');
    my $regexp = trim($::FORM{regexp} || '');
    # convert an undefined value in the inactive field to zero
    # (this occurs when the inactive checkbox is not checked
    # and the browser does not send the field to the server)
    my $isactive = $::FORM{isactive} || 0;

    unless ($name) {
        ShowError("You must enter a name for the new group.<BR>" .
                  "Please click the <b>Back</b> button and try again.");
        PutFooter();
        exit;
    }
    unless ($desc) {
        ShowError("You must enter a description for the new group.<BR>" .
                  "Please click the <b>Back</b> button and try again.");
        PutFooter();
        exit;
    }
    if (TestGroup($name)) {
        ShowError("The group '" . $name . "' already exists.<BR>" .
                  "Please click the <b>Back</b> button and try again.");
        PutFooter();
        exit;
    }

    if ($isactive != 0 && $isactive != 1) {
        ShowError("The active flag was improperly set.  There may be " . 
                  "a problem with Bugzilla or a bug in your browser.<br>" . 
                  "Please click the <b>Back</b> button and try again.");
        PutFooter();
        exit;
    }

    if (!eval {qr/$regexp/}) {
        ShowError("The regular expression you entered is invalid. " .
                  "Please click the <b>Back</b> button and try again.");
        PutFooter();
        exit;
    }

    # Add the new group
    SendSQL("INSERT INTO groups ( " .
            "name, description, isbuggroup, userregexp, isactive, last_changed " .
            " ) VALUES ( " .
            SqlQuote($name) . ", " .
            SqlQuote($desc) . ", " .
            "1," .
            SqlQuote($regexp) . ", " . 
            $isactive . ", NOW())" );
    SendSQL("SELECT last_insert_id()");
    my $gid = FetchOneColumn();
    my $admin = GroupNameToId('admin');
    SendSQL("INSERT INTO group_group_map (member_id, grantor_id, isbless)
             VALUES ($admin, $gid, 0)");
    SendSQL("INSERT INTO group_group_map (member_id, grantor_id, isbless)
             VALUES ($admin, $gid, 1)");
    # Permit all existing products to use the new group if makeproductgroups.
    if ($::FORM{insertnew}) {
        SendSQL("INSERT INTO group_control_map " .
                "(group_id, product_id, entry, membercontrol, " .
                "othercontrol, canedit) " .
                "SELECT $gid, products.id, 0, " .
                CONTROLMAPSHOWN . ", " .
                CONTROLMAPNA . ", 0 " .
                "FROM products");
    }
    print "OK, done.<p>\n";
    PutTrailer("<a href=\"editgroups.cgi?action=add\">Add another group</a>",
               "<a href=\"editgroups.cgi\">Back to the group list</a>");
    exit;
}

#
# action='del' -> ask if user really wants to delete
#
# (next action would be 'delete')
#

if ($action eq 'del') {
    PutHeader("Delete group");
    my $gid = trim($::FORM{group} || '');
    unless ($gid) {
        ShowError("No group specified.<BR>" .
                  "Click the <b>Back</b> button and try again.");
        PutFooter();
        exit;
    }
    SendSQL("SELECT id FROM groups WHERE id=" . SqlQuote($gid));
    if (!FetchOneColumn()) {
        ShowError("That group doesn't exist.<BR>" .
                  "Click the <b>Back</b> button and try again.");
        PutFooter();
        exit;
    }
    SendSQL("SELECT name,description " .
            "FROM groups " .
            "WHERE id = " . SqlQuote($gid));

    my ($name, $desc) = FetchSQLData();
    print "<table border=1>\n";
    print "<tr>";
    print "<th>Id</th>";
    print "<th>Name</th>";
    print "<th>Description</th>";
    print "</tr>\n";
    print "<tr>\n";
    print "<td>$gid</td>\n";
    print "<td>$name</td>\n";
    print "<td>$desc</td>\n";
    print "</tr>\n";
    print "</table>\n";

    print "<FORM METHOD=POST ACTION=editgroups.cgi>\n";
    my $cantdelete = 0;
    SendSQL("SELECT user_id FROM user_group_map 
             WHERE group_id = $gid AND isbless = 0");
    if (!FetchOneColumn()) {} else {
       $cantdelete = 1;
       print "
<B>One or more users belong to this group. You cannot delete this group while
there are users in it.</B><BR>
<A HREF=\"editusers.cgi?action=list&group=$gid\">Show me which users.</A> - <INPUT TYPE=CHECKBOX NAME=\"removeusers\">Remove all users from
this group for me<P>
";
    }
    SendSQL("SELECT bug_id FROM bug_group_map WHERE group_id = $gid");
    my $buglist="";
    if (MoreSQLData()) {
        $cantdelete = 1;
        my $buglist = "0";
        while (MoreSQLData()) {
            my ($bug) = FetchSQLData();
            $buglist .= "," . $bug;
        }
       print "
<B>One or more bug reports are visible only to this group.
You cannot delete this group while any bugs are using it.</B><BR>
<A HREF=\"buglist.cgi?bug_id=$buglist\">Show me which bugs.</A> -
<INPUT TYPE=CHECKBOX NAME=\"removebugs\">Remove all bugs from this group
restriction for me<BR>
<B>NOTE:</B> It's quite possible to make confidential bugs public by checking
this box.  It is <B>strongly</B> suggested that you review the bugs in this
group before checking the box.<P>
";
    }
    SendSQL("SELECT name FROM products WHERE name=" . SqlQuote($name));
    if (MoreSQLData()) {
       $cantdelete = 1;
       print "
<B>This group is tied to the <U>$name</U> product.
You cannot delete this group while it is tied to a product.</B><BR>
<INPUT TYPE=CHECKBOX NAME=\"unbind\">Delete this group anyway, and make the
<U>$name</U> product publicly visible.<BR>
";
    }

    print "<H2>Confirmation</H2>\n";
    print "<P>Do you really want to delete this group?\n";
    if ($cantdelete) {
      print "<BR><B>You must check all of the above boxes or correct the " .
            "indicated problems first before you can proceed.</B>";
    }
    print "<P><INPUT TYPE=SUBMIT VALUE=\"Yes, delete\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"action\" VALUE=\"delete\">\n";
    print "<INPUT TYPE=HIDDEN NAME=\"group\" VALUE=\"$gid\">\n";
    print "</FORM>";

    PutTrailer("<a href=editgroups.cgi>No, go back to the group list</a>");
    exit;
}

#
# action='delete' -> really delete the group
#

if ($action eq 'delete') {
    PutHeader("Deleting group");
    my $gid = trim($::FORM{group} || '');
    unless ($gid) {
        ShowError("No group specified.<BR>" .
                  "Click the <b>Back</b> button and try again.");
        PutFooter();
        exit;
    }
    SendSQL("SELECT name " .
            "FROM groups " .
            "WHERE id = " . SqlQuote($gid));
    my ($name) = FetchSQLData();

    my $cantdelete = 0;

    SendSQL("SELECT user_id FROM user_group_map 
             WHERE group_id = $gid AND isbless = 0");
    if (FetchOneColumn()) {
      if (!defined $::FORM{'removeusers'}) {
        $cantdelete = 1;
      }
    }
    SendSQL("SELECT bug_id FROM bug_group_map WHERE group_id = $gid");
    if (FetchOneColumn()) {
      if (!defined $::FORM{'removebugs'}) {
        $cantdelete = 1;
      }
    }
    SendSQL("SELECT name FROM products WHERE name=" . SqlQuote($name));
    if (FetchOneColumn()) {
      if (!defined $::FORM{'unbind'}) {
        $cantdelete = 1;
      }
    }

    if ($cantdelete == 1) {
      ShowError("This group cannot be deleted because there are " .
          "records in the database which refer to it.  All such records " .
          "must be removed or altered to remove the reference to this " .
          "group before the group can be deleted.");
      print "<A HREF=\"editgroups.cgi?action=del&group=$gid\">" .
            "View the list of which records are affected</A><BR>";
      PutTrailer("<a href=editgroups.cgi>Back to group list</a>");
      exit;
    }

    SendSQL("DELETE FROM user_group_map WHERE group_id = $gid");
    SendSQL("DELETE FROM group_group_map WHERE grantor_id = $gid");
    SendSQL("DELETE FROM bug_group_map WHERE group_id = $gid");
    SendSQL("DELETE FROM group_control_map WHERE group_id = $gid");
    SendSQL("DELETE FROM groups WHERE id = $gid");
    print "<B>Group $gid has been deleted.</B><BR>";


    PutTrailer("<a href=editgroups.cgi>Back to group list</a>");
    exit;
}

#
# action='postchanges' -> update the groups
#

if ($action eq 'postchanges') {
    PutHeader("Updating group hierarchy");
    my $gid = trim($::FORM{group} || '');
    unless ($gid) {
        ShowError("No group specified.<BR>" .
                  "Click the <b>Back</b> button and try again.");
        PutFooter();
        exit;
    }
    SendSQL("SELECT isbuggroup FROM groups WHERE id = $gid");
    my ($isbuggroup) = FetchSQLData();
    my $chgs = 0;
    if (($isbuggroup == 1) && ($::FORM{"oldname"} ne $::FORM{"name"})) {
        $chgs = 1;
        SendSQL("UPDATE groups SET name = " . 
            SqlQuote($::FORM{"name"}) . " WHERE id = $gid");
    }
    if (($isbuggroup == 1) && ($::FORM{"olddesc"} ne $::FORM{"desc"})) {
        $chgs = 1;
        SendSQL("UPDATE groups SET description = " . 
            SqlQuote($::FORM{"desc"}) . " WHERE id = $gid");
    }
    if ($::FORM{"oldrexp"} ne $::FORM{"rexp"}) {
        $chgs = 1;
        if (!eval {qr/$::FORM{"rexp"}/}) {
            ShowError("The regular expression you entered is invalid. " .
                      "Please click the <b>Back</b> button and try again.");
            PutFooter();
            exit;
        }
        SendSQL("UPDATE groups SET userregexp = " . 
            SqlQuote($::FORM{"rexp"}) . " WHERE id = $gid");
    }
    if (($isbuggroup == 1) && ($::FORM{"oldisactive"} ne $::FORM{"isactive"})) {
        $chgs = 1;
        SendSQL("UPDATE groups SET isactive = " . 
            SqlQuote($::FORM{"isactive"}) . " WHERE id = $gid");
    }
    
    print "Checking....";
    foreach my $b (grep(/^oldgrp-\d*$/, keys %::FORM)) {
        if (defined($::FORM{$b})) {
            my $v = substr($b, 7);
            my $grp = $::FORM{"grp-$v"} || 0;
            if ($::FORM{"oldgrp-$v"} != $grp) {
                $chgs = 1;
                print "changed";
                if ($grp != 0) {
                    print " set ";
                    SendSQL("INSERT INTO group_group_map 
                             (member_id, grantor_id, isbless)
                             VALUES ($v, $gid, 0)");
                } else {
                    print " cleared ";
                    SendSQL("DELETE FROM group_group_map
                             WHERE member_id = $v AND grantor_id = $gid
                             AND isbless = 0");
                }
            }

            my $bless = $::FORM{"bless-$v"} || 0;
            if ($::FORM{"oldbless-$v"} != $bless) {
                $chgs = 1;
                print "changed";
                if ($bless != 0) {
                    print " set ";
                    SendSQL("INSERT INTO group_group_map 
                             (member_id, grantor_id, isbless)
                             VALUES ($v, $gid, 1)");
                } else {
                    print " cleared ";
                    SendSQL("DELETE FROM group_group_map
                             WHERE member_id = $v AND grantor_id = $gid
                             AND isbless = 1");
                }
            }

        }
    }
    if (!$chgs) {
        print "You didn't change anything!<BR>\n";
        print "If you really meant it, hit the <B>Back</B> button and try again.<p>\n";
    } else {
        SendSQL("UPDATE groups SET last_changed = NOW() WHERE id = $gid");
        print "Done.<p>\n";
    }
    PutTrailer("<a href=editgroups.cgi>Back to the group list</a>");
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

PutTrailer("<a href=editgroups.cgi>Try the group list</a>");
