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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>

use diagnostics;
use strict;

require "CGI.pl";

use RelationSet;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.
sub sillyness {
    my $zz;
    $zz = $::defaultqueryname;
    $zz = $::usergroupset;
}

my $userid;


sub EmitEntry {
    my ($description, $entry) = (@_);
    print qq{<TR><TH ALIGN="right">$description:</TH><TD>$entry</TD></TR>\n};
}

sub Error {
    my ($msg) = (@_);
    print qq{
$msg
<P>
Please hit <B>back</B> and try again.
};
    PutFooter();
    exit();
}


sub ShowAccount {
    SendSQL("SELECT realname FROM profiles WHERE userid = $userid");
    my ($realname) = (FetchSQLData());

    $realname = value_quote($realname);
    
    EmitEntry("Old password",
              qq{<input type=password name="oldpwd">});
    EmitEntry("New password",
              qq{<input type=password name="pwd1">});
    EmitEntry("Re-enter new password", 
              qq{<input type=password name="pwd2">});
    EmitEntry("Your username (optional)",
              qq{<INPUT SIZE=35 NAME="realname" VALUE="$realname">});
}

sub SaveAccount {
    if ($::FORM{'oldpwd'} ne ""
        || $::FORM{'pwd1'} ne "" || $::FORM{'pwd2'} ne "") {
        my $old = SqlQuote($::FORM{'oldpwd'});
        my $pwd1 = SqlQuote($::FORM{'pwd1'});
        my $pwd2 = SqlQuote($::FORM{'pwd2'});
        SendSQL("SELECT cryptpassword = ENCRYPT($old, LEFT(cryptpassword, 2)) " .
                "FROM profiles WHERE userid = $userid");
        if (!FetchOneColumn()) {
            Error("You did not enter your old password correctly.");
        }
        if ($pwd1 ne $pwd2) {
            Error("The two passwords you entered did not match.");
        }
        if ($::FORM{'pwd1'} eq '') {
            Error("You must enter a new password.");
        }
        SendSQL("UPDATE profiles SET password = $pwd1, " .
                "cryptpassword = ENCRYPT($pwd1) " .
                "WHERE userid = $userid");
    }
    SendSQL("UPDATE profiles SET " .
            "realname = " . SqlQuote($::FORM{'realname'}) .
            " WHERE userid = $userid");
}



sub ShowDiffs {
    SendSQL("SELECT emailnotification, newemailtech FROM profiles " .
            "WHERE userid = $userid");
    my ($emailnotification, $newemailtech) = (FetchSQLData());
    my $qacontactpart = "";
    if (Param('useqacontact')) {
        $qacontactpart = ", the current QA Contact";
    }
    print qq{
<TR><TD COLSPAN="2">
Bugzilla will send out email notification of changed bugs to 
the current owner, the submitter of the bug$qacontactpart, and anyone on the
CC list.  However, you can suppress some of those email notifications.
On which of these bugs would you like email notification of changes?
</TD></TR>
};
    my $entry =
        BuildPulldown("emailnotification", 
                      [["ExcludeSelfChanges",
                        "All qualifying bugs except those which I change"],
                       ["CConly",
                        "Only those bugs which I am listed on the CC line"],
                       ["All",
                        "All qualifying bugs"]],
                       $emailnotification);
    EmitEntry("Notify me of changes to", $entry);

    if (Param("newemailtech")) {
        my $checkedpart = $newemailtech ? "CHECKED" : "";
        print qq{
<TR><TD COLSPAN="2"><HR></TD></TR>
<TR><TD COLSPAN="2"><FONT COLOR="red">New!</FONT>
Bugzilla has a new email 
notification scheme.  It is <b>experimental and bleeding edge</b> and will 
hopefully evolve into a brave new happy world where all the spam and ugliness
of the old notifications will go away.  If you wish to sign up for this (and 
risk any bugs), check here.
</TD></TR>
};
        EmitEntry("Check here to sign up (and risk any bugs)",
                  qq{<INPUT TYPE="checkbox" NAME="newemailtech" $checkedpart>New email tech});
    }

    if (Param("supportwatchers")) {
      my $watcheduserSet = new RelationSet;
      $watcheduserSet->mergeFromDB("SELECT watched FROM watch WHERE" .
                                    " watcher=$userid");
      my $watchedusers = $watcheduserSet->toString();

      print qq{
<TR><TD COLSPAN="2"><HR></TD></TR>
<TR><TD COLSPAN="2"><FONT COLOR="red">New!</FONT>
If you want to help cover for someone when they're on vacation, or if 
you need to do the QA related to all of their bugs, you can tell bugzilla
to send mail related to their bugs to you also.  List the email addresses
of any users you wish to watch here, separated by commas.
<FONT COLOR="red">Note that you MUST have the above "New email tech" 
button selected in order to use this feature.</FONT>
</TD></TR>
};
      EmitEntry("Users to watch",
              qq{<INPUT SIZE=35 NAME="watchedusers" VALUE="$watchedusers">});

    }

}

