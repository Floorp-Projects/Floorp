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

require "CGI.pl";

confirm_login();

print "Content-type: text/html\n\n";

if (! defined $::FORM{'pwd1'}) {
    PutHeader("Preferences", "Change your password and<br>other preferences",
              $::COOKIE{'Bugzilla_login'});

    my $qacontactpart = "";
    if (Param('useqacontact')) {
        $qacontactpart = ", the current QA Contact";
    }
    my $loginname = SqlQuote($::COOKIE{'Bugzilla_login'});
    SendSQL("select emailnotification,realname from profiles where login_name = " .
            $loginname);
    my ($emailnotification, $realname) = (FetchSQLData());
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
</table>
<hr>
<input type=submit value=Submit>
</form>
<hr>
";
    navigation_header();
    exit;
}

if ($::FORM{'pwd1'} ne $::FORM{'pwd2'}) {
    print "<H1>Try again.</H1>
The two passwords you entered did not match.  Please click <b>Back</b> and try again.\n";
    exit;
}


my $pwd = $::FORM{'pwd1'};


sub x {
    my $sc="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789./";
    return substr($sc, int (rand () * 100000) % (length ($sc) + 1), 1);
}

if ($pwd ne "") {
    if ($pwd !~ /^[a-zA-Z0-9-_]*$/ || length($pwd) < 3 || length($pwd) > 15) {
        print "<H1>Sorry; we're picky.</H1>
Please choose a password that is between 3 and 15 characters long, and that
contains only numbers, letters, hyphens, or underlines.
<p>
Please click <b>Back</b> and try again.\n";
        exit;
    }
    
    
# Generate a random salt.
    
    my $salt  = x() . x();
    
    my $encrypted = crypt($pwd, $salt);
    
    SendSQL("update profiles set password='$pwd',cryptpassword='$encrypted' where login_name=" .
            SqlQuote($::COOKIE{'Bugzilla_login'}));
    
    SendSQL("update logincookies set cryptpassword = '$encrypted' where cookie = $::COOKIE{'Bugzilla_logincookie'}");
}


SendSQL("update profiles set emailnotification='$::FORM{'emailnotification'}' where login_name = " .
        SqlQuote($::COOKIE{'Bugzilla_login'}));

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
navigation_header();

