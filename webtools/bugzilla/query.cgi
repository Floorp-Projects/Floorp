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
#                 David Gardiner <david.gardiner@unisa.edu.au>

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":

use vars @::legal_resolution,
  @::legal_product,
  @::legal_bug_status,
  @::legal_priority,
  @::legal_resolution,
  @::legal_opsys,
  @::legal_platform,
  @::legal_components,
  @::legal_versions,
  @::legal_severity,
  @::legal_target_milestone,
  %::versions,
  %::components,
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
                  "component", "version",
                  "email1", "emailtype1", "emailreporter1",
                  "emailassigned_to1", "emailcc1", "emailqa_contact1",
                  "email2", "emailtype2", "emailreporter2",
                  "emailassigned_to2", "emailcc2", "emailqa_contact2") {
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

sub GenerateEmailInput {
    my ($id) = (@_);
    my $defstr = value_quote($default{"email$id"});
    my $deftype = $default{"emailtype$id"};
    if ($deftype eq "") {
        $deftype = "substring";
    }
    my $assignedto = ($default{"emailassigned_to$id"} eq "1") ? "checked" : "";
    my $reporter = ($default{"emailreporter$id"} eq "1") ? "checked" : "";
    my $cc = ($default{"emailcc$id"} eq "1") ? "checked" : "";

    if ($assignedto eq "" && $reporter eq "" && $cc eq "") {
        if ($id eq "1") {
            $assignedto = "checked";
        } else {
            $reporter = "checked";
        }
    }

    my $qapart = "";
    if (Param("useqacontact")) {
        my $qacontact =
            ($default{"emailqa_contact$id"} eq "1") ? "checked" : "";
        $qapart = qq|
<tr>
<td></td>
<td>
<input type="checkbox" name="emailqa_contact$id" value=1 $qacontact>QA Contact
</td>
</tr>
|;
    }

    return qq|
<table border=1 cellspacing=0 cellpadding=0>
<tr><td>
<table cellspacing=0 cellpadding=0>
<tr>
<td rowspan=2 valign=top><a href="helpemailquery.html">Email:</a>
<input name="email$id" size="30" value="">&nbsp;matching as
<SELECT NAME=emailtype$id>
<OPTION VALUE="regexp">regexp
<OPTION VALUE="notregexp">not regexp
<OPTION SELECTED VALUE="substring">substring
<OPTION VALUE="exact">exact
</SELECT>
</td>
<td>
<input type="checkbox" name="emailassigned_to$id" value=1 $assignedto>Assigned To
</td>
</tr>
<tr>
<td>
<input type="checkbox" name="emailreporter$id" value=1 $reporter>Reporter
</td>
</tr>$qapart
<tr>
<td align=right>(Will match any of the selected fields)</td>
<td>
<input type="checkbox" name="emailcc$id" value=1 $cc>CC &nbsp;&nbsp;
</td>
</tr>
</table>
</table>
|;
}


            


my $emailinput1 = GenerateEmailInput(1);
my $emailinput2 = GenerateEmailInput(2);


# javascript
    
my $jscript = << 'ENDSCRIPT';
<script language="Javascript1.2">
<!--
var cpts = new Array();
var vers = new Array();
ENDSCRIPT


my $p;
my $v;
my $c;
my $i = 0;
my $j = 0;

foreach $c (@::legal_components) {
    $jscript .= "cpts['$c'] = [];\n";
}

foreach $v (@::legal_versions) {
    $jscript .= "vers['$v'] = [];\n";
}


for $p (@::legal_product) {
    foreach $c (@{$::components{$p}}) {
        $jscript .= "cpts['$c'].push('$p');\n";
    }
    foreach $v (@{$::versions{$p}}) {
        $jscript .= "vers['$v'].push('$p');\n";
    }
}

$i = 0;
$jscript .= "

// Only display versions/components valid for selected product(s)

function selectProduct(f) {
    var cnt = 0;
    var i;
    var j;
    for (i=0 ; i<f.product.length ; i++) {
        if (f.product[i].selected) {
            cnt++;
        }
    }
    var doall = (cnt == f.product.length || cnt == 0);

    var csel = new Array();
    for (i=0 ; i<f.component.length ; i++) {
        if (f.component[i].selected) {
            csel[f.component[i].value] = 1;
        }
    }

    f.component.options.length = 0;

    for (c in cpts) {
        var doit = doall;
        for (i=0 ; !doit && i<f.product.length ; i++) {
            if (f.product[i].selected) {
                var p = f.product[i].value;
                for (j in cpts[c]) {
                    var p2 = cpts[c][j];
                    if (p2 == p) {
                        doit = true;
                        break;
                    }
                }
            }
        }
        if (doit) {
            var l = f.component.length;
            f.component[l] = new Option(c, c);
            if (csel[c] != undefined) {
                f.component[l].selected = true;
            }
        }
    }

    var vsel = new Array();
    for (i=0 ; i<f.version.length ; i++) {
        if (f.version[i].selected) {
            vsel[f.version[i].value] = 1;
        }
    }

    f.version.options.length = 0;

    for (v in vers) {
        var doit = doall;
        for (i=0 ; !doit && i<f.product.length ; i++) {
            if (f.product[i].selected) {
                var p = f.product[i].value;
                for (j in vers[v]) {
                    var p2 = vers[v][j];
                    if (p2 == p) {
                        doit = true;
                        break;
                    }
                }
            }
        }
        if (doit) {
            var l = f.version.length;
            f.version[l] = new Option(v, v);
            if (vsel[v] != undefined) {
                f.version[l].selected = true;
            }
        }
    }




}
// -->
</script>\n";



# Muck the "legal product" list so that the default one is always first (and
# is therefore visibly selected.

# Commented out, until we actually have enough products for this to matter.

# set w [lsearch $legal_product $default{"product"}]
# if {$w >= 0} {
#    set legal_product [concat $default{"product"} [lreplace $legal_product $w $w]]
# }

PutHeader("Bugzilla Query Page", "Query Page");

push @::legal_resolution, "---"; # Oy, what a hack.
push @::legal_target_milestone, "---"; # Oy, what a hack.

print $jscript;

print "
<FORM NAME=queryForm METHOD=GET ACTION=\"buglist.cgi\">

<table>
<tr>
<th align=left><A HREF=\"bug_status.html\">Status</a>:</th>
<th align=left><A HREF=\"bug_status.html\">Resolution</a>:</th>
<th align=left><A HREF=\"bug_status.html#rep_platform\">Platform</a>:</th>
<th align=left><A HREF=\"bug_status.html#op_sys\">OpSys</a>:</th>
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
<SELECT NAME=\"op_sys\" MULTIPLE SIZE=7>
@{[make_options(\@::legal_opsys, $default{'op_sys'}, $type{'op_sys'})]}
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
$emailinput1<p>
$emailinput2<p>




<NOBR>Changed in the last <INPUT NAME=changedin SIZE=2> days.</NOBR>


<P>

<table>
<tr>
<TH ALIGN=LEFT VALIGN=BOTTOM>Program:</th>
<TH ALIGN=LEFT VALIGN=BOTTOM>Version:</th>
<TH ALIGN=LEFT VALIGN=BOTTOM><A HREF=describecomponents.cgi>Component:<a></th>
";

if (Param("usetargetmilestone")) {
    print "<TH ALIGN=LEFT VALIGN=BOTTOM>Target Milestone:</th>";
}

print "
</tr>
<tr>

<td align=left valign=top>
<SELECT NAME=\"product\" MULTIPLE SIZE=5 onChange=\"selectProduct(this.form);\">
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
</td>";

if (Param("usetargetmilestone")) {
    print "
<td align=left valign=top>
<SELECT NAME=\"target_milestone\" MULTIPLE SIZE=5>
@{[make_options(\@::legal_target_milestone, $default{'component'}, $type{'component'})]}
</SELECT>
</td>";
}

print "
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
<tr>
<td align=right>URL:</td>
<td><input name=bug_file_loc size=30></td>
<td><input type=radio name=bug_file_loc_type value=substr checked>Substring</td>
<td><input type=radio name=bug_file_loc_type value=regexp>Regexp</td>
</tr>";

if (Param("usestatuswhiteboard")) {
    print "
<tr>
<td align=right>Status whiteboard:</td>
<td><input name=status_whiteboard size=30></td>
<td><input type=radio name=status_whiteboard_type value=substr checked>Substring</td>
<td><input type=radio name=status_whiteboard_type value=regexp>Regexp</td>
</tr>";
}

print "
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


quietly_check_login();

if (UserInGroup("tweakparams")) {
    print "<a href=editparams.cgi>Edit Bugzilla operating parameters</a><br>\n";
}
if (UserInGroup("editcomponents")) {
    print "<a href=editcomponents.cgi>Edit Bugzilla components</a><br>\n";
}
if (defined $::COOKIE{"Bugzilla_login"}) {
    print "<a href=relogin.cgi>Log in as someone besides <b>$::COOKIE{'Bugzilla_login'}</b></a><br>\n";
}
print "<a href=changepassword.cgi>Change your password.</a><br>\n";
print "<a href=\"enter_bug.cgi\">Create a new bug.</a><br>\n";
print "<a href=\"reports.cgi\">Bug reports</a><br>\n";
