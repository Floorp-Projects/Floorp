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

use diagnostics;
use strict;

use RelationSet;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub bug_form_pl_sillyness {
    my $zz;
    $zz = %::FORM;
    $zz = %::components;
    $zz = %::prodmaxvotes;
    $zz = %::versions;
    $zz = @::legal_keywords;
    $zz = @::legal_opsys;
    $zz = @::legal_platform;
    $zz = @::legal_product;
    $zz = @::legal_priority;
    $zz = @::legal_resolution_no_dup;
    $zz = @::legal_severity;
    $zz = %::target_milestone;
}

my $loginok = quietly_check_login();

my $id = $::FORM{'id'};

my $query = "
select
        bugs.bug_id,
        product,
        version,
        rep_platform,
        op_sys,
        bug_status,
        resolution,
        priority,
        bug_severity,
        component,
        assigned_to,
        reporter,
        bug_file_loc,
        short_desc,
	target_milestone,
	qa_contact,
	status_whiteboard,
        date_format(creation_ts,'%Y-%m-%d %H:%i'),
        groupset,
	delta_ts,
	sum(votes.count)
from bugs left join votes using(bug_id)
where bugs.bug_id = $id
and bugs.groupset & $::usergroupset = bugs.groupset
group by bugs.bug_id";

SendSQL($query);
my %bug;
my @row;
if (@row = FetchSQLData()) {
    my $count = 0;
    foreach my $field ("bug_id", "product", "version", "rep_platform",
		       "op_sys", "bug_status", "resolution", "priority",
		       "bug_severity", "component", "assigned_to", "reporter",
		       "bug_file_loc", "short_desc", "target_milestone",
                       "qa_contact", "status_whiteboard", "creation_ts",
                       "groupset", "delta_ts", "votes") {
	$bug{$field} = shift @row;
	if (!defined $bug{$field}) {
	    $bug{$field} = "";
	}
	$count++;
    }
} else {
    SendSQL("select groupset from bugs where bug_id = $id");
    if (@row = FetchSQLData()) {
        print "<H1>Permission denied.</H1>\n";
        if ($loginok) {
            print "Sorry; you do not have the permissions necessary to see\n";
            print "bug $id.\n";
        } else {
            print "Sorry; bug $id can only be viewed when logged\n";
            print "into an account with the appropriate permissions.  To\n";
            print "see this bug, you must first\n";
            print "<a href=\"show_bug.cgi?id=$id&GoAheadAndLogIn=1\">";
            print "log in</a>.";
        }
    } else {
        print "<H1>Bug not found</H1>\n";
        print "There does not seem to be a bug numbered $id.\n";
    }
    PutFooter();
    exit;
}

my $assignedtoid = $bug{'assigned_to'};
my $reporterid = $bug{'reporter'};
my $qacontactid =  $bug{'qa_contact'};

$bug{'assigned_to'} = DBID_to_name($bug{'assigned_to'});
$bug{'reporter'} = DBID_to_name($bug{'reporter'});

print qq{<FORM NAME="changeform" METHOD="POST" ACTION="process_bug.cgi">\n};

#  foreach my $i (sort(keys(%bug))) {
#      my $q = value_quote($bug{$i});
#      print qq{<INPUT TYPE="HIDDEN" NAME="orig-$i" VALUE="$q">\n};
#  }

$bug{'long_desc'} = GetLongDescriptionAsHTML($id);
my $longdesclength = length($bug{'long_desc'});

GetVersionTable();



#
# These should be read from the database ...
#

my $resolution_popup = make_options(\@::legal_resolution_no_dup,
				    $bug{'resolution'});
my $platform_popup = make_options(\@::legal_platform, $bug{'rep_platform'});
my $priority_popup = make_options(\@::legal_priority, $bug{'priority'});
my $sev_popup = make_options(\@::legal_severity, $bug{'bug_severity'});


my $component_popup = make_options($::components{$bug{'product'}},
				   $bug{'component'});

my $ccSet = new RelationSet;
$ccSet->mergeFromDB("select who from cc where bug_id=$id");
my $cc_element = '<INPUT NAME=cc SIZE=30 VALUE="' .
  $ccSet->toString() . '">';


my $URL = $bug{'bug_file_loc'};

if (defined $URL && $URL ne "none" && $URL ne "NULL" && $URL ne "") {
    $URL = "<B><A HREF=\"$URL\">URL:</A></B>";
} else {
    $URL = "<B>URL:</B>";
}

