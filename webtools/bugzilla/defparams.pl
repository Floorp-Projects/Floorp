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
#                 Dawn Endico <endico@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Jake <jake@acutex.net>
#                 J. Paul Reed <preed@sigkill.com>
#

# This file defines all the parameters that we have a GUI to edit within
# Bugzilla.

# ATTENTION!!!!   THIS FILE ONLY CONTAINS THE DEFAULTS.
# You cannot change your live settings by editing this file.
# Only adding new parameters is done here.  Once the parameter exists, you 
# must use %baseurl%/editparams.cgi from the web to edit the settings.

use diagnostics;
use strict;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub defparams_pl_sillyness {
    my $zz;
    $zz = %::param_checker;
    $zz = %::param_desc;
    $zz = %::param_type;
    $zz = %::MFORM;
}

sub WriteParams {
    foreach my $i (@::param_list) {
        if (!defined $::param{$i}) {
            if ($::param_type{$i} eq "m") {
                ## For list params (single or multi), param_default is an array
                ## with the second element as the default; we have to loop
                ## through it to get them all for multi lists and we have to
                ## select the second one by itself for single list (next branch
                ## of the if)
                my $multiParamStr = "[ ";
                foreach my $defaultParam (@{$::param_default{$i}->[1]}) {
                    $multiParamStr .= "'$defaultParam', "; 
                }

                $multiParamStr .= " ]";

                $::param{$i} = $multiParamStr;
            }
            elsif ($::param_type{$i} eq "s") {
                $::param{$i} = $::param_default{$i}->[1];
            }
            else {
                $::param{$i} = $::param_default{$i};
            }

            if (!defined $::param{$i}) {
                die "No default parameter ever specified for $i";
            }
        }
    }

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
    ChmodDataFile('data/params', 0666);
}

## $checker is a CODE ref that points to a function that verifies the
## parameter for validity; it is called by doeditparams.cgi with the value
## of the param as the first arg and the param name as the 2nd arg (which
## many checker functions ignore, but a couple of them need it.

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

## Converts text parameters for single- and multi-select type params to their
## array indices; takes the name of the parameter and the value you want the
## index of; returns undef on failure.

sub get_select_param_index {
    my ($paramName, $val) = (@_);

    return undef if ($::param_type{$paramName} !~ /^m|s$/);

    my $paramList = $::param_default{$paramName}->[0];

    for (my $ndx = 0; $ndx < scalar(@{$paramList}); $ndx++) {
        ## The first element of the $param_default array in selects is the
        ## list of possible params; search through this array for a match.
        return $ndx if ($val eq $paramList->[$ndx]);
    }

    return undef;
}

sub check_multi {
    my ($value, $param) = (@_);

    if ($::param_type{$param} eq "s") {
        if (check_numeric($value) ne "") {
            return "List param types must be digits";
        }
        elsif ($value < 0 || $value > $#{$::param_default{$param}->[0]}) {
            return "Invalid choice for single-select list param '$param'";
        }
        else {
            return "";
        }
    }
    elsif ($::param_type{$param} eq "m") {
        foreach my $chkParam (@{$::MFORM{$param}}) {
            if (check_numeric($chkParam) ne "") {
                return "List param types must be digits";
            }
            elsif ($chkParam < 0 || $chkParam > 
             $#{$::param_default{$param}->[0]}) {
                return "Invalid choice for multi-select list param '$param'";
            }
        }

        return "";
    }
    else {
        return "Invalid param type for check_multi(); contact your BZ admin";
    }
}

sub check_numeric {
    my ($value) = (@_);
    if ($value !~ /^[0-9]+$/) {
        return "must be a numeric value";
    }
    return "";
}
    
