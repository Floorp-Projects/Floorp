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
#                 Dawn Endico <endico@mozilla.org>


# This file defines all the parameters that we have a GUI to edit within
# Bugzilla.

use diagnostics;
use strict;


sub WriteParams {
    foreach my $i (@::param_list) {
	if (!defined $::param{$i}) {
	    $::param{$i} = $::param_default{$i};
            if (!defined $::param{$i}) {
                die "No default parameter ever specified for $i";
            }
	}
    }
    mkdir("data", 0777);
    chmod 0777, "data";
    my $tmpname = "data/params.$$";
    open(FID, ">$tmpname") || die "Can't create $tmpname";
    my $v = $::param{'version'};
    delete $::param{'version'};  # Don't write the version number out to
                                # the params file.
    print FID GenerateCode('%::param');
    $::param{'version'} = $v;
    print FID "1;\n";
    close FID;
    rename $tmpname, "data/params" || die "Can't rename $tmpname to data/params";
    chmod 0666, "data/params";
}
    

sub DefParam {
    my ($id, $desc, $type, $default, $checker) = (@_);
    push @::param_list, $id;
    $::param_desc{$id} = $desc;
    $::param_type{$id} = $type;
    $::param_default{$id} = $default;
    if (defined $checker) {
	$::param_checker{$id} = $checker;
    }
}


sub check_numeric {
    my ($value) = (@_);
    if ($value !~ /^[0-9]+$/) {
	return "must be a numeric value";
    }
    return "";
}
    


@::param_list = ();



# OK, here are the definitions themselves.
#
# The type of parameters (the third parameter to DefParam) can be one
# of the following:
#
# t -- A short text entry field (suitable for a single line)
# l -- A long text field (suitable for many lines)
# b -- A boolean value (either 1 or 0)
# i -- An integer.
# defenum -- This param defines an enum that defines a column in one of
#	     the database tables.  The name of the parameter is of the form
#	     "tablename.columnname".

DefParam("maintainer",
	 "The email address of the person who maintains this installation of Bugzilla.",
	 "t",
         'THE MAINTAINER HAS NOT YET BEEN SET');

DefParam("urlbase",
	 "The URL that is the common initial leading part of all Bugzilla URLs.",
	 "t",
	 "http://cvs-mirror.mozilla.org/webtools/bugzilla/",
	 \&check_urlbase);

sub check_urlbase {
    my ($url) = (@_);
    if ($url !~ m:^http.*/$:) {
	return "must be a legal URL, that starts with http and ends with a slash.";
    }
    return "";
}


DefParam("usedespot",
         "If this is on, then we are using the Despot system to control our database of users.  Bugzilla won't ever write into the user database, it will let the Despot code maintain that.  And Bugzilla will send the user over to Despot URLs if they need to change their password.  Also, in that case, Bugzilla will treat the passwords stored in the database as being crypt'd, not plaintext.",
         "b",
         0);

DefParam("despotbaseurl",
         "The base URL for despot.  Used only if <b>usedespot</b> is turned on, above.",
         "t",
         "http://cvs-mirror.mozilla.org/webtools/despot/despot.cgi",
         \&check_despotbaseurl);


sub check_despotbaseurl {
    my ($url) = (@_);
    if ($url !~ /^http.*cgi$/) {
	return "must be a legal URL, that starts with http and ends with .cgi";
    }
    return "";
}


DefParam("headerhtml",
         "Additional HTML to add to the HEAD area of documents, eg. links to stylesheets.",
         "l",
         '');


DefParam("bannerhtml",
         "The html that gets emitted at the head of every Bugzilla page. 
Anything of the form %<i>word</i>% gets replaced by the defintion of that 
word (as defined on this page).",
         "l",
         q{<TABLE BGCOLOR="#000000" WIDTH="100%" BORDER=0 CELLPADDING=0 CELLSPACING=0>
<TR><TD><A HREF="http://www.mozilla.org/"><IMG
 SRC="http://www.mozilla.org/images/mozilla-banner.gif" ALT=""
BORDER=0 WIDTH=600 HEIGHT=58></A></TD></TR></TABLE>
<CENTER><FONT SIZE=-1>Bugzilla version %version%
</FONT></CENTER>});