sub SaveDiffs {
    my $newemailtech = 0;
    if (exists $::FORM{'newemailtech'}) {
        $newemailtech = 1;
    }
    SendSQL("UPDATE profiles " .
            "SET emailnotification = " . SqlQuote($::FORM{'emailnotification'})
            . ", newemailtech = $newemailtech WHERE userid = $userid");

    # deal with any watchers
    #
    if (Param("supportwatchers") ) {

        if (exists $::FORM{'watchedusers'}) {

            Error ('You must have "New email tech" set to watch someone')
                if ( $::FORM{'watchedusers'} ne "" && $newemailtech == 0);

            # Just in case.  Note that this much locking is actually overkill:
            # we don't really care if anyone reads the watch table.  So 
            # some small amount of contention could be gotten rid of by
            # using user-defined locks rather than table locking.
            #
            SendSQL("LOCK TABLES watch WRITE, profiles READ");

            # what the db looks like now
            #
            my $origWatchedUsers = new RelationSet;
            $origWatchedUsers->mergeFromDB("SELECT watched FROM watch WHERE" .
                                           " watcher=$userid");

            # update the database to look like the form
            #
            my $newWatchedUsers = new RelationSet($::FORM{'watchedusers'});
            my @CCDELTAS = $origWatchedUsers->generateSqlDeltas(
                                                             $newWatchedUsers, 
                                                             "watch", 
                                                             "watcher", 
                                                             $userid,
                                                             "watched");
            $CCDELTAS[0] eq "" || SendSQL($CCDELTAS[0]);
            $CCDELTAS[1] eq "" || SendSQL($CCDELTAS[1]);

            # all done
            #
            SendSQL("UNLOCK TABLES");
        
        }
    }
}



sub ShowFooter {
    SendSQL("SELECT mybugslink FROM profiles " .
            "WHERE userid = $userid");
    my ($mybugslink) = (FetchSQLData());
    my $entry =
        BuildPulldown("mybugslink",
                      [["1", "should appear"],
                       ["0", "should not be displayed"]],
                      $mybugslink);
    EmitEntry("The 'My bugs' link at the footer of each page", $entry);
    SendSQL("SELECT name, linkinfooter FROM namedqueries " .
            "WHERE userid = $userid");
    my $count = 0;
    while (MoreSQLData()) {
        my ($name, $linkinfooter) = (FetchSQLData());
        if ($name eq $::defaultqueryname) {
            next;
        }
        my $entry =
            BuildPulldown("query-$count",
                          [["0", "should only appear in the query page"],
                           ["1", "should appear on the footer of every page"]],
                          $linkinfooter);
        EmitEntry("Your query named '$name'", $entry);
        my $q = value_quote($name);
        print qq{<INPUT TYPE=HIDDEN NAME="name-$count" VALUE="$q">\n};
        $count++;
    }
    print qq{<INPUT TYPE=HIDDEN NAME="numqueries" VALUE="$count">\n};
    if (!$count) {
        print qq{
<TR><TD COLSPAN="2">
If you go create remembered queries in the <A HREF="query.cgi">query page</A>,
you can then come to this page and choose to have some of them appear in the 
footer of each Bugzilla page.
</TD></TR>};
    }
}
              
    
sub SaveFooter {
    my %old;
    SendSQL("SELECT name, linkinfooter FROM namedqueries " .
            "WHERE userid = $userid");
    while (MoreSQLData()) {
        my ($name, $linkinfooter) = (FetchSQLData());
        $old{$name} = $linkinfooter;
    }
    
    for (my $c=0 ; $c<$::FORM{'numqueries'} ; $c++) {
        my $name = $::FORM{"name-$c"};
        if (exists $old{$name}) {
            my $new = $::FORM{"query-$c"};
            if ($new ne $old{$name}) {
                SendSQL("UPDATE namedqueries SET linkinfooter = $new " .
                        "WHERE userid = $userid " .
                        "AND name = " . SqlQuote($name));
            }
        } else {
            Error("Hmm, the $name query seems to have gone away.");
        }
    }
    SendSQL("UPDATE profiles SET mybugslink = '" . $::FORM{'mybugslink'} .
            "' WHERE userid = $userid");
}
    


