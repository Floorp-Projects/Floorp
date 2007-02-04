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
# The Original Code is the Bonsai Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>


# This file defines all the parameters that we have a GUI to edit within
# Bonsai.

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
    open(PARAM_FID, ">$tmpname") || die "Can't create $tmpname";
    my $v = $::param{'version'};
    delete $::param{'version'};  # Don't write the version number out to
                                # the params file.
    print PARAM_FID GenerateCode('%::param');
    $::param{'version'} = $v;
    print PARAM_FID "1;\n";
    close PARAM_FID;
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
    

sub check_urlbase {
    my ($url) = (@_);
    if ($url !~ m:^(http|/).*/$:) {
	return "must be a legal URL, that starts with either 'http' or a slash, and ends with a slash.";
    }
    return "";
}


sub check_registryurl {
    my ($url) = (@_);
    if ($url !~ m:/$:) {
	return "must be a legal URL ending with a slash.";
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
# p -- A password text entry field
# l -- A long text field (suitable for many lines)
# b -- A boolean value (either 1 or 0)
# i -- An integer.
# defenum -- This param defines an enum that defines a column in one of
#	     the database tables.  The name of the parameter is of the form
#	     "tablename.columnname".

DefParam("maintainer",
	 "The email address of the person who maintains this installation of Bonsai.",
	 "t",
         'THE MAINTAINER HAS NOT YET BEEN SET');

DefParam("userdomain",
         "The default domain of the people who don't have an \@ in their email address.",
         "t",
         "");

DefParam("urlbase",
	 "The URL that is the common initial leading part of all Bonsai URLs.",
	 "t",
	 "http://www.mozilla.org/webtools/bonsai/",
	 \&check_urlbase);

DefParam("toplevel",
         "What is the top level of bonsai called.  Links to
         the toplevel.cgi script will be named this.",
         "t",
         "hooklist");

DefParam("cvsadmin",
	 "The email address of the person responsible for cvs.",
	 "t",
         '%maintainer%');

DefParam("dbiparam",
         "The first parameter to pass to the DBI->connect() method.<br>Example: <code>DBI:mysql:host=localhost;database=bonsai</code>",
         "t",
         "DBI:mysql:database=bonsai;");

DefParam("mysqluser",
         "The username of the bonsai database user.",
         "t",
         "nobody");

DefParam("mysqlpassword",
         "The password of the bonsai database user.",
         "p",
         "");

DefParam("shadowdbiparam",
         "The first parameter to pass to the DBI->connect() method of a read-only replicated slave database to use for queries, to help with performance on high-traffic systems.  If left blank, queries will be made against the primary database and this param will be ignored.<br>Example: <code>DBI:mysql:host=slaveserver;database=bonsai</code>",
         "t",
         "");

DefParam("shadowmysqluser",
         "The username of the bonsai database user for the shadow database.",
         "t",
         "nobody");

DefParam("shadowmysqlpassword",
         "The password of the bonsai database user for the shadow database.",
         "p",
         "");

DefParam("readonly",
         "Are the hook files readonly.  (This value gets changed on the fly,
so it is ok to leave the way it is.)",
         "b",
         0);


##
## Page configuration (look and feel)
##
DefParam("headerhtml",
         "Additional HTML to add to the HEAD area of documents, eg. links to stylesheets.",
         "l",
         '');


DefParam("bannerhtml",
         "The html that gets emitted at the head of every Bonsai page. 
Anything of the form %<i>word</i>% gets replaced by the defintion of that 
word (as defined on this page).",
         "l",
         q{<TABLE BGCOLOR="#FFFFFF" WIDTH="100%" BORDER=0 CELLPADDING=0 CELLSPACING=0>
<TR><TD><!-- insert imagery here --></TD></TR></TABLE>
<CENTER><FONT SIZE=-1>Bonsai version %version%
</FONT></CENTER>});

DefParam("blurbhtml",
         "A blurb that appears as part of the header of every Bonsai page.  This is a place to put brief warnings, pointers to one or two related pages, etc.",
         "l",

         "This is <B>Bonsai</B>: a query interface to the CVS source repository");



##
## Command addresses/locations
##
DefParam("mailrelay",
         "This is the default mail relay (SMTP Server) that we use to transmit email messages.",
         "t",
         'localhost');

DefParam("cvscommand",
         "This is the location of the CVS command.",
         "t",
         '/usr/bin/cvs');

DefParam("rlogcommand",
         "This is the location of the rlog command.",
         "t",
         '/usr/bin/rlog');

DefParam("rcsdiffcommand",
         "This is the location of the rcsdiff command.",
         "t",
         '/usr/bin/rcsdiff');

DefParam("cocommand",
         "This is the location of the RCS co command.",
         "t",
         '/usr/bin/co');

DefParam("cvsgraph",
         "cvsgraph is an application that will output, in the form of a
graphic, every branch, tag, and revision that exists for a file.  It requires
that the <a href=\"http://www.akhphd.au.dk/~bertho/cvsgraph/\">cvsgraph
executable</a> be installed on this system.  If you don't wish to use
cvsgraph, leave this param blank.",
         "t",
         "");


##
## Things that we link to on the fly
##
DefParam("lxr_base",
	 "The URL that is the common initial leading part of all LXR URLs.",
	 "t",
	 "http://lxr.mozilla.org/",
	 \&check_urlbase);

DefParam("lxr_mungeregexp",
         'A regexp to use to munge a pathname from the $CVSROOT into a valid LXR pathname.  So, for example, if we tend to have a lot of pathnames that start with "mozilla/", and the LXR URLs should not contain that leading mozilla/, then you would use something like:  s@^mozilla/@@',
         "t",
         "");

DefParam("bugs_base",
	 "The URL that is the common initial leading part of all Bugzilla URLs.",
	 "t",
	 "http://bugzilla.mozilla.org/",
	 \&check_urlbase);

DefParam("bugsmatch",
         'Bugsmatch defines the number of consecutive digits that identify a bug to link to.',
         't',
         2);

DefParam("bugsystemexpr",
         'Bugsystemexpr defines what to replace a number found in log
         messages with.  It is used to generate an HTML reference to
         the bug database in the displayed text.  The number of the
         bug found can be inserted using the %bug_id% substitution.',
         "t",
         '<A HREF="%bugs_base%show_bug.cgi?id=%bug_id%">%bug_id%</A>');


##
## Email Addresses that get sent messages automatically when certain
## events happen
##
DefParam("bonsai-hookinterest",
         "The email address of the build team interested in the status of the hook.",
         "t",
         "bonsai-hookinterest");

DefParam("bonsai-daemon",
         "The email address of the sender of Bonsai related mail.",
         "t",
         "bonsai-daemon");

DefParam("bonsai-messageinterest",
         "The email address of those interested in the status of Bonsai itself.",
         "t",
         "bonsai-messageinterest");

DefParam("bonsai-treeinterest",
         "The email address of those interested in the status of development trees.",
         "t",
         "bonsai-treeinterest");

DefParam("software",
         "The email address list of those doing development on the trees.",
         "t",
         "software");


##
##  LDAP configuration
##
DefParam("ldapserver",
         "The address ofthe LDAP server containing name information,
         leave blank if you don't have an LDAP server.",
         "t", 
         '');

DefParam("ldapport",
         "The port of the LDAP server.",
         "t", 
         389);


##
##  Other URLs
##
DefParam("tinderboxbase",
         "The base URL of the tinderbox build pages.  Leave blank if
         you don't want to use tinderbox.",
         "t",
         "");

DefParam("other_ref_urls",
         "A list of pointers to other documentation, displayed on main bonsai menu",
         "l",
         '<a href=http://www.mozilla.org/hacking/bonsai.html>Mozilla\'s Introduction to Bonsai.</a><br>');


DefParam("phonebookurl",
         'A URL used to generate lookups for usernames.  The following
         parameters are substituted: %user_name% for the user\'s name
         in bonsai; %email_name% for the user\'s email address; and
         %account_name% for the user\'s account name on their email
         system (ie account_name@some.domain).',
         "t",
#         '<a href="http://phonebook/ds/dosearch/phonebook/uid=%account_name%,ou=People,o= Netscape Communications Corp.,c=US">%user_name%</a>'
         '<a href="mailto:%email_name%">%user_name%</a>'
        );

DefParam("registryurl",
         "A URL relative to urlbase (or an absolute URL) which leads to the
installed 'registry' package (available from the mozilla.org repository as
a sibling directory to the 'bonsai' directory.).  This contains pages that 
generate lists of links about a person or a file.",
         "t",
         qq{../registry/},
	 \&check_registryurl);



1;

