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


# This routine quoteUrls contains inspirations from the HTML::FromText CPAN
# module by Gareth Rees <garethr@cre.canon.co.uk>.  It has been heavily hacked,
# all that is really recognizable from the original is bits of the regular
# expressions.

sub quoteUrls {
    my $text = shift;		# Take a copy; don't modify in-place.
    return $text unless $text;

    my $protocol = join '|',
    qw(afs cid ftp gopher http https mid news nntp prospero telnet wais);

    my %options = ( metachars => 1, @_ );

    my $count = 0;

    # Now, quote any "#" characters so they won't confuse stuff later
    $text =~ s/#/%#/g;

    # Next, find anything that looks like a URL or an email address and
    # pull them out the the text, replacing them with a "##<digits>##
    # marker, and writing them into an array.  All this confusion is
    # necessary so that we don't match on something we've already replaced,
    # which can happen if you do multiple s///g operations.

    my @things;
    while ($text =~ s%((mailto:)?([\w\.\-\+\=]+\@\w+(?:\.\w+)+)\b|
                       (\b((?:$protocol):\S+[\w/])))%"##$count##"%exo) {
        my $item = $&;

        $item = value_quote($item);

        if ($item !~ m/^$protocol:/o && $item !~ /^mailto:/) {
            # We must have grabbed this one because it looks like an email
            # address.
            $item = qq{<A HREF="mailto:$item">$item</A>};
        } else {
            $item = qq{<A HREF="$item">$item</A>};
        }

        $things[$count++] = $item;
    }
    while ($text =~ s/\bbug(\s|%\#)*(\d+)/"##$count##"/ei) {
        my $item = $&;
        my $num = $2;
        $item = value_quote($item); # Not really necessary, since we know
                                # there's no special chars in it.
        my $base = Param('urlbase');
        $item = qq{<A HREF="${base}show_bug.cgi?id=$num">$item</A>};
        $things[$count++] = $item;
    }

    $text = value_quote($text);

    # Stuff everything back from the array.
    for (my $i=0 ; $i<$count ; $i++) {
        $text =~ s/##$i##/$things[$i]/e;
    }

    # And undo the quoting of "#" characters.
    $text =~ s/%#/#/g;

    return $text;
}

quietly_check_login();

my $query = "
select
        bug_id,
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
        date_format(creation_ts,'Y-m-d'),
        groupset
from bugs
where bug_id = $::FORM{'id'}
and bugs.groupset & $::usergroupset = bugs.groupset";

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
                       "groupset") {
	$bug{$field} = shift @row;
	if (!defined $bug{$field}) {
	    $bug{$field} = "";
	}
	$count++;
    }
} else {
    my $maintainer = Param("maintainer");
    print "<TITLE>Bug Splat Error</TITLE>\n";
    print "<H1>Query Error</H1>Somehow something went wrong.  Possibly if\n";
    print "you mail this page to $maintainer, he will be able to fix\n";
    print "things.<HR>\n";
    print "Bug $::FORM{'id'} not found<H2>Query Text</H2><PRE>$query<PRE>\n";
    exit 0
}

$bug{'assigned_to'} = DBID_to_name($bug{'assigned_to'});
$bug{'reporter'} = DBID_to_name($bug{'reporter'});
$bug{'long_desc'} = GetLongDescription($::FORM{'id'});


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

my $cc_element = '<INPUT NAME=cc SIZE=30 VALUE="' .
    ShowCcList($::FORM{'id'}) . '">';


my $URL = $bug{'bug_file_loc'};

if (defined $URL && $URL ne "none" && $URL ne "NULL" && $URL ne "") {
    $URL = "<B><A HREF=\"$URL\">URL:</A></B>";
} else {
    $URL = "<B>URL:</B>";
}

print "
<HEAD><TITLE>Bug $::FORM{'id'} -- " . html_quote($bug{'short_desc'}) .
    "</TITLE></HEAD><BODY>
<FORM NAME=changeform METHOD=POST ACTION=\"process_bug.cgi\">
<INPUT TYPE=HIDDEN NAME=\"id\" VALUE=$::FORM{'id'}>
<INPUT TYPE=HIDDEN NAME=\"was_assigned_to\" VALUE=\"$bug{'assigned_to'}\">
  <TABLE CELLSPACING=0 CELLPADDING=0 BORDER=0><TR>
    <TD ALIGN=RIGHT><B>Bug#:</B></TD><TD>$bug{'bug_id'}</TD>
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
    push(@::legal_target_milestone, " ");
    print "
<TD ALIGN=RIGHT><A href=\"$url\"><B>Target Milestone:</B></A></TD>
<TD><SELECT NAME=target_milestone>" .
    make_options(\@::legal_target_milestone,
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


print "
</TABLE>
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

if ($status eq "NEW" || $status eq "ASSIGNED" || $status eq "REOPENED") {
    if ($status ne "ASSIGNED") {
        print "<INPUT TYPE=radio NAME=knob VALUE=accept>";
	print "Accept bug (change status to <b>ASSIGNED</b>)<br>";
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
    my $assign_element = "<INPUT NAME=assigned_to SIZE=32 ONCHANGE=\"document.changeform.knob\[$knum\].checked=true\" VALUE=$bug{'assigned_to'}>";

    print "<INPUT TYPE=radio NAME=knob VALUE=reassign> 
          <A HREF=\"bug_status.html#assigned_to\">Reassign</A> bug to
          $assign_element
        <br>\n";
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
 
print "
<INPUT TYPE=\"submit\" VALUE=\"Commit\">
<INPUT TYPE=\"reset\" VALUE=\"Reset\">
<INPUT TYPE=hidden name=form_name VALUE=process_bug>
<BR>
<FONT size=\"+1\"><B>
 <A HREF=\"show_activity.cgi?id=$::FORM{'id'}\">View Bug Activity</A>
 <A HREF=\"long_list.cgi?buglist=$::FORM{'id'}\">Format For Printing</A>
</B></FONT><BR>
</FORM>
<table><tr><td align=left><B>Description:</B></td><td width=100%>&nbsp;</td>
<td align=right>Opened:&nbsp;$bug{'creation_ts'}</td></tr></table>
<HR>
<PRE>
";
print quoteUrls($bug{'long_desc'}, email=>1, urls=>1);
print "
</PRE>
<HR>\n";

# To add back option of editing the long description, insert after the above
# long_list.cgi line:
#  <A HREF=\"edit_desc.cgi?id=$::FORM{'id'}\">Edit Long Description</A>


navigation_header();

print "</BODY>\n";

1;