sub ShowPermissions {
    print "You have the following permission bits set on your account:\n";
    print "<P><UL>\n";
    my $found = 0;
    SendSQL("SELECT description FROM groups " .
            "WHERE bit & $::usergroupset != 0 " .
            "ORDER BY bit");
    while (MoreSQLData()) {
        my ($description) = (FetchSQLData());
        print "<LI>$description\n";
        $found = 1;
    }
    if ($found == 0) {
        print "<LI>(No extra permission bits have been set).\n";
    }
    print "</UL>\n";
    SendSQL("SELECT blessgroupset FROM profiles WHERE userid = $userid");
    my $blessgroupset = FetchOneColumn();
    if ($blessgroupset) {
        print "And you can turn on or off the following bits for\n";
        print qq{<A HREF="editusers.cgi">other users</A>:\n};
        print "<P><UL>\n";
        SendSQL("SELECT description FROM groups " .
                "WHERE bit & $blessgroupset != 0 " .
                "ORDER BY bit");
        while (MoreSQLData()) {
            my ($description) = (FetchSQLData());
            print "<LI>$description\n";
        }
        print "</UL>\n";
    }
}
        



######################################################################
################# Live code (not sub defs) starts here ###############


confirm_login();

print "Content-type: text/html\n\n";

GetVersionTable();

PutHeader("Preferences", "Preferences", $::COOKIE{'Bugzilla_login'});

#  foreach my $k (sort(keys(%::FORM))) {
#      print "<pre>" . value_quote($k) . ": " . value_quote($::FORM{$k}) . "\n</pre>";
#  }

my $bank = $::FORM{'bank'} || "account";

my @banklist = (
                ["account", "Account settings",
                 \&ShowAccount, \&SaveAccount],
                ["diffs", "Email settings",
                 \&ShowDiffs, \&SaveDiffs],
                ["footer", "Page footer",
                 \&ShowFooter, \&SaveFooter],
                ["permissions", "Permissions",
                 \&ShowPermissions, undef]
                );


my $numbanks = @banklist;
my $numcols = $numbanks + 2;

my $headcol = '"lightblue"';

print qq{
<CENTER>
<TABLE CELLSPACING="0" CELLPADDING="10" BORDER=0 WIDTH="100%">
<TR>
<TH COLSPAN="$numcols" BGCOLOR="lightblue">User preferences</TH>
</TR>
<TR><TD BGCOLOR=$headcol>&nbsp;</TD>
};


my $bankdescription;
my $showfunc;
my $savefunc;

foreach my $i (@banklist) {
    my ($name, $description) = (@$i);
    my $color = "";
    if ($name eq $bank) {
        print qq{<TD ALIGN="center">$description</TD>};
        my $zz;
        ($zz, $bankdescription, $showfunc, $savefunc) = (@$i);
    } else {
        print qq{<TD ALIGN="center" BGCOLOR="lightblue"><A HREF="userprefs.cgi?bank=$name">$description</A></TD>};
    }
}
print qq{
<TD BGCOLOR=$headcol>&nbsp;</TD></TR>
</TABLE>
</CENTER>
<P>
};




if (defined $bankdescription) {
    $userid = DBNameToIdAndCheck($::COOKIE{'Bugzilla_login'});

    if ($::FORM{'dosave'}) {
        &$savefunc;
        print "Your changes have been saved.";
    }
    print qq{
<H3>$bankdescription</H3>
<FORM METHOD="POST">
<TABLE>
};
    &$showfunc;
    print qq{
</TABLE>
<INPUT TYPE="hidden" NAME="dosave" VALUE="1">
<INPUT TYPE="hidden" NAME="bank" VALUE="$bank">
};
    if ($savefunc) {
        print qq{<INPUT TYPE="submit" VALUE="Submit">\n};
    }
    print qq{</FORM>\n};
} else {
    print "<P>Please choose from the above links which settings you wish to change.</P>";
}


print "<P>";


PutFooter();