sub check_shadowdb {
    my ($value) = (@_);
    $value = trim($value);
    if ($value eq "") {
        return "";
    }
    SendSQL("SHOW DATABASES");
    while (MoreSQLData()) {
        my $n = FetchOneColumn();
        if (lc($n) eq lc($value)) {
            return "The $n database already exists.  If that's really the name you want to use for the backup, please CAREFULLY make the existing database go away somehow, and then try again.";
        }
    }
    SendSQL("CREATE DATABASE $value");
    SendSQL("INSERT INTO shadowlog (command) VALUES ('SYNCUP')", 1);
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
# m -- A list of values, with many selectable (shows up as a select box)
#      To specify the list of values, make the 'default value' for DefParam()
#      a reference to two anonymous arrays, the first being the list of options
#      and the second being a list of defaults (which must appear in the 
#      first anonymous array), i.e.:
#       DefParam("multiselect", "A list of options, choose many", 
#                "m", [ ['a','b','c','d'], ['a', 'd'] ], \&check_multi);
#
#      Here, 'a' and 'd' are the default options, and the user may pick any
#      combination of a, b, c, and d as valid options.
#
#      &check_multi should always be used as the param verification function
#      for list (single and multiple) parameter types.
#
# s -- A list of values, with one selectable (shows up as a select box)
#      To specify the list of values, make the default value a reference to
#      an anonymous array with two items inside of it, the first being an 
#      array of the possible values and the second being the scalar that is
#      the default default value, i.e.:
#       DefParam("singleselect", "A list of options, choose one", "s", 
#               [ ['a','b','c'], 'b'], \&check_multi);
#      
#      Here, 'b' is the default option, and 'a' and 'c' are other possible
#      options, but only one at a time! 
#
#      &check_multi should always be used as the param verification function
#      for list (single and multiple) parameter types.

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

DefParam("cookiepath", 
  "Directory path under your document root that holds your Bugzilla installation. Make sure to begin with a /.",
  "t",
  "/");

DefParam("enablequips",
        "If this is on, Bugzilla displays a silly quip at the beginning of buglists, and lets users add to the list of quips. If this is frozen, Bugzilla will display the quip but not permit new additions.",
        "s",
        [['on','frozen','off'], (($::param{"usequip"} || 1) ? 'on' : 'off')],
        \&check_multi);


# Added parameter - JMR, 2/16/00
DefParam("usebuggroups",
         "If this is on, Bugzilla will associate a bug group with each product in the database, and use it for querying bugs.",
         "b",
         0); 

# Added parameter - JMR, 2/16/00
DefParam("usebuggroupsentry",
         "If this is on, Bugzilla will use product bug groups to restrict who can enter bugs.  Requires usebuggroups to be on as well.",
         "b",
         0); 

DefParam("shadowdb",
         "If non-empty, then this is the name of another database in which Bugzilla will keep a shadow read-only copy of everything.  This is done so that long slow read-only operations can be used against this db, and not lock up things for everyone else.  Turning on this parameter will create the given database; be careful not to use the name of an existing database with useful data in it!",
         "t",
         "",
         \&check_shadowdb);

DefParam("queryagainstshadowdb",
         "If this is on, and the shadowdb is set, then queries will happen against the shadow database.",
         "b",
         0);
         

# Adding in four parameters for LDAP authentication.  -JMR, 7/28/00
DefParam("useLDAP",
         "Turn this on to use an LDAP directory for user authentication ".
         "instead of the Bugzilla database.  (User profiles will still be ".
         "stored in the database, and will match against the LDAP user by ".
         "email address.)",
         "b",
         0);


DefParam("LDAPserver",
         "The name (and optionally port) of your LDAP server. (e.g. ldap.company.com, or ldap.company.com:portnum)",
         "t",
         "");


DefParam("LDAPBaseDN",
         "The BaseDN for authenticating users against. (e.g. \"ou=People,o=Company\")",
         "t",
         "");


DefParam("LDAPmailattribute",
         "The name of the attribute of a user in your directory that ".
         "contains the email address.",
         "t",
         "mail");
#End of LDAP parameters


DefParam("mostfreqthreshold",
         "The minimum number of duplicates a bug needs to show up on the <a href=\"duplicates.cgi\">most frequently reported bugs page</a>. If you have a large database and this page takes a long time to load, try increasing this number.",
         "t",
         "2");


DefParam("mybugstemplate",
         "This is the URL to use to bring up a simple 'all of my bugs' list for a user.  %userid% will get replaced with the login name of a user.",
         "t",
         "buglist.cgi?bug_status=NEW&amp;bug_status=ASSIGNED&amp;bug_status=REOPENED&amp;email1=%userid%&amp;emailtype1=exact&amp;emailassigned_to1=1&amp;emailreporter1=1");
    

DefParam("shutdownhtml",
         "If this field is non-empty, then Bugzilla will be completely disabled and this text will be displayed instead of all the Bugzilla pages.",
         "l",
         "");

DefParam("sendmailnow",
         "If this is on, Bugzilla will tell sendmail to send any e-mail immediately. If you have a large number of users with a large amount of e-mail traffic, enabling this option may dramatically slow down Bugzilla. Best used for smaller installations of Bugzilla.",
         "b",
         0);

DefParam("passwordmail",
q{The email that gets sent to people to tell them their password.  Within
this text, %mailaddress% gets replaced by the person's email address,
%login% gets replaced by the person's login (usually the same thing), and
%password% gets replaced by their password.  %<i>anythingelse</i>% gets
replaced by the definition of that parameter (as defined on this page).},
         "l",
         q{From: bugzilla-daemon
To: %mailaddress%
Subject: Your Bugzilla password.

To use the wonders of Bugzilla, you can use the following:

 E-mail address: %login%
       Password: %password%

 To change your password, go to:
 %urlbase%userprefs.cgi
});


DefParam("newchangedmail",
q{The email that gets sent to people when a bug changes.  Within this
text, %to% gets replaced with the e-mail address of the person recieving
the mail.  %bugid% gets replaced by the bug number.  %diffs% gets
replaced with what's changed.  %neworchanged% is "New:" if this mail is
reporting a new bug or empty if changes were made to an existing one.
%summary% gets replaced by the summary of this bug. %reasonsheader% 
is replaced by an abbreviated list of reasons why the user is getting the email,
suitable for use in an email header (such as X-Bugzilla-Reason).
%reasonsbody% is replaced by text that explains why the user is getting the email 
in more user friendly text than %reasonsheader%.
%<i>anythingelse</i>% gets replaced by the definition of 
that parameter (as defined on this
page).},
         "l",
"From: bugzilla-daemon
To: %to%
Subject: [Bug %bugid%] %neworchanged%%summary%
X-Bugzilla-Reason: %reasonsheader%

%urlbase%show_bug.cgi?id=%bugid%

%diffs%



%reasonsbody%");



DefParam("whinedays",
         "The number of days that we'll let a bug sit untouched in a NEW state before our cronjob will whine at the owner.",
         "t",
         7,
         \&check_numeric);


DefParam("whinemail",
         "The email that gets sent to anyone who has a NEW bug that hasn't been touched for more than <b>whinedays</b>.  Within this text, %email% gets replaced by the offender's email address.  %userid% gets replaced by the offender's bugzilla login (which, in most installations, is the same as the email address.)  %<i>anythingelse</i>% gets replaced by the definition of that parameter (as defined on this page).<p> It is a good idea to make sure this message has a valid From: address, so that if the mail bounces, a real person can know that there are bugs assigned to an invalid address.",
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

    %urlbase%buglist.cgi?bug_status=NEW&assigned_to=%userid%

Or, you can use the general query page, at
%urlbase%query.cgi.

Appended below are the individual URLs to get to all of your NEW bugs that 
haven't been touched for a week or more.

You will get this message once a day until you've dealt with these bugs!

});



DefParam("defaultquery",
          "This is the default query that initially comes up when you submit a bug.  It's in URL parameter format, which makes it hard to read.  Sorry!",
         "t",
         "bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&emailassigned_to1=1&emailassigned_to2=1&emailreporter2=1&emailcc2=1&emailqa_contact2=1&order=%22Importance%22");


DefParam("letsubmitterchoosepriority",
         "If this is on, then people submitting bugs can choose an initial priority for that bug.  If off, then all bugs initially have the default priority selected below.",
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

DefParam("musthavemilestoneonaccept",
         "If you are using Target Milestone, do you want to require that the milestone be set in order for a user to ACCEPT a bug?",
         "b",
         0);

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

DefParam("usevotes",
         "Do you wish to allow users to vote for bugs? Note that in order for this to be effective, you will have to change the maximum votes allowed in a product to be non-zero in <a href=\"editproducts.cgi\">the product edit page</a>.",
         "b",
         1);

DefParam("usebugaliases",
         "Do you wish to use bug aliases, which allow you to assign bugs 
          an easy-to-remember name by which you can refer to them?",
         "b",
         0);

DefParam("webdotbase",
         "It is possible to show graphs of dependent bugs. You may set this parameter to
any of the following:
<ul>
<li>A complete file path to \'dot\' (part of <a
href=\"http://www.graphviz.org\">GraphViz</a>) will generate the graphs
locally.</li>
<li>A URL prefix pointing to an installation of the <a
href=\"http://www.research.att.com/~north/cgi-bin/webdot.cgi\">webdot
package</a> will generate the graphs remotely.</li>
<li>A blank value will disable dependency graphing.</li>
</ul>
The default value is a publically-accessible webdot server. If you change
this value, make certain that the webdot server can read files from your
data/webdot directory. On Apache you do this by editing the .htaccess file,
for other systems the needed measures may vary. You can run checksetup.pl
to recreate the .htaccess file if it has been lost.",
         "t",
         "http://www.research.att.com/~north/cgi-bin/webdot.cgi/%urlbase%",
         \&check_webdotbase);

sub check_webdotbase {
    my ($value) = (@_);
    $value = trim($value);
    if ($value eq "") {
        return "";
    }
    if($value !~ /^https?:/) {
        if(! -x $value) {
            return "The file path \"$value\" is not a valid executable.  Please specify the complete file path to 'dot' if you intend to generate graphs locally.";
        }
        # Check .htaccess allows access to generated images
        if(-e "data/webdot/.htaccess") {
            open HTACCESS, "data/webdot/.htaccess";
            if(! grep(/png/,<HTACCESS>)) {
                return "Dependency graph images are not accessible.\nDelete data/webdot/.htaccess and re-run checksetup.pl to rectify.\n";
            }
            close HTACCESS;
        }
    }
    return "";
}

sub checkregexp {
    my ($value) = (@_);
    eval { qr/$value/ };
    return $@;
}

DefParam("emailregexp",
         'This defines the regexp to use for legal email addresses.  The default tries to match fully qualified email addresses.  Another popular value to put here is <tt>^[^@]+$</tt>, which means "local usernames, no @ allowed."',
         "t",
         q:^[^@]+@[^@]+\\.[^@]+$:,
         \&checkregexp);

DefParam("emailregexpdesc",
         "This describes in english words what kinds of legal addresses are allowed by the <tt>emailregexp</tt> param.",
         "l",
         "A legal address must contain exactly one '\@', and at least one '.' after the \@.");

DefParam("emailsuffix",
         "This is a string to append to any email addresses when actually sending mail to that address.  It is useful if you have changed the <tt>emailregexp</tt> param to only allow local usernames, but you want the mail to be delivered to username\@my.local.hostname.",
         "t",
         "");


DefParam("voteremovedmail",
q{This is a mail message to send to anyone who gets a vote removed from a bug for any reason.  %to% gets replaced by the person who used to be voting for this bug.  %bugid% gets replaced by the bug number.  %reason% gets replaced by a short reason describing why the vote(s) were removed.  %votesremoved%, %votesold% and %votesnew% is the number of votes removed, before and after respectively.  %votesremovedtext%, %votesoldtext% and %votesnewtext% are these as sentences, eg "You had 2 votes on this bug."  %count% is also supported for backwards compatibility.  %<i>anythingelse</i>% gets replaced by the definition of that parameter (as defined on this page).},
         "l",
"From: bugzilla-daemon
To: %to%
Subject: [Bug %bugid%] Some or all of your votes have been removed.

Some or all of your votes have been removed from bug %bugid%.

%votesoldtext%

%votesnewtext%

Reason: %reason%

%urlbase%show_bug.cgi?id=%bugid%
");
         
DefParam("allowbugdeletion",
         q{The pages to edit products and components and versions can delete all associated bugs when you delete a product (or component or version).  Since that is a pretty scary idea, you have to turn on this option before any such deletions will ever happen.},
         "b",
         0);


DefParam("allowemailchange",
         q{Users can change their own email address through the preferences.  Note that the change is validated by emailing both addresses, so switching this option on will not let users use an invalid address.},
         "b",
         0);


DefParam("allowuserdeletion",
         q{The pages to edit users can also let you delete a user.  But there is no code that goes and cleans up any references to that user in other tables, so such deletions are kinda scary.  So, you have to turn on this option before any such deletions will ever happen.},
         "b",
         0);

DefParam("browserbugmessage",
         "If bugzilla gets unexpected data from the browser, in addition to displaying the cause of the problem, it will output this HTML as well.",
         "l",
         "this may indicate a bug in your browser.\n");

#
# Parameters to force users to comment their changes for different actions.
DefParam("commentonaccept", 
         "If this option is on, the user needs to enter a short comment if he accepts the bug",
         "b", 0 );
DefParam("commentonclearresolution", 
         "If this option is on, the user needs to enter a short comment if the bugs resolution is cleared",
         "b", 0 );
DefParam("commentonconfirm", 
         "If this option is on, the user needs to enter a short comment when confirming a bug",
         "b", 0 );
DefParam("commentonresolve", 
         "If this option is on, the user needs to enter a short comment if the bug is resolved",
         "b", 0 );
DefParam("commentonreassign", 
         "If this option is on, the user needs to enter a short comment if the bug is reassigned",
         "b", 0 );
DefParam("commentonreassignbycomponent", 
         "If this option is on, the user needs to enter a short comment if the bug is reassigned by component",
         "b", 0 );
DefParam("commentonreopen", 
         "If this option is on, the user needs to enter a short comment if the bug is reopened",
         "b", 0 );
DefParam("commentonverify", 
         "If this option is on, the user needs to enter a short comment if the bug is verified",
         "b", 0 );
DefParam("commentonclose", 
         "If this option is on, the user needs to enter a short comment if the bug is closed",
         "b", 0 );
DefParam("commentonduplicate",
         "If this option is on, the user needs to enter a short comment if the bug is marked as duplicate",
         "b", 0 );
DefParam("supportwatchers",
         "Support one user watching (ie getting copies of all related email" .
         " about) another's bugs.  Useful for people going on vacation, and" .
         " QA folks watching particular developers' bugs",
         "b", 0 );


DefParam("move-enabled",
         "If this is on, Bugzilla will allow certain people to move bugs to the defined database.",
         "b",
         0);
DefParam("move-button-text",
         "The text written on the Move button. Explain where the bug is being moved to.",
         "t",
         'Move To Bugscape');
DefParam("move-to-url",
         "The URL of the database we allow some of our bugs to be moved to.",
         "t",
         '');
DefParam("move-to-address",
         "To move bugs, an email is sent to the target database. This is the email address that database
          uses to listen for incoming bugs.",
         "t",
         'bugzilla-import');
DefParam("moved-from-address",
         "To move bugs, an email is sent to the target database. This is the email address from which
          this mail, and error messages are sent.",
         "t",
         'bugzilla-admin');
DefParam("movers",
         "A list of people with permission to move bugs and reopen moved bugs (in case the move operation fails).",
         "t",
         '');
DefParam("moved-default-product",
         "Bugs moved from other databases to here are assigned to this product.",
         "t",
         '');
DefParam("moved-default-component",
         "Bugs moved from other databases to here are assigned to this component.",
         "t",
         '');

# The maximum size (in bytes) for patches and non-patch attachments.
# The default limit is 1000KB, which is 24KB less than mysql's default
# maximum packet size (which determines how much data can be sent in a
# single mysql packet and thus how much data can be inserted into the
# database) to provide breathing space for the data in other fields of
# the attachment record as well as any mysql packet overhead (I don't
# know of any, but I suspect there may be some.)

DefParam("maxpatchsize",
         "The maximum size (in kilobytes) of patches.  Bugzilla will not 
          accept patches greater than this number of kilobytes in size.
          To accept patches of any size (subject to the limitations of 
          your server software), set this value to zero." ,
         "t",
         '1000');

DefParam("maxattachmentsize" , 
         "The maximum size (in kilobytes) of non-patch attachments.  Bugzilla 
          will not accept attachments greater than this number of kilobytes 
          in size.  To accept attachments of any size (subject to the
          limitations of your server software), set this value to zero." , 
         "t" , 
         '1000');

DefParam("insidergroup",
         "The name of the group of users who can see/change private comments
          and attachments.",
         "t",
         '');
1;