DefParam("blurbhtml",
         "A blurb that appears as part of the header of every Bugzilla page.  This is a place to put brief warnings, pointers to one or two related pages, etc.",
         "l",
         "This is <B>Bugzilla</B>: the Mozilla bug system.  For more 
information about what Bugzilla is and what it can do, see 
<A HREF=\"http://www.mozilla.org/\">mozilla.org</A>'s
<A HREF=\"http://www.mozilla.org/bugs/\"><B>bug pages</B></A>.");



    
DefParam("changedmail",
q{The email that gets sent to people when a bug changes.  Within this
text, %to% gets replaced by the assigned-to and reported-by people,
separated by a comma (with duplication removed, if they're the same
person).  %cc% gets replaced by the list of people on the CC list,
separated by commas.  %bugid% gets replaced by the bug number.
%diffs% gets replaced by the diff text from the old version to the new
version of this bug.  %neworchanged% is either "New" or "Changed",
depending on whether this mail is reporting a new bug or changes made
to an existing one.  %summary% gets replaced by the summary of this
bug.  %<i>anythingelse</i>% gets replaced by the definition of that
parameter (as defined on this page).},
         "l",
"From: bugzilla-daemon
To: %to%
Cc: %cc%
Subject: [Bug %bugid%] %neworchanged% - %summary%

%urlbase%show_bug.cgi?id=%bugid%

%diffs%");



DefParam("whinedays",
         "The number of days that we'll let a bug sit untouched in a NEW state before our cronjob will whine at the owner.",
         "t",
         7,
         \&check_numeric);


DefParam("whinemail",
         "The email that gets sent to anyone who has a NEW bug that hasn't been touched for more than <b>whinedays</b>.  Within this text, %email% gets replaced by the offender's email address.  %<i>anythingelse</i>% gets replaced by the definition of that parameter (as defined on this page).<p> It is a good idea to make sure this message has a valid From: address, so that if the mail bounces, a real person can know that there are bugs assigned to an invalid address.",
         "l",
         q{From: %maintainer%
To: %email%
Subject: Your Bugzilla buglist needs attention.

[This e-mail has been automatically generated.]

You have one or more bugs assigned to you in the Bugzilla 
bugsystem (%urlbase%) that require
attention.

All of these bugs are in the NEW state, and have not been touched
in %whinedays% days or more.  You need to take a look at them, and 
decide on an initial action.

Generally, this means one of three things:

(1) You decide this bug is really quick to deal with (like, it's INVALID),
    and so you get rid of it immediately.
(2) You decide the bug doesn't belong to you, and you reassign it to someone
    else.  (Hint: if you don't know who to reassign it to, make sure that
    the Component field seems reasonable, and then use the "Reassign bug to
    owner of selected component" option.)
(3) You decide the bug belongs to you, but you can't solve it this moment.
    Just use the "Accept bug" command.

To get a list of all NEW bugs, you can use this URL (bookmark it if you like!):

    %urlbase%buglist.cgi?bug_status=NEW&assigned_to=%email%

Or, you can use the general query page, at
%urlbase%query.cgi.

Appended below are the individual URLs to get to all of your NEW bugs that 
haven't been touched for a week or more.

You will get this message once a day until you've dealt with these bugs!

});



DefParam("defaultquery",
 	 "This is the default query that initially comes up when you submit a bug.  It's in URL parameter format, which makes it hard to read.  Sorry!",
	 "t",
	 "bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&order=%22Importance%22");


DefParam("letsubmitterchoosepriority",
         "If this is on, then people submitting bugs can choose an initial priority for that bug.  If off, then all bugs initially have the default priority selected above.",
         "b",
         1);


sub check_priority {
    my ($value) = (@_);
    GetVersionTable();
    if (lsearch(\@::legal_priority, $value) < 0) {
        return "Must be a legal priority value: one of " .
            join(", ", @::legal_priority);
    }
    return "";
}

DefParam("defaultpriority",
         "This is the priority that newly entered bugs are set to.",
         "t",
         "P2",
         \&check_priority);


DefParam("usetargetmilestone",
	 "Do you wish to use the Target Milestone field?",
	 "b",
	 0);

DefParam("nummilestones",
         "If using Target Milestone, how many milestones do you wish to
          appear?",
         "t",
         10,
         \&check_numeric);

DefParam("curmilestone",
         "If using Target Milestone, Which milestone are we working toward right now?",
         "t",
         1,
         \&check_numeric);

DefParam("useqacontact",
	 "Do you wish to use the QA Contact field?",
	 "b",
	 0);

DefParam("usestatuswhiteboard",
	 "Do you wish to use the Status Whiteboard field?",
	 "b",
	 0);

DefParam("usebrowserinfo",
	 "Do you want bug reports to be assigned an OS & Platform based on the browser
	  the user makes the report from?",
	 "b",
	 1);

DefParam("usedependencies",
         "Do you wish to use dependencies (allowing you to mark which bugs depend on which other ones)?",
         "b",
         1);

DefParam("webdotbase",
         "This is the URL prefix that is common to all requests for webdot.  The <a href=\"http://www.research.att.com/~north/cgi-bin/webdot.cgi\">webdot package</a> is a very swell thing that generates pictures of graphs.  If you have an installation of bugsplat that hides behind a firewall, then to get graphs to work, you will have to install a copy of webdot behind your firewall, and change this path to match.  Also, webdot has some trouble with software domain names, so you may have to play games and hack the %urlbase% part of this.  If this all seems like too much trouble, you can set this paramater to be the empty string, which will cause the graphing feature to be disabled entirely.",
         "t",
         "http://www.research.att.com/~north/cgi-bin/webdot.cgi/%urlbase%");

DefParam("entryheaderhtml",
         "This is a special header for the bug entry page. The text will be printed after the page header, before the bug entry form. It is meant to be a place to put pointers to intructions on how to enter bugs.",
         "l",
         '');

DefParam("expectbigqueries",
         "If this is on, then we will tell mysql to <tt>set option SQL_BIG_TABLES=1</tt> before doing queries on bugs.  This will be a little slower, but one will not get the error <tt>The table ### is full</tt> for big queries that require a big temporary table.",
         "b",
         0);

DefParam("emailregexp",
         'This defines the regexp to use for legal email addresses.  The default tries to match fully qualified email addresses.  Another popular value to put here is <tt>^[^@, ]$</tt>, which means "local usernames, no @ allowed.',
         "t",
         q:^[^@, ]*@[^@, ]*\\.[^@, ]*$:);

DefParam("emailregexpdesc",
         "This describes in english words what kinds of legal addresses are allowed by the <tt>emailregexp</tt> param.",
         "l",
         "A legal address must contain exactly one '\@', and at least one '.' after the \@, and may not contain any commas or spaces.");

DefParam("emailsuffix",
         "This is a string to append to any email addresses when actually sending mail to that address.  It is useful if you have changed the <tt>emailregexp</tt> param to only allow local usernames, but you want the mail to be delivered to username\@my.local.hostname.",
         "t",
         "");


1;