print "
<INPUT TYPE=HIDDEN NAME=\"delta_ts\" VALUE=\"$bug{'delta_ts'}\">
<INPUT TYPE=HIDDEN NAME=\"longdesclength\" VALUE=\"$longdesclength\">
<INPUT TYPE=HIDDEN NAME=\"id\" VALUE=$id>
  <TABLE CELLSPACING=0 CELLPADDING=0 BORDER=0><TR>
    <TD ALIGN=RIGHT><B>Bug#:</B></TD><TD><A HREF=\"show_bug.cgi?id=$bug{'bug_id'}\">$bug{'bug_id'}</A></TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#rep_platform\">Platform:</A></B></TD>
    <TD><SELECT NAME=rep_platform>$platform_popup</SELECT></TD>
    <TD ALIGN=RIGHT><B>Version:</B></TD>
    <TD><SELECT NAME=version>" .
    make_options($::versions{$bug{'product'}}, $bug{'version'}) .
    "</SELECT></TD>
  </TR><TR>
    <TD ALIGN=RIGHT><B>Product:</B></TD>
    <TD><SELECT NAME=product>" .
    make_options(\@::legal_product, $bug{'product'}) .
    "</SELECT></TD>
    <TD ALIGN=RIGHT><B>OS:</B></TD>
    <TD><SELECT NAME=op_sys>" .
    make_options(\@::legal_opsys, $bug{'op_sys'}) .
    "</SELECT><TD ALIGN=RIGHT><B>Reporter:</B></TD><TD>$bug{'reporter'}</TD>
  </TR><TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html\">Status:</A></B></TD>
      <TD>$bug{'bug_status'}</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#priority\">Priority:</A></B></TD>
      <TD><SELECT NAME=priority>$priority_popup</SELECT></TD>
    <TD ALIGN=RIGHT><B>Cc:</B></TD>
      <TD> $cc_element </TD>
  </TR><TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html\">Resolution:</A></B></TD>
      <TD>$bug{'resolution'}</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#severity\">Severity:</A></B></TD>
      <TD><SELECT NAME=bug_severity>$sev_popup</SELECT></TD>
    <TD ALIGN=RIGHT><B><A HREF=\"describecomponents.cgi?product=" .
    url_quote($bug{'product'}) . "\">Component:</A></B></TD>
      <TD><SELECT NAME=component>$component_popup</SELECT></TD>
  </TR><TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#assigned_to\">Assigned&nbsp;To:
        </A></B></TD>
      <TD>$bug{'assigned_to'}</TD>";

if (Param("usetargetmilestone")) {
    my $url = "";
    if (defined $::milestoneurl{$bug{'product'}}) {
        $url = $::milestoneurl{$bug{'product'}};
    }
    if ($url eq "") {
        $url = "notargetmilestone.html";
    }
    if ($bug{'target_milestone'} eq "") {
        $bug{'target_milestone'} = " ";
    }
    print "
<TD ALIGN=RIGHT><A href=\"$url\"><B>Target Milestone:</B></A></TD>
<TD><SELECT NAME=target_milestone>" .
    make_options($::target_milestone{$bug{'product'}},
                 $bug{'target_milestone'}) .
                     "</SELECT></TD>";
}

print "
</TR>";

if (Param("useqacontact")) {
    my $name = $bug{'qa_contact'} > 0 ? DBID_to_name($bug{'qa_contact'}) : "";
    print "
  <TR>
    <TD ALIGN=\"RIGHT\"><B>QA Contact:</B>
    <TD COLSPAN=6>
      <INPUT NAME=qa_contact VALUE=\"" .
    value_quote($name) .
    "\" SIZE=60></
  </TR>";
}


print "
  <TR>
    <TD ALIGN=\"RIGHT\">$URL
    <TD COLSPAN=6>
      <INPUT NAME=bug_file_loc VALUE=\"$bug{'bug_file_loc'}\" SIZE=60></TD>
  </TR><TR>
    <TD ALIGN=\"RIGHT\"><B>Summary:</B>
    <TD COLSPAN=6>
      <INPUT NAME=short_desc VALUE=\"" .
    value_quote($bug{'short_desc'}) .
    "\" SIZE=60></TD>
  </TR>";

if (Param("usestatuswhiteboard")) {
    print "
  <TR>
    <TD ALIGN=\"RIGHT\"><B>Status Whiteboard:</B>
    <TD COLSPAN=6>
      <INPUT NAME=status_whiteboard VALUE=\"" .
    value_quote($bug{'status_whiteboard'}) .
    "\" SIZE=60></
  </TR>";
}

