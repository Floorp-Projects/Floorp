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

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":

use vars @::legal_resolution,
  @::legal_product,
  @::legal_bug_status,
  @::legal_priority,
  @::legal_resolution,
  @::legal_platform,
  @::legal_components,
  @::legal_versions,
  @::legal_severity,
  %::FORM;


if (defined $::FORM{"GoAheadAndLogIn"}) {
    # We got here from a login page, probably from relogin.cgi.  We better
    # make sure the password is legit.
    confirm_login();
}

if (!defined $::COOKIE{"DEFAULTQUERY"}) {
    $::COOKIE{"DEFAULTQUERY"} = Param("defaultquery");
}

if (!defined $::buffer || $::buffer eq "") {
    $::buffer = $::COOKIE{"DEFAULTQUERY"};
}

my %default;
my %type;

foreach my $name ("bug_status", "resolution", "assigned_to", "rep_platform",
                  "priority", "bug_severity", "product", "reporter", "op_sys",
                  "component", "version") {
    $default{$name} = "";
    $type{$name} = 0;
}


foreach my $item (split(/\&/, $::buffer)) {
    my @el = split(/=/, $item);
    my $name = $el[0];
    my $value;
    if ($#el > 0) {
        $value = url_decode($el[1]);
    } else {
        $value = "";
    }
    if (defined $default{$name}) {
        if ($default{$name} ne "") {
            $default{$name} .= "|$value";
            $type{$name} = 1;
        } else {
            $default{$name} = $value;
        }
    }
}
                  


my $namelist = "";

foreach my $i (sort (keys %::COOKIE)) {
    if ($i =~ /^QUERY_/) {
        if ($::COOKIE{$i} ne "") {
            my $name = substr($i, 6); 
            $namelist .= "<OPTION>$name";
        }
    }
}

print "Set-Cookie: BUGLIST=
Content-type: text/html\n\n";

GetVersionTable();
my $who = GeneratePeopleInput("assigned_to", $default{"assigned_to"});
my $reporter = GeneratePeopleInput("reporter", $default{"reporter"});


# Muck the "legal product" list so that the default one is always first (and
# is therefore visibly selected.

# Commented out, until we actually have enough products for this to matter.

# set w [lsearch $legal_product $default{"product"}]
# if {$w >= 0} {
#    set legal_product [concat $default{"product"} [lreplace $legal_product $w $w]]
# }

PutHeader("Bugzilla Query Page", "Query Page");

push @::legal_resolution, "---"; # Oy, what a hack.

print "
<FORM NAME=queryForm METHOD=GET ACTION=\"buglist.cgi\">

<table>
<tr>
<th align=left><A HREF=\"bug_status.html\">Status</a>:</th>
<th align=left><A HREF=\"bug_status.html\">Resolution</a>:</th>
<th align=left><A HREF=\"bug_status.html#rep_platform\">Platform</a>:</th>
<th align=left><A HREF=\"bug_status.html#priority\">Priority</a>:</th>
<th align=left><A HREF=\"bug_status.html#severity\">Severity</a>:</th>
</tr>
<tr>
<td align=left valign=top>
<SELECT NAME=\"bug_status\" MULTIPLE SIZE=7>
@{[make_options(\@::legal_bug_status, $default{'bug_status'}, $type{'bug_status'})]}
</SELECT>
</td>
<td align=left valign=top>
<SELECT NAME=\"resolution\" MULTIPLE SIZE=7>
@{[make_options(\@::legal_resolution, $default{'resolution'}, $type{'resolution'})]}
</SELECT>
</td>
<td align=left valign=top>
<SELECT NAME=\"rep_platform\" MULTIPLE SIZE=7>
@{[make_options(\@::legal_platform, $default{'rep_platform'}, $type{'rep_platform'})]}
</SELECT>
</td>
<td align=left valign=top>
<SELECT NAME=\"priority\" MULTIPLE SIZE=7>
@{[make_options(\@::legal_priority, $default{'priority'}, $type{'priority'})]}
</SELECT>
</td>
<td align=left valign=top>
<SELECT NAME=\"bug_severity\" MULTIPLE SIZE=7>
@{[make_options(\@::legal_severity, $default{'bug_severity'}, $type{'bug_severity'})]}
</SELECT>
</tr>
</table>

