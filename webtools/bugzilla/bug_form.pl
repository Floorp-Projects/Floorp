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
#                 Dave Miller <justdave@syndicomm.com>

use diagnostics;
use strict;

use RelationSet;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub bug_form_pl_sillyness {
    my $zz;
    $zz = %::FORM;
    $zz = %::components;
    $zz = %::proddesc;
    $zz = %::prodmaxvotes;
    $zz = %::versions;
    $zz = @::legal_keywords;
    $zz = @::legal_opsys;
    $zz = @::legal_platform;
    $zz = @::legal_product;
    $zz = @::legal_priority;
    $zz = @::settable_resolution;
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
group by bugs.bug_id";

SendSQL($query);
my %bug;
my @row;
@row = FetchSQLData();
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

my $assignedtoid = $bug{'assigned_to'};
my $reporterid = $bug{'reporter'};
my $qacontactid =  $bug{'qa_contact'};

$bug{'assigned_to_email'} = DBID_to_name($assignedtoid);
$bug{'assigned_to'} = DBID_to_real_or_loginname($bug{'assigned_to'});
$bug{'reporter'} = DBID_to_real_or_loginname($bug{'reporter'});

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

my $platform_popup = make_options(\@::legal_platform, $bug{'rep_platform'});
my $priority_popup = make_options(\@::legal_priority, $bug{'priority'});
my $sev_popup = make_options(\@::legal_severity, $bug{'bug_severity'});


my $component_popup = make_options($::components{$bug{'product'}},
				   $bug{'component'});

my $ccSet = new RelationSet;
$ccSet->mergeFromDB("select who from cc where bug_id=$id");
my @ccList = $ccSet->toArrayOfStrings();
my $cc_element = "<INPUT TYPE=HIDDEN NAME=cc VALUE=\"\">";
if (scalar(@ccList) > 0) {
  $cc_element = "<SELECT NAME=cc MULTIPLE SIZE=5>\n";
  foreach my $ccName ( @ccList ) {
    $cc_element .= "<OPTION VALUE=\"$ccName\">$ccName\n";
  }
  $cc_element .= "</SELECT><BR>\n" .
        "<INPUT TYPE=CHECKBOX NAME=removecc>Remove selected CCs<br>\n";
}

my $URL = value_quote($bug{'bug_file_loc'});

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
    <TD ALIGN=RIGHT><B>Bug#:</B></TD><TD><A HREF=\"" . Param('urlbase') . "show_bug.cgi?id=$bug{'bug_id'}\">$bug{'bug_id'}</A></TD>
  <TD>&nbsp;</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#rep_platform\">Platform:</A></B></TD>
    <TD><SELECT NAME=rep_platform>$platform_popup</SELECT></TD>
  <TD>&nbsp;</TD>
    <TD ALIGN=RIGHT><B>Reporter:</B></TD><TD>$bug{'reporter'}</TD>
</TR><TR>
    <TD ALIGN=RIGHT><B>Product:</B></TD>
    <TD><SELECT NAME=product>" .
    make_options(\@::legal_product, $bug{'product'}) .
    "</SELECT></TD>
  <TD>&nbsp;</TD>
    <TD ALIGN=RIGHT><B>OS:</B></TD>
    <TD><SELECT NAME=op_sys>" .
    make_options(\@::legal_opsys, $bug{'op_sys'}) .
    "</SELECT></TD>
  <TD>&nbsp;</TD>
    <TD ALIGN=RIGHT NOWRAP><b>Add CC:</b></TD>
    <TD><INPUT NAME=newcc SIZE=30 VALUE=\"\"></TD>
</TR><TR>
    <TD ALIGN=RIGHT><B><A HREF=\"describecomponents.cgi?product=" .
    url_quote($bug{'product'}) . "\">Component:</A></B></TD>
      <TD><SELECT NAME=component>$component_popup</SELECT></TD>
  <TD>&nbsp;</TD>
    <TD ALIGN=RIGHT><B>Version:</B></TD>
    <TD><SELECT NAME=version>" .
    make_options($::versions{$bug{'product'}}, $bug{'version'}) .
    "</SELECT></TD>
  <TD>&nbsp;</TD>
    <TD ROWSPAN=4 ALIGN=RIGHT VALIGN=TOP><B>CC:</B></TD>
    <TD ROWSPAN=4 VALIGN=TOP> $cc_element </TD>
</TR><TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html\">Status:</A></B></TD>
      <TD>$bug{'bug_status'}</TD>
  <TD>&nbsp;</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#priority\">Priority:</A></B></TD>
      <TD><SELECT NAME=priority>$priority_popup</SELECT></TD>
  <TD>&nbsp;</TD>
</TR><TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html\">Resolution:</A></B></TD>
      <TD>$bug{'resolution'}</TD>
  <TD>&nbsp;</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#severity\">Severity:</A></B></TD>
      <TD><SELECT NAME=bug_severity>$sev_popup</SELECT></TD>
  <TD>&nbsp;</TD>
</TR><TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#assigned_to\">Assigned&nbsp;To:
        </A></B></TD>
      <TD>$bug{'assigned_to'}</TD>
  <TD>&nbsp;</TD>";

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
                     "</SELECT></TD>
  <TD>&nbsp;</TD>";
} else { print "<TD></TD><TD></TD><TD>&nbsp;</TD>"; }