if (@::legal_keywords) {
    SendSQL("SELECT keyworddefs.name 
             FROM keyworddefs, keywords
             WHERE keywords.bug_id = $id AND keyworddefs.id = keywords.keywordid
             ORDER BY keyworddefs.name");
    my @list;
    while (MoreSQLData()) {
        push(@list, FetchOneColumn());
    }
    my $value = value_quote(join(', ', @list));
    print qq{
<TR>
<TD ALIGN=right><B><A HREF="describekeywords.cgi">Keywords</A>:</B>
<TD COLSPAN=6><INPUT NAME="keywords" VALUE="$value" SIZE=60></TD>
</TR>
};
}

print "<tr><td align=right><B>Attachments:</b></td>\n";
SendSQL("select attach_id, creation_ts, description from attachments where bug_id = $id");
while (MoreSQLData()) {
    my ($attachid, $date, $desc) = (FetchSQLData());
    if ($date =~ /^(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/) {
        $date = "$3/$4/$2 $5:$6";
    }
    my $link = "showattachment.cgi?attach_id=$attachid";
    $desc = value_quote($desc);
    print qq{<td><a href="$link">$date</a></td><td colspan=4>$desc</td></tr><tr><td></td>};
}
print "<td colspan=6><a href=\"createattachment.cgi?id=$id\">Create a new attachment</a> (proposed patch, testcase, etc.)</td></tr></table>\n";


sub EmitDependList {
    my ($desc, $myfield, $targetfield) = (@_);
    print "<th align=right>$desc:</th><td>";
    my @list;
    SendSQL("select dependencies.$targetfield, bugs.bug_status
 from dependencies, bugs
 where dependencies.$myfield = $id
   and bugs.bug_id = dependencies.$targetfield
 order by dependencies.$targetfield");
    while (MoreSQLData()) {
        my ($i, $stat) = (FetchSQLData());
        push(@list, $i);
        my $opened = ($stat eq "NEW" || $stat eq "ASSIGNED" ||
                      $stat eq "REOPENED");
        if (!$opened) {
            print "<strike>";
        }
        print qq{<a href="show_bug.cgi?id=$i">$i</a>};
        if (!$opened) {
            print "</strike>";
        }
        print " ";
    }
    print "</td><td><input name=$targetfield value=\"" .
        join(',', @list) . "\"></td>\n";
}

if (Param("usedependencies")) {
    print "<table><tr>\n";
    EmitDependList("Bug $id depends on", "blocked", "dependson");
    print qq{
<td rowspan=2><a href="showdependencytree.cgi?id=$id">Show dependency tree</a>
};
    if (Param("webdotbase") ne "") {
        print qq{
<br><a href="showdependencygraph.cgi?id=$id">Show dependency graph</a>
};
    }
    print "</td></tr><tr>";
    EmitDependList("Bug $id blocks", "dependson", "blocked");
    print "</tr></table>\n";
}

if ($::prodmaxvotes{$bug{'product'}}) {
    print qq{
<table><tr>
<th><a href="votehelp.html">Votes</a> for bug $id:</th><td>
<a href="showvotes.cgi?bug_id=$id">$bug{'votes'}</a>
&nbsp;&nbsp;&nbsp;<a href="showvotes.cgi?voteon=$id">Vote for this bug</a>
</td></tr></table>
};
}

print "
<br>
<B>Additional Comments:</B>
<BR>
<TEXTAREA WRAP=HARD NAME=comment ROWS=5 COLS=80></TEXTAREA><BR>";


if ($::usergroupset ne '0') {
    SendSQL("select bit, description, (bit & $bug{'groupset'} != 0) from groups where bit & $::usergroupset != 0 and isbuggroup != 0 order by bit");
    while (MoreSQLData()) {
        my ($bit, $description, $ison) = (FetchSQLData());
        my $check0 = !$ison ? " SELECTED" : "";
        my $check1 = $ison ? " SELECTED" : "";
        print "<select name=bit-$bit><option value=0$check0>\n";
        print "People not in the \"$description\" group can see this bug\n";
        print "<option value=1$check1>\n";
        print "Only people in the \"$description\" group can see this bug\n";
        print "</select><br>\n";
    }
}

print "<br>
<INPUT TYPE=radio NAME=knob VALUE=none CHECKED>
        Leave as <b>$bug{'bug_status'} $bug{'resolution'}</b><br>";


# knum is which knob number we're generating, in javascript terms.

my $knum = 1;

my $status = $bug{'bug_status'};

# In the below, if the person hasn't logged in ($::userid == 0), then
# we treat them as if they can do anything.  That's because we don't
# know why they haven't logged in; it may just be because they don't
# use cookies.  Display everything as if they have all the permissions
# in the world; their permissions will get checked when they log in
# and actually try to make the change.

my $canedit = UserInGroup("editbugs") || ($::userid == 0);
my $canconfirm;

if ($status eq $::unconfirmedstate) {
    $canconfirm = UserInGroup("canconfirm") || ($::userid == 0);
    if ($canedit || $canconfirm) {
        print "<INPUT TYPE=radio NAME=knob VALUE=confirm>";
	print "Confirm bug (change status to <b>NEW</b>)<br>";
        $knum++;
    }
}


if ($canedit || $::userid == $assignedtoid ||
      $::userid == $reporterid || $::userid == $qacontactid) {
    if (IsOpenedState($status)) {
        if ($status ne "ASSIGNED") {
            print "<INPUT TYPE=radio NAME=knob VALUE=accept>";
            my $extra = "";
            if ($status eq $::unconfirmedstate && ($canconfirm || $canedit)) {
                $extra = "confirm bug, ";
            }
            print "Accept bug (${extra}change status to <b>ASSIGNED</b>)<br>";
            $knum++;
        }
        if ($bug{'resolution'} ne "") {
            print "<INPUT TYPE=radio NAME=knob VALUE=clearresolution>\n";
            print "Clear the resolution (remove the current resolution of\n";
            print "<b>$bug{'resolution'}</b>)<br>\n";
            $knum++;
        }
        print "<INPUT TYPE=radio NAME=knob VALUE=resolve>
        Resolve bug, changing <A HREF=\"bug_status.html\">resolution</A> to
        <SELECT NAME=resolution
          ONCHANGE=\"document.changeform.knob\[$knum\].checked=true\">
          $resolution_popup</SELECT><br>\n";
        $knum++;
        print "<INPUT TYPE=radio NAME=knob VALUE=duplicate>
        Resolve bug, mark it as duplicate of bug # 
        <INPUT NAME=dup_id SIZE=6 ONCHANGE=\"document.changeform.knob\[$knum\].checked=true\"><br>\n";
        $knum++;
        my $assign_element = "<INPUT NAME=\"assigned_to\" SIZE=32 ONCHANGE=\"document.changeform.knob\[$knum\].checked=true\" VALUE=\"$bug{'assigned_to'}\">";

        print "<INPUT TYPE=radio NAME=knob VALUE=reassign> 
          <A HREF=\"bug_status.html#assigned_to\">Reassign</A> bug to
          $assign_element
        <br>\n";
        if ($status eq $::unconfirmedstate && ($canconfirm || $canedit)) {
            print "&nbsp;&nbsp;&nbsp;&nbsp;<INPUT TYPE=checkbox NAME=andconfirm> and confirm bug (change status to <b>NEW</b>)<BR>";
        }
        $knum++;
        print "<INPUT TYPE=radio NAME=knob VALUE=reassignbycomponent>
          Reassign bug to owner of selected component<br>\n";
        $knum++;
    } else {
        print "<INPUT TYPE=radio NAME=knob VALUE=reopen> Reopen bug<br>\n";
        $knum++;
        if ($status eq "RESOLVED") {
            print "<INPUT TYPE=radio NAME=knob VALUE=verify>
        Mark bug as <b>VERIFIED</b><br>\n";
            $knum++;
        }
        if ($status ne "CLOSED") {
            print "<INPUT TYPE=radio NAME=knob VALUE=close>
        Mark bug as <b>CLOSED</b><br>\n";
            $knum++;
        }
    }
}
 
print "
<INPUT TYPE=\"submit\" VALUE=\"Commit\">
<INPUT TYPE=\"reset\" VALUE=\"Reset\">
<INPUT TYPE=hidden name=form_name VALUE=process_bug>
<BR>
<FONT size=\"+1\"><B>
 <A HREF=\"show_activity.cgi?id=$id\">View Bug Activity</A>
 <A HREF=\"long_list.cgi?buglist=$id\">Format For Printing</A>
</B></FONT><BR>
</FORM>
<table><tr><td align=left><B>Description:</B></td>
<td align=right width=100%>Opened: $bug{'creation_ts'}</td></tr></table>
<HR>
";
print $bug{'long_desc'};
print "
<HR>\n";

# To add back option of editing the long description, insert after the above
# long_list.cgi line:
#  <A HREF=\"edit_desc.cgi?id=$id\">Edit Long Description</A>

navigation_header();

PutFooter();

1;