<p>
<TABLE>
<TR><TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#assigned_to\">Assigned To:</a></B><TD>$who

<p>
<TR><TD ALIGN=RIGHT><B>Reporter:</B><TD>$reporter
</TABLE>
<NOBR>Changed in the last <INPUT NAME=changedin SIZE=2> days.</NOBR>


<P>

<table>
<tr>
<TH ALIGN=LEFT>Program:</th>
<TH ALIGN=LEFT>Version:</th>
<TH ALIGN=LEFT>Component:</th>
</tr>
<tr>

<td align=left valign=top>
<SELECT NAME=\"product\" MULTIPLE SIZE=5>
@{[make_options(\@::legal_product, $default{'product'}, $type{'product'})]}
</SELECT>
</td>

<td align=left valign=top>
<SELECT NAME=\"version\" MULTIPLE SIZE=5>
@{[make_options(\@::legal_versions, $default{'version'}, $type{'version'})]}
</SELECT>
</td>

<td align=left valign=top>
<SELECT NAME=\"component\" MULTIPLE SIZE=5>
@{[make_options(\@::legal_components, $default{'component'}, $type{'component'})]}
</SELECT>
</td>

</tr>
</table>

<table border=0>
<tr>
<td align=right>Summary:</td>
<td><input name=short_desc size=30></td>
<td><input type=radio name=short_desc_type value=substr checked>Substring</td>
<td><input type=radio name=short_desc_type value=regexp>Regexp</td>
</tr>
<tr>
<td align=right>Description:</td>
<td><input name=long_desc size=30></td>
<td><input type=radio name=long_desc_type value=substr checked>Substring</td>
<td><input type=radio name=long_desc_type value=regexp>Regexp</td>
</tr>
</table>
<p>



<BR>
<INPUT TYPE=radio NAME=cmdtype VALUE=doit CHECKED> Run this query
<BR>
";

if ($namelist ne "") {
    print "
<table cellspacing=0 cellpadding=0><tr>
<td><INPUT TYPE=radio NAME=cmdtype VALUE=editnamed> Load the remembered query:</td>
<td rowspan=3><select name=namedcmd>$namelist</select>
</tr><tr>
<td><INPUT TYPE=radio NAME=cmdtype VALUE=runnamed> Run the remembered query:</td>
</tr><tr>
<td><INPUT TYPE=radio NAME=cmdtype VALUE=forgetnamed> Forget the remembered query:</td>
</tr></table>"
}

print "
<INPUT TYPE=radio NAME=cmdtype VALUE=asdefault> Remember this as the default query
<BR>
<INPUT TYPE=radio NAME=cmdtype VALUE=asnamed> Remember this query, and name it:
<INPUT TYPE=text NAME=newqueryname>
<BR>

<NOBR><B>Sort By:</B>
<SELECT NAME=\"order\">
  <OPTION>Bug Number
  <OPTION SELECTED>\"Importance\"
  <OPTION>Assignee
</SELECT></NOBR>
<INPUT TYPE=\"submit\" VALUE=\"Submit query\">
<INPUT TYPE=\"reset\" VALUE=\"Reset back to the default query\">
<INPUT TYPE=hidden name=form_name VALUE=query>
<BR>Give me a <A HREF=\"help.html\">clue</A> about how to use this form.
</CENTER>
</FORM>

";


if (defined $::COOKIE{"Bugzilla_login"}) {
    if ($::COOKIE{"Bugzilla_login"} eq Param("maintainer")) {
        print "<a href=editparams.cgi>Edit Bugzilla operating parameters</a><br>\n";
        print "<a href=editowners.cgi>Edit Bugzilla component owners</a><br>\n";
    }
    print "<a href=relogin.cgi>Log in as someone besides <b>$::COOKIE{'Bugzilla_login'}</b></a><br>\n";
}
print "<a href=changepassword.cgi>Change your password.</a><br>\n";
print "<a href=\"enter_bug.cgi\">Create a new bug.</a><br>\n";
print "<a href=\"reports.cgi\">Bug reports</a><br>\n";