print "
</TR>";

if (Param("useqacontact")) {
    my $name = $bug{'qa_contact'} > 0 ? DBID_to_name($bug{'qa_contact'}) : "";
    print "
  <TR>
    <TD ALIGN=\"RIGHT\"><B>QA Contact:</B>
    <TD COLSPAN=7>
      <INPUT NAME=qa_contact VALUE=\"" .
    value_quote($name) .
    "\" SIZE=60></TD>
  </TR>";
}


print "
  <TR>
    <TD ALIGN=\"RIGHT\">$URL
    <TD COLSPAN=7>
      <INPUT NAME=bug_file_loc VALUE=\"" . value_quote($bug{'bug_file_loc'}) . "\" SIZE=60></TD>
  </TR><TR>
    <TD ALIGN=\"RIGHT\"><B>Summary:</B>
    <TD COLSPAN=7>
      <INPUT NAME=short_desc VALUE=\"" .
    value_quote($bug{'short_desc'}) .
    "\" SIZE=60></TD>
  </TR>";

if (Param("usestatuswhiteboard")) {
    print "
  <TR>
    <TD ALIGN=\"RIGHT\"><B>Status Whiteboard:</B>
    <TD COLSPAN=7>
      <INPUT NAME=status_whiteboard VALUE=\"" .
    value_quote($bug{'status_whiteboard'}) .
    "\" SIZE=60></TD>
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
<TD ALIGN=right><B><A HREF="describekeywords.cgi">Keywords:</A></B>
<TD COLSPAN=7><INPUT NAME="keywords" VALUE="$value" SIZE=60></TD>
</TR>
};
}

