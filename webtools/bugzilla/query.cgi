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
#                 David Gardiner <david.gardiner@unisa.edu.au>
#                 Matthias Radestock <matthias@sorted.org>

use diagnostics;
use strict;

require "CGI.pl";

$::CheckOptionValues = 0;       # It's OK if we have some bogus things in the
                                # pop-up lists here, from a remembered query
                                # that is no longer quite valid.  We don't
                                # want to crap out in the query page.

# Shut up misguided -w warnings about "used only once":

use vars
  @::CheckOptionValues,
  @::legal_resolution,
  @::legal_bug_status,
  @::legal_components,
  @::legal_keywords,
  @::legal_opsys,
  @::legal_platform,
  @::legal_priority,
  @::legal_product,
  @::legal_severity,
  @::legal_target_milestone,
  @::legal_versions,
  @::log_columns,
  %::versions,
  %::components,
  %::FORM;


if (defined $::FORM{"GoAheadAndLogIn"}) {
    # We got here from a login page, probably from relogin.cgi.  We better
    # make sure the password is legit.
    confirm_login();
} else {
    quietly_check_login();
}
my $userid = 0;
if (defined $::COOKIE{"Bugzilla_login"}) {
    $userid = DBNameToIdAndCheck($::COOKIE{"Bugzilla_login"});
}

# Backwards compatability hack -- if there are any of the old QUERY_*
# cookies around, and we are logged in, then move them into the database
# and nuke the cookie.
if ($userid) {
    my @oldquerycookies;
    foreach my $i (keys %::COOKIE) {
        if ($i =~ /^QUERY_(.*)$/) {
            push(@oldquerycookies, [$1, $i, $::COOKIE{$i}]);
        }
    }
    if (defined $::COOKIE{'DEFAULTQUERY'}) {
        push(@oldquerycookies, [$::defaultqueryname, 'DEFAULTQUERY',
                                $::COOKIE{'DEFAULTQUERY'}]);
    }
    if (@oldquerycookies) {
        foreach my $ref (@oldquerycookies) {
            my ($name, $cookiename, $value) = (@$ref);
            if ($value) {
                my $qname = SqlQuote($name);
                SendSQL("SELECT query FROM namedqueries " .
                        "WHERE userid = $userid AND name = $qname");
                my $query = FetchOneColumn();
                if (!$query) {
                    SendSQL("REPLACE INTO namedqueries " .
                            "(userid, name, query) VALUES " .
                            "($userid, $qname, " . SqlQuote($value) . ")");
                }
            }
            print "Set-Cookie: $cookiename= ; path=/ ; expires=Sun, 30-Jun-1980 00:00:00 GMT\n";
        }
    }
}
                



if ($::FORM{'nukedefaultquery'}) {
    if ($userid) {
        SendSQL("DELETE FROM namedqueries " .
                "WHERE userid = $userid AND name = '$::defaultqueryname'");
    }
    $::buffer = "";
}


my $userdefaultquery;
if ($userid) {
    SendSQL("SELECT query FROM namedqueries " .
            "WHERE userid = $userid AND name = '$::defaultqueryname'");
    $userdefaultquery = FetchOneColumn();
}

my %default;
my %type;

sub ProcessFormStuff {
    my ($buf) = (@_);
    my $foundone = 0;
    foreach my $name ("bug_status", "resolution", "assigned_to",
                      "rep_platform", "priority", "bug_severity",
                      "product", "reporter", "op_sys",
                      "component", "version", "chfield", "chfieldfrom",
                      "chfieldto", "chfieldvalue",
                      "email1", "emailtype1", "emailreporter1",
                      "emailassigned_to1", "emailcc1", "emailqa_contact1",
                      "emaillongdesc1",
                      "email2", "emailtype2", "emailreporter2",
                      "emailassigned_to2", "emailcc2", "emailqa_contact2",
                      "emaillongdesc2",
                      "changedin", "votes", "short_desc", "short_desc_type",
                      "long_desc", "long_desc_type", "bug_file_loc",
                      "bug_file_loc_type", "status_whiteboard",
                      "status_whiteboard_type", "bug_id",
                      "bugidtype", "keywords", "keywords_type") {
        $default{$name} = "";
        $type{$name} = 0;
    }


    foreach my $item (split(/\&/, $buf)) {
        my @el = split(/=/, $item);
        my $name = $el[0];
        my $value;
        if ($#el > 0) {
            $value = url_decode($el[1]);
        } else {
            $value = "";
        }
        if (defined $default{$name}) {
            $foundone = 1;
            if ($default{$name} ne "") {
                $default{$name} .= "|$value";
                $type{$name} = 1;
            } else {
                $default{$name} = $value;
            }
        }
    }
    return $foundone;
}


