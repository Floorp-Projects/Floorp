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

require "CGI.pl";


sub sillyness {
    my $zz;
    $zz = $::anyvotesallowed;
}

confirm_login();

print "Content-type: text/html\n\n";

GetVersionTable();

if (! defined $::FORM{'pwd1'}) {
    PutHeader("Preferences", "Change your password and<br>other preferences",
              $::COOKIE{'Bugzilla_login'});

    my $qacontactpart = "";
    if (Param('useqacontact')) {
        $qacontactpart = ", the current QA Contact";
    }
    my $loginname = SqlQuote($::COOKIE{'Bugzilla_login'});
    SendSQL("select emailnotification,realname,newemailtech from profiles where login_name = " .
            $loginname);
    my ($emailnotification, $realname, $newemailtech) = (FetchSQLData());
    $realname = value_quote($realname);
    print qq{
<form method=post>
<hr>
<table>
<tr>
<td align=right>Please enter the new password for <b>$::COOKIE{'Bugzilla_login'}</b>:</td>
<td><input type=password name="pwd1"></td>
</tr>
<tr>
<td align=right>Re-enter your new password:</td>
<td><input type=password name="pwd2"></td>
</tr>
<tr>
<td align=right>Your real name (optional):</td>
<td><input size=35 name=realname value="$realname"></td>
</tr>
</table>
<hr>
<table>
<tr>
<td align=right>Bugzilla will send out email notification of changed bugs to 
the current owner, the submitter of the bug$qacontactpart, and anyone on the
CC list.  However, you can suppress some of those email notifications.
On which of these bugs would you like email notification of changes?</td>
<td><SELECT NAME="emailnotification">
};
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
    print "
</SELECT>
</td>
</tr>
";
    if (Param("newemailtech")) {
        my $checkedpart = $newemailtech ? "CHECKED" : "";
        print qq{
<tr><td colspan=2><hr></td></tr>
<tr><td align=right><font color="red">New!</font>  Bugzilla has a new email 
notification scheme.  It is <b>experimental and bleeding edge</b> and will 
hopefully evolve into a brave new happy world where all the spam and ugliness
of the old notifications will go away.  If you wish to sign up for this (and 
risk any bugs), check here.</td>
<td><input type="checkbox" name="newemailtech" $checkedpart>New email tech</td>
</tr>
};
    }
    print "
</table>
<hr>
<input type=submit value=Submit>
</form>
<hr>";
    if ($::anyvotesallowed) {
        print qq{<a href="showvotes.cgi">Review your votes</a><hr>\n};
    }
    PutFooter();
    exit;
}

if ($::FORM{'pwd1'} ne $::FORM{'pwd2'}) {
    print "<H1>Try again.</H1>
The two passwords you entered did not match.  Please click <b>Back</b> and try again.\n";
    PutFooter();
    exit;
}


my $pwd = $::FORM{'pwd1'};


if ($pwd ne "") {
    if ($pwd !~ /^[a-zA-Z0-9-_]*$/ || length($pwd) < 3 || length($pwd) > 15) {
        print "<H1>Sorry; we're picky.</H1>
Please choose a password that is between 3 and 15 characters long, and that
contains only numbers, letters, hyphens, or underlines.
<p>
Please click <b>Back</b> and try again.\n";
        PutFooter();
        exit;
    }
    
    
    my $qpwd = SqlQuote($pwd);
    SendSQL("UPDATE profiles SET password=$qpwd,cryptpassword=encrypt($qpwd) 
             WHERE login_name = " .
            SqlQuote($::COOKIE{'Bugzilla_login'}));
    SendSQL("SELECT cryptpassword FROM profiles WHERE login_name = " .
            SqlQuote($::COOKIE{'Bugzilla_login'}));
    my $encrypted = FetchOneColumn();
    
    SendSQL("update logincookies set cryptpassword = '$encrypted' where cookie = $::COOKIE{'Bugzilla_logincookie'}");
}


my $newemailtech = exists $::FORM{'newemailtech'};

SendSQL("UPDATE profiles " .
        "SET emailnotification='$::FORM{'emailnotification'}', " .
        "    newemailtech = '$newemailtech' " .
        "WHERE login_name = " . SqlQuote($::COOKIE{'Bugzilla_login'}));

my $newrealname = $::FORM{'realname'};

if ($newrealname ne "") {
    $newrealname = SqlQuote($newrealname);
    SendSQL("update profiles set realname=$newrealname where login_name = " .
            SqlQuote($::COOKIE{'Bugzilla_login'}));
}

PutHeader("Preferences updated.");
print "
Your preferences have been updated.
<p>";
PutFooter();