# 2001-05-16 myk@mozilla.org: use the attachment tracker to display attachments
# if this installation has enabled use of the attachment tracker.
if (Param('useattachmenttracker')) {
    print "</table>\n";
    use Attachment;
    &Attachment::list($id);
} else {
    print "<tr><td align=right><B>Attachments:</b></td>\n";
    SendSQL("select attach_id, creation_ts, mimetype, description from attachments where bug_id = $id");
    while (MoreSQLData()) {
        my ($attachid, $date, $mimetype, $desc) = (FetchSQLData());
        if ($date =~ /^(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/) {
            $date = "$3/$4/$2 $5:$6";
        }
        my $link = "showattachment.cgi?attach_id=$attachid";
        $desc = value_quote($desc);
        print qq{<td><a href="$link">$date</a></td><td colspan=6>$desc&nbsp;&nbsp;&nbsp;($mimetype)</td></tr><tr><td></td>};
    }
    print "<td colspan=7><a href=\"createattachment.cgi?id=$id\">Create a new attachment</a> (proposed patch, testcase, etc.)</td></tr></table>\n";
}


sub EmitDependList {
    my ($desc, $myfield, $targetfield) = (@_);
    print "<th align=right>$desc:</th><td>";
    my @list;
    SendSQL("select $targetfield from dependencies where  
             $myfield = $id order by $targetfield");
    while (MoreSQLData()) {
        my ($i) = (FetchSQLData());
        push(@list, $i);
        print GetBugLink($i, $i);
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
<th><a href="votehelp.html">Votes:</a></th>
<td>
$bug{'votes'}&nbsp;&nbsp;&nbsp;
<a href="showvotes.cgi?bug_id=$id">Show votes for this bug</a>&nbsp;&nbsp;&nbsp;
<a href="showvotes.cgi?voteon=$id">Vote for this bug</a>
</td>
</tr></table>
};
}

print "
<br>
<B>Additional Comments:</B>
<BR>
<TEXTAREA WRAP=HARD NAME=comment ROWS=10 COLS=80></TEXTAREA><BR>";


if ($::usergroupset ne '0') {
    SendSQL("select bit, name, description, (bit & $bug{'groupset'} != 0) " .
	    "from groups where bit & $::usergroupset != 0 " .
	    "and isbuggroup != 0 " . 
            # Include active groups as well as inactive groups to which
            # the bug already belongs.  This way the bug can be removed
            # from an inactive group but can only be added to active ones.
            "and (isactive = 1 or (bit & $bug{'groupset'} != 0)) " . 
	    "order by description");
    # We only print out a header bit for this section if there are any
    # results.
    my $groupFound = 0;
    while (MoreSQLData()) {
      my ($bit, $name, $description, $ison) = (FetchSQLData());
      # For product groups, we only want to display the checkbox if either
      # (1) The bit is already set, or
      # (2) It's the group for this product.
      # All other product groups will be skipped.  Non-product bug groups
      # will still be displayed.
      if($ison || ($name eq $bug{'product'}) || (!defined $::proddesc{$name})) {
        if(!$groupFound) {
          print "<br><b>Only users in the selected groups can view this bug:</b><br>\n";
          print "<font size=\"-1\">(Leave all boxes unchecked to make this a public bug.)</font><br><br>\n";
          $groupFound = 1;
        }
        # Modifying this to use checkboxes instead
        my $checked = $ison ? " CHECKED" : "";
        # indent these a bit
        print "&nbsp;&nbsp;&nbsp;&nbsp;";
        print "<input type=checkbox name=\"bit-$bit\" value=1$checked>\n";
        print "$description<br>\n";
      }
    }

    # If the bug is restricted to a group, display checkboxes that allow
    # the user to set whether or not the reporter, assignee, QA contact, 
    # and cc list can see the bug even if they are not members of all 
    # groups to which the bug is restricted.
    if ( $bug{'groupset'} != 0 ) {
        # Determine whether or not the bug is always accessible by the reporter,
        # QA contact, and/or users on the cc: list.
        SendSQL("SELECT  reporter_accessible , assignee_accessible , 
                         qacontact_accessible , cclist_accessible
                 FROM    bugs
                 WHERE   bug_id = $id
                ");
        my ($reporter_accessible, $assignee_accessible, $qacontact_accessible, $cclist_accessible) = FetchSQLData();

        # Convert boolean data about which roles always have access to the bug
        # into "checked" attributes for the HTML checkboxes by which users
        # set and change these values.
        my $reporter_checked = $reporter_accessible ? " checked" : "";
        my $assignee_checked = $assignee_accessible ? " checked" : "";
        my $qacontact_checked = $qacontact_accessible ? " checked" : "";
        my $cclist_checked = $cclist_accessible ? " checked" : "";

        # Display interface for changing the values.
        print qq|
            <p>
            <b>But users in the roles selected below can always view this bug:</b><br>
            <small>(Does not take effect unless the bug is restricted to at least one group.)</small>
            </p>

            <p>
            <input type="checkbox" name="reporter_accessible" value="1" $reporter_checked>Reporter
            <input type="checkbox" name="assignee_accessible" value="1" $assignee_checked>Assignee
            <input type="checkbox" name="qacontact_accessible" value="1" $qacontact_checked>QA Contact
            <input type="checkbox" name="cclist_accessible" value="1" $cclist_checked>CC List
            </p>
        |;
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

my $movers = Param("movers");
$movers =~ s/\s?,\s?/|/g;
$movers =~ s/@/\@/g;

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
        my $resolution_popup = make_options(\@::settable_resolution,
                                            $bug{'resolution'});
        print "<INPUT TYPE=radio NAME=knob VALUE=resolve>
        Resolve bug, changing <A HREF=\"bug_status.html\">resolution</A> to
        <SELECT NAME=resolution
          ONCHANGE=\"document.changeform.knob\[$knum\].checked=true\">
          $resolution_popup</SELECT><br>\n";
        $knum++;
        print "<INPUT TYPE=radio NAME=knob VALUE=duplicate>
        Resolve bug, mark it as duplicate of bug # 
        <INPUT NAME=dup_id SIZE=6 ONCHANGE=\"if (this.value != '') {document.changeform.knob\[$knum\].checked=true}\"><br>\n";
        $knum++;
        my $assign_element = "<INPUT NAME=\"assigned_to\" SIZE=32 ONCHANGE=\"if ((this.value != ".SqlQuote($bug{'assigned_to_email'}) .") && (this.value != '')) { document.changeform.knob\[$knum\].checked=true; }\" VALUE=\"$bug{'assigned_to_email'}\">";

        print "<INPUT TYPE=radio NAME=knob VALUE=reassign> 
          <A HREF=\"bug_status.html#assigned_to\">Reassign</A> bug to
          $assign_element
        <br>\n";
        if ($status eq $::unconfirmedstate && ($canconfirm || $canedit)) {
            print "&nbsp;&nbsp;&nbsp;&nbsp;<INPUT TYPE=checkbox NAME=andconfirm> and confirm bug (change status to <b>NEW</b>)<BR>";
        }
        $knum++;
        print "<INPUT TYPE=radio NAME=knob VALUE=reassignbycomponent>
          Reassign bug to owner ";
        if (Param("useqacontact")) { print "and QA contact "; }
        print "of selected component<br>\n";
        if ($status eq $::unconfirmedstate && ($canconfirm || $canedit)) {
            print "&nbsp;&nbsp;&nbsp;&nbsp;<INPUT TYPE=checkbox NAME=compconfirm> and confirm bug (change status to <b>NEW</b>)<BR>";
        }
        $knum++;
    } elsif ( Param("move-enabled") && ($bug{'resolution'} eq "MOVED") ) {
        if ( (defined $::COOKIE{"Bugzilla_login"}) 
             && ($::COOKIE{"Bugzilla_login"} =~ /($movers)/) ){
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
<INPUT TYPE=\"hidden\" name=\"form_name\" VALUE=\"process_bug\">
<P>
<FONT size=\"+1\"><B>
 <A HREF=\"show_activity.cgi?id=$id\">View Bug Activity</A>
 &nbsp; | &nbsp;
 <A HREF=\"long_list.cgi?buglist=$id\">Format For Printing</A>
</B></FONT>
";

if ( Param("move-enabled") && (defined $::COOKIE{"Bugzilla_login"}) && ($::COOKIE{"Bugzilla_login"} =~ /($movers)/) ){
  print "&nbsp; <FONT size=\"+1\"><B> | </B></FONT> &nbsp;"
       ."<INPUT TYPE=\"SUBMIT\" NAME=\"action\" VALUE=\"" 
       . Param("move-button-text") . "\">\n";
}

print "<BR></FORM>";

print "
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