if (!ProcessFormStuff($::buffer)) {
    # Ah-hah, there was no form stuff specified.  Do it again with the
    # default query.
    if ($userdefaultquery) {
        ProcessFormStuff($userdefaultquery);
    } else {
        ProcessFormStuff(Param("defaultquery"));
    }
}


                 

if ($default{'chfieldto'} eq "") {
    $default{'chfieldto'} = "Now";
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
    my $longdesc = ($default{"emaillongdesc$id"} eq "1") ? "checked" : "";

    my $qapart = "";
    my $qacontact = "";
    if (Param("useqacontact")) {
        $qacontact = ($default{"emailqa_contact$id"} eq "1") ? "checked" : "";
        $qapart = qq|
<tr>
<td></td>
<td>
<input type="checkbox" name="emailqa_contact$id" value=1 $qacontact>QA Contact
</td>
</tr>
|;
    }
    if ($assignedto eq "" && $reporter eq "" && $cc eq "" &&
          $qacontact eq "") {
        if ($id eq "1") {
            $assignedto = "checked";
        } else {
            $reporter = "checked";
        }
    }


    $default{"emailtype$id"} ||= "substring";

    return qq{
<table border=1 cellspacing=0 cellpadding=0>
<tr><td>
<table cellspacing=0 cellpadding=0>
<tr>
<td rowspan=2 valign=top><a href="helpemailquery.html">Email:</a>
<input name="email$id" size="30" value="$defstr">&nbsp;matching as
} . BuildPulldown("emailtype$id",
                  [["regexp", "regexp"],
                   ["notregexp", "not regexp"],
                   ["substring", "substring"],
                   ["exact", "exact"]],
                  $default{"emailtype$id"}) . qq{
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
<input type="checkbox" name="emailcc$id" value=1 $cc>CC
</td>
</tr>
<tr>
<td></td>
<td>
<input type="checkbox" name="emaillongdesc$id" value=1 $longdesc>Added comment
</td>
</tr>
</table>
</table>
};
}


            


my $emailinput1 = GenerateEmailInput(1);
my $emailinput2 = GenerateEmailInput(2);

# if using usebuggroups, then we don't want people to see products they don't
# have access to.  remove them from the list.

@::product_list = ();
my %component_set;
my %version_set;
my %milestone_set;
foreach my $p (@::legal_product) {
  if(Param("usebuggroups")
     && GroupExists($p)
     && !UserInGroup($p)) {
    # If we're using bug groups to restrict entry on products, and
    # this product has a bug group, and the user is not in that
    # group, we don't want to include that product in this list.
    next;
  }
  push @::product_list, $p;
  foreach my $c (@{$::components{$p}}) {
    $component_set{$c} = 1;
  }
  foreach my $v (@{$::versions{$p}}) {
    $version_set{$v} = 1;
  }
  foreach my $m (@{$::target_milestone{$p}}) {
    $milestone_set{$m} = 1;
  }
}

@::component_list = ();
@::version_list = ();
@::milestone_list = ();
foreach my $c (@::legal_components) {
  if ($component_set{$c}) {
    push @::component_list, $c;
  }
}
foreach my $v (@::legal_versions) {
  if ($version_set{$v}) {
    push @::version_list, $v;
  }
}
foreach my $m (@::legal_target_milestone) {
  if ($milestone_set{$m}) {
    push @::milestone_list, $m;
  }
}

# javascript
    
my $jscript = << 'ENDSCRIPT';
<script language="Javascript1.1" type="text/javascript">
<!--
var cpts = new Array();
var vers = new Array();
var tms  = new Array();
ENDSCRIPT


my $p;
my $v;
my $c;
my $m;
my $i = 0;
my $j = 0;

foreach $c (@::component_list) {
    $jscript .= "cpts['$c'] = new Array();\n";
}

foreach $v (@::version_list) {
    $jscript .= "vers['$v'] = new Array();\n";
}

my $tm;
foreach $tm (@::milestone_list) {
    $jscript .= "tms['$tm'] = new Array();\n";
}

for $p (@::product_list) {
    if ($::components{$p}) {
        foreach $c (@{$::components{$p}}) {
            $jscript .= "cpts['$c'][cpts['$c'].length] = '$p';\n";
        }
    }

    if ($::versions{$p}) {
        foreach $v (@{$::versions{$p}}) {
            $jscript .= "vers['$v'][vers['$v'].length] = '$p';\n";
        }
    }

    if ($::target_milestone{$p}) {
        foreach $m (@{$::target_milestone{$p}}) {
            $jscript .= "tms['$m'][tms['$m'].length] = '$p';\n";
        }
    }
}

$i = 0;
$jscript .= q{

// Only display versions/components valid for selected product(s)

function selectProduct(f) {
    // Netscape 4.04 and 4.05 also choke with an "undefined"
    // error.  if someone can figure out how to "define" the
    // whatever, we'll remove this hack.  in the mean time, we'll
    // assume that v4.00-4.03 also die, so we'll disable the neat
    // javascript stuff for Netscape 4.05 and earlier.

    var cnt = 0;
    var i;
    var j;

    if (!f) {
        return;
    }

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
        if (typeof(cpts[c]) == 'function') continue;
        var doit = doall;
        for (i=0 ; !doit && i<f.product.length ; i++) {
            if (f.product[i].selected) {
                var p = f.product[i].value;
                for (j in cpts[c]) {
                    if (typeof(cpts[c][j]) == 'function') continue;
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
            if (csel[c]) {
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
        if (typeof(vers[v]) == 'function') continue;
        var doit = doall;
        for (i=0 ; !doit && i<f.product.length ; i++) {
            if (f.product[i].selected) {
                var p = f.product[i].value;
                for (j in vers[v]) {
                    if (typeof(vers[v][j]) == 'function') continue;
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
            if (vsel[v]) {
                f.version[l].selected = true;
            }
        }
    }

    if (f.target_milestone) {
        var tmsel = new Array();
        for (i=0 ; i<f.target_milestone.length ; i++) {
            if (f.target_milestone[i].selected) {
                tmsel[f.target_milestone[i].value] = 1;
            }
        }
    
        f.target_milestone.options.length = 0;
    
        for (tm in tms) {
            if (typeof(tms[v]) == 'function') continue;
            var doit = doall;
            for (i=0 ; !doit && i<f.product.length ; i++) {
                if (f.product[i].selected) {
                    var p = f.product[i].value;
                    for (j in tms[tm]) {
                        if (typeof(tms[tm][j]) == 'function') continue;
                        var p2 = tms[tm][j];
                        if (p2 == p) {
                            doit = true;
                            break;
                        }
                    }
                }
            }
            if (doit) {
                var l = f.target_milestone.length;
                f.target_milestone[l] = new Option(tm, tm);
                if (tmsel[tm]) {
                    f.target_milestone[l].selected = true;
                }
            }
        }
    }
}
// -->
</script>

};



# Muck the "legal product" list so that the default one is always first (and
# is therefore visibly selected.

# Commented out, until we actually have enough products for this to matter.

# set w [lsearch $legal_product $default{"product"}]
# if {$w >= 0} {
#    set legal_product [concat $default{"product"} [lreplace $legal_product $w $w]]
# }

PutHeader("Bugzilla Query Page", "Query", 
          "This page lets you search the database for recorded bugs.",
          q{onLoad="selectProduct(document.forms[0]);"},
          0, $jscript);

push @::legal_resolution, "---"; # Oy, what a hack.

my @logfields = ("[Bug creation]", @::log_columns);

print qq{
<FORM METHOD=GET ACTION="buglist.cgi">

<table>
<tr>
<th align=left><A HREF="bug_status.html">Status</a>:</th>
<th align=left><A HREF="bug_status.html">Resolution</a>:</th>
<th align=left><A HREF="bug_status.html#rep_platform">Platform</a>:</th>
<th align=left><A HREF="bug_status.html#op_sys">OpSys</a>:</th>
<th align=left><A HREF="bug_status.html#priority">Priority</a>:</th>
<th align=left><A HREF="bug_status.html#severity">Severity</a>:</th>
};

print "
</tr>
<tr>
<td align=left valign=top>

@{[make_selection_widget(\"bug_status\",\@::legal_bug_status,$default{'bug_status'}, $type{'bug_status'}, 1)]}

</td>
<td align=left valign=top>
@{[make_selection_widget(\"resolution\",\@::legal_resolution,$default{'resolution'}, $type{'resolution'}, 1)]}

</td>
<td align=left valign=top>
@{[make_selection_widget(\"rep_platform\",\@::legal_platform,$default{'platform'}, $type{'platform'}, 1)]}

</td>
<td align=left valign=top>
@{[make_selection_widget(\"op_sys\",\@::legal_opsys,$default{'op_sys'}, $type{'op_sys'}, 1)]}

</td>
<td align=left valign=top>
@{[make_selection_widget(\"priority\",\@::legal_priority,$default{'priority'}, $type{'priority'}, 1)]}

</td>
<td align=left valign=top>
@{[make_selection_widget(\"bug_severity\",\@::legal_severity,$default{'bug_severity'}, $type{'bug_severity'}, 1)]}

</tr>
</table>

<p>

<table>
<tr><td colspan=2>
$emailinput1<p>
</td></tr><tr><td colspan=2>
$emailinput2<p>
</td></tr>";

my $inclselected = "SELECTED";
my $exclselected = "";

    
if ($default{'bugidtype'} eq "exclude") {
    $inclselected = "";
    $exclselected = "SELECTED";
}
my $bug_id = value_quote($default{'bug_id'}); 

print qq{
<TR>
<TD COLSPAN="3">
<SELECT NAME="bugidtype">
<OPTION VALUE="include" $inclselected>Only
<OPTION VALUE="exclude" $exclselected>Exclude
</SELECT>
bugs numbered: 
<INPUT TYPE="text" NAME="bug_id" VALUE="$bug_id" SIZE=30>
</TD>
</TR>
};

print "
<tr>
<td>
Changed in the <NOBR>last <INPUT NAME=changedin SIZE=2 VALUE=\"$default{'changedin'}\"> days.</NOBR>
</td>
<td align=right>
At <NOBR>least <INPUT NAME=votes SIZE=3 VALUE=\"$default{'votes'}\"> votes.</NOBR>
</tr>
</table>


<table>
<tr>
<td rowspan=2 align=right>Where the field(s)
</td><td rowspan=2>
<SELECT NAME=\"chfield\" MULTIPLE SIZE=4>
@{[make_options(\@logfields, $default{'chfield'}, $type{'chfield'})]}
</SELECT>
</td><td rowspan=2>
changed.
</td><td>
<nobr>dates <INPUT NAME=chfieldfrom SIZE=10 VALUE=\"$default{'chfieldfrom'}\"></nobr>
<nobr>to <INPUT NAME=chfieldto SIZE=10 VALUE=\"$default{'chfieldto'}\"></nobr>
</td>
</tr>
<tr>
<td>changed to value <nobr><INPUT NAME=chfieldvalue SIZE=10> (optional)</nobr>
</td>
</table>


<P>

<table>
<tr>
<TH ALIGN=LEFT VALIGN=BOTTOM>Program:</th>
<TH ALIGN=LEFT VALIGN=BOTTOM>Version:</th>
<TH ALIGN=LEFT VALIGN=BOTTOM><A HREF=describecomponents.cgi>Component:</a></th>
";

if (Param("usetargetmilestone")) {
    print "<TH ALIGN=LEFT VALIGN=BOTTOM>Target Milestone:</th>";
}

print "
</tr>
<tr>

<td align=left valign=top>
<SELECT NAME=\"product\" MULTIPLE SIZE=5 onChange=\"selectProduct(this.form);\">
@{[make_options(\@::product_list, $default{'product'}, $type{'product'})]}
</SELECT>
</td>

<td align=left valign=top>
<SELECT NAME=\"version\" MULTIPLE SIZE=5>
@{[make_options(\@::version_list, $default{'version'}, $type{'version'})]}
</SELECT>
</td>

<td align=left valign=top>
<SELECT NAME=\"component\" MULTIPLE SIZE=5>
@{[make_options(\@::component_list, $default{'component'}, $type{'component'})]}
</SELECT>
</td>";

if (Param("usetargetmilestone")) {
    print "
<td align=left valign=top>
<SELECT NAME=\"target_milestone\" MULTIPLE SIZE=5>
@{[make_options(\@::milestone_list, $default{'target_milestone'}, $type{'target_milestone'})]}
</SELECT>
</td>";
}


sub StringSearch {
    my ($desc, $name) = (@_);
    my $type = $name . "_type";
    my $def = value_quote($default{$name});
    print qq{<tr>
<td align=right>$desc:</td>
<td><input name=$name size=30 value="$def"></td>
<td><SELECT NAME=$type>
};
    if ($default{$type} eq "") {
        $default{$type} = "substring";
    }
    foreach my $i (["substring", "case-insensitive substring"],
                   ["casesubstring", "case-sensitive substring"],
                   ["allwords", "all words"],
                   ["anywords", "any words"],
                   ["regexp", "regular expression"],
                   ["notregexp", "not ( regular expression )"]) {
        my ($n, $d) = (@$i);
        my $sel = "";
        if ($default{$type} eq $n) {
            $sel = " SELECTED";
        }
        print qq{<OPTION VALUE="$n"$sel>$d\n};
    }
    print "</SELECT></TD>
</tr>
";
}

print "
</tr>
</table>

<table border=0>
";

StringSearch("Summary", "short_desc");
StringSearch("A description entry", "long_desc");
StringSearch("URL", "bug_file_loc");

if (Param("usestatuswhiteboard")) {
    StringSearch("Status whiteboard", "status_whiteboard");
}

if (@::legal_keywords) {
    my $def = value_quote($default{'keywords'});
    print qq{
<TR>
<TD ALIGN="right"><A HREF="describekeywords.cgi">Keywords</A>:</TD>
<TD><INPUT NAME="keywords" SIZE=30 VALUE="$def"></TD>
<TD>
};
    my $type = $default{"keywords_type"};
    if ($type eq "or") {        # Backward compatability hack.
        $type = "anywords";
    }
    print BuildPulldown("keywords_type",
                        [["anywords", "Any of the listed keywords set"],
                         ["allwords", "All of the listed keywords set"],
                         ["nowords", "None of the listed keywords set"]],
                        $type);
    print qq{</TD></TR>};
}

print "
</table>
<p>
";


my @fields;
push(@fields, ["noop", "---"]);
ConnectToDatabase();
SendSQL("SELECT name, description FROM fielddefs ORDER BY sortkey");
while (MoreSQLData()) {
    my ($name, $description) = (FetchSQLData());
    push(@fields, [$name, $description]);
}

my @types = (
	     ["noop", "---"],
	     ["equals", "equal to"],
	     ["notequals", "not equal to"],
	     ["casesubstring", "contains (case-sensitive) substring"],
	     ["substring", "contains (case-insensitive) substring"],
	     ["notsubstring", "does not contain (case-insensitive) substring"],
	     ["regexp", "contains regexp"],
	     ["notregexp", "does not contain regexp"],
	     ["lessthan", "less than"],
	     ["greaterthan", "greater than"],
	     ["anywords", "any words"],
	     ["allwords", "all words"],
	     ["nowords", "none of the words"],
	     ["changedbefore", "changed before"],
	     ["changedafter", "changed after"],
	     ["changedto", "changed to"],
	     ["changedby", "changed by"],
	     );


print qq{<A NAME="chart"> </A>\n};

foreach my $cmd (grep(/^cmd-/, keys(%::FORM))) {
    if ($cmd =~ /^cmd-add(\d+)-(\d+)-(\d+)$/) {
	$::FORM{"field$1-$2-$3"} = "xyzzy";
    }
}
	
#  foreach my $i (sort(keys(%::FORM))) {
#      print "$i : " . value_quote($::FORM{$i}) . "<BR>\n";
#  }


if (!exists $::FORM{'field0-0-0'}) {
    $::FORM{'field0-0-0'} = "xyzzy";
}

my $jsmagic = qq{ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"};

my $chart;
for ($chart=0 ; exists $::FORM{"field$chart-0-0"} ; $chart++) {
    my @rows;
    my $row;
    for ($row = 0 ; exists $::FORM{"field$chart-$row-0"} ; $row++) {
	my @cols;
	my $col;
	for ($col = 0 ; exists $::FORM{"field$chart-$row-$col"} ; $col++) {
	    my $key = "$chart-$row-$col";
	    my $deffield = $::FORM{"field$key"} || "";
	    my $deftype = $::FORM{"type$key"} || "";
	    my $defvalue = value_quote($::FORM{"value$key"} || "");
	    my $line = "";
	    $line .= "<TD>";
	    $line .= BuildPulldown("field$key", \@fields, $deffield);
	    $line .= BuildPulldown("type$key", \@types, $deftype);
	    $line .= qq{<INPUT NAME="value$key" VALUE="$defvalue">};
	    $line .= "</TD>\n";
	    push(@cols, $line);
	}
	push(@rows, "<TR>" . join(qq{<TD ALIGN="center"> or </TD>\n}, @cols) .
	     qq{<TD><INPUT TYPE="submit" VALUE="Or" NAME="cmd-add$chart-$row-$col" $jsmagic></TD></TR>});
    }
    print qq{
<HR>
<TABLE>
};
    print join('<TR><TD>And</TD></TR>', @rows);
    print qq{
<TR><TD><INPUT TYPE="submit" VALUE="And" NAME="cmd-add$chart-$row-0" $jsmagic>
};
    my $n = $chart + 1;
    if (!exists $::FORM{"field$n-0-0"}) {
        print qq{
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<INPUT TYPE="submit" VALUE="Add another boolean chart" NAME="cmd-add$n-0-0" $jsmagic>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<NOBR><A HREF="booleanchart.html">What is this stuff?</A></NOBR>
};
    }
    print qq{
</TD>
</TR>
</TABLE>
    };
}
print qq{<HR>};




if (!$userid) {
    print qq{<INPUT TYPE="hidden" NAME="cmdtype" VALUE="doit">};
} else {
    print "
<BR>
<INPUT TYPE=radio NAME=cmdtype VALUE=doit CHECKED> Run this query
<BR>
";

    my @namedqueries;
    if ($userid) {
        SendSQL("SELECT name FROM namedqueries " .
                "WHERE userid = $userid AND name != '$::defaultqueryname' " .
                "ORDER BY name");
        while (MoreSQLData()) {
            push(@namedqueries, FetchOneColumn());
        }
    }
    
    
    
    
    if (@namedqueries) {
        my $namelist = make_options(\@namedqueries);
        print qq{
<table cellspacing=0 cellpadding=0><tr>
<td><INPUT TYPE=radio NAME=cmdtype VALUE=editnamed> Load the remembered query:</td>
<td rowspan=3><select name=namedcmd>$namelist</select>
</tr><tr>
<td><INPUT TYPE=radio NAME=cmdtype VALUE=runnamed> Run the remembered query:</td>
</tr><tr>
<td><INPUT TYPE=radio NAME=cmdtype VALUE=forgetnamed> Forget the remembered query:</td>
</tr></table>};
    }

    print "
<INPUT TYPE=radio NAME=cmdtype VALUE=asdefault> Remember this as the default query
<BR>
<INPUT TYPE=radio NAME=cmdtype VALUE=asnamed> Remember this query, and name it:
<INPUT TYPE=text NAME=newqueryname>
<BR>
"
}

print "
<NOBR><B>Sort By:</B>
<SELECT NAME=\"order\">
";

my $deforder = "'Importance'";
my @orders = ('Bug Number', $deforder, 'Assignee');

if ($::COOKIE{'LASTORDER'}) {
    $deforder = "Reuse same sort as last time";
    unshift(@orders, $deforder);
}

if ($::FORM{'order'}) { $deforder = $::FORM{'order'} }

my $defquerytype = $userdefaultquery ? "my" : "the";

print make_options(\@orders, $deforder);
print "</SELECT></NOBR>
<INPUT TYPE=\"submit\" VALUE=\"Submit query\">
<INPUT TYPE=\"reset\" VALUE=\"Reset back to $defquerytype default query\">
";

if ($userdefaultquery) {
    print qq{<BR><A HREF="query.cgi?nukedefaultquery=1">Set my default query back to the system default</A>};
}

print "
</FORM>
<P>Give me a <A HREF=\"help.html\">clue</A> about how to use this form.
<P>
";


if (UserInGroup("tweakparams")) {
    print "<a href=editparams.cgi>Edit Bugzilla operating parameters</a><br>\n";
}
if (UserInGroup("editcomponents")) {
    print "<a href=editproducts.cgi>Edit Bugzilla products and components</a><br>\n";
}
if (UserInGroup("editkeywords")) {
    print "<a href=editkeywords.cgi>Edit Bugzilla keywords</a><br>\n";
}
if ($userid) {
    print "<a href=relogin.cgi>Log in as someone besides <b>$::COOKIE{'Bugzilla_login'}</b></a><br>\n";
}
print "<a href=userprefs.cgi>Change your password or preferences.</a><br>\n";
print "<a href=\"enter_bug.cgi\">Report a new bug.</a><br>\n";
print "<a href=\"createaccount.cgi\">Open a new Bugzilla account</a><br>\n";
print "<a href=\"reports.cgi\">Bug reports</a><br>\n";

PutFooter();
