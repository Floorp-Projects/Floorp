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
# Contributor(s):         Brian Bober <boberb@rpi.edu>
#			  Terry Weissman <terry@mozilla.org>
#			  Tara Hernandez <tara@tequilarista.org>

use vars %::FORM;

use diagnostics;
use strict;

require "CGI.pl";

ConnectToDatabase();
GetVersionTable();

print "Content-type: text/html\n\n";

my $product = $::FORM{'product'};

PutHeader("Bugzilla Query Page Help","Help", "This page is to help you learn how to use the query form.");





print qq{

<br>

<form action="none"> <!-- Cause NS4.x is stupid. Die NS4.x you eeeevil eeeevil program! -->

<a name="top"></a>

<p><center><b><font size="+2">Help Using The Bugzilla Query Form</font></b><br>January, 20 2001 - <a href="mailto:netdemonz\@yahoo.com">Brian Bober (netdemon)</a></center>

<br><center><img width="329" height="220" src="ant.jpg" border="2" alt="Da Ant"></center>

<p><b>Notice:</b> This was written for the new query page that will probably be added in a version later than Bugzilla 2.12 in order to fix some regressions. Therefore, the layout of the forms shown in captions might not be exactly as they appear on the query page. The number of fields, though, is the same and you should be able to locate the fields on the current query.cgi without much problem.

<p><br><center><h3>The Sections</h3></center>

<p>The query page is broken down into the following sections:

<p><a href="#bugsettings">Bug Settings</a> 
<br><a href="#peopleinvolved">People Involved</a> 
<br><a href="#textsearch">Text Search Options</a>
<br><a href="#moduleoptions">Module Options</a> 
<br><a href="#advancedquerying">Advanced Querying</a>
<br><a href="#therest">The Bottom Of The Form</a>

<p>&quot;I already know how to use <a href="http://www.mozilla.org/bugs/">Bugzilla</a>, but would like <a href="#info">information</a> about Bugzilla and the author of this document.&quot;
<br>&quot;Ok, I am almost certain the bug I discovered isn't in Bugzilla, how do I <a href="enter_bug.cgi">submit</a> the bug?&quot; - <a href= "http://www.mozilla.org/quality/bug-writing-guidelines.html">Read the guidelines first</a>!

<p><br><center><h3>Tips</h3></center>
You don't have to fill out any field on the query page you don't need. Filling out fields will limit
your search. On the list boxes, such as Status, you can Ctrl-Click to unselect an option. 
Until you get better, you can use the "brute force" method where you enter a very simple query and search through 
the long list of bugs manually. Just try not to overuse this method if you don't have to as you might be slowing down
the search for other people if there are many people searching at the same time. Finally, I would recommend learning the Boolean Chart immediately because it is extremely 
powerful. Also, there is a navigation bar at the <a href="#bottom">bottom</a> of 
most Bugzilla pages, and important links at the <a href="index.html">front page</a>.



<p>Back to the <a href="query.cgi">Query</a>. If you typed anything in the forms already, you might want to hit back on the
browser. When you are all done reading, do a <a href="#samplequery">sample query</a>!

};





print qq{

<a name="bugsettings"></a>

<p><br><center><h3>Bug Settings</h3></center>

<center>

<table width="700" bgcolor="#00afff" border="0" cellpadding="0" cellspacing="0">
<tr>
<td align="center" height="200">
<table cellspacing="0">
<tr>
<th align="left"><A HREF="queryhelp.cgi#status">Status</a>:</th>
<th align="left"><A HREF="queryhelp.cgi#resolution">Resolution</a>:</th>
<th align="left"><A HREF="queryhelp.cgi#platform">Platform</a>:</th>
<th align="left"><A HREF="queryhelp.cgi#opsys">OpSys</a>:</th>
<th align="left"><A HREF="queryhelp.cgi#priority">Priority</a>:</th>
<th align="left"><A HREF="queryhelp.cgi#severity">Severity</a>:</th>
</tr>
<tr>
<td align="left" valign="top">

<SELECT NAME="bug_status" MULTIPLE SIZE="7">
<OPTION VALUE="UNCONFIRMED">UNCONFIRMED<OPTION VALUE="NEW">NEW<OPTION VALUE="ASSIGNED">ASSIGNED<OPTION VALUE="REOPENED">REOPENED<OPTION VALUE="RESOLVED">RESOLVED<OPTION VALUE="VERIFIED">VERIFIED<OPTION VALUE="CLOSED">CLOSED</SELECT>

</td>
<td align="left" valign="top">
<SELECT NAME="resolution" MULTIPLE SIZE="7">
<OPTION VALUE="FIXED">FIXED<OPTION VALUE="INVALID">INVALID<OPTION VALUE="WONTFIX">WONTFIX<OPTION VALUE="LATER">LATER<OPTION VALUE="REMIND">REMIND<OPTION VALUE="DUPLICATE">DUPLICATE<OPTION VALUE="WORKSFORME">WORKSFORME<OPTION VALUE="MOVED">MOVED<OPTION VALUE="---">---</SELECT>

</td>
<td align="left" valign="top">
<SELECT NAME="rep_platform" MULTIPLE SIZE="7">
<OPTION VALUE="All">All<OPTION VALUE="DEC">DEC<OPTION VALUE="HP">HP<OPTION VALUE="Macintosh">Macintosh<OPTION VALUE="PC">PC<OPTION VALUE="SGI">SGI<OPTION VALUE="Sun">Sun<OPTION VALUE="Other">Other</SELECT>

</td>
<td align="left" valign="top">
<SELECT NAME="op_sys" MULTIPLE SIZE="7">
<OPTION VALUE="All">All<OPTION VALUE="Windows 3.1">Windows 3.1<OPTION VALUE="Windows 95">Windows 95<OPTION VALUE="Windows 98">Windows 98<OPTION VALUE="Windows ME">Windows ME<OPTION VALUE="Windows 2000">Windows 2000<OPTION VALUE="Windows NT">Windows NT<OPTION VALUE="Mac System 7">Mac System 7<OPTION VALUE="Mac System 7.5">Mac System 7.5<OPTION VALUE="Mac System 7.6.1">Mac System 7.6.1<OPTION VALUE="Mac System 8.0">Mac System 8.0<OPTION VALUE="Mac System 8.5">Mac System 8.5<OPTION VALUE="Mac System 8.6">Mac System 8.6<OPTION VALUE="Mac System 9.0">Mac System 9.0<OPTION VALUE="Linux">Linux<OPTION VALUE="BSDI">BSDI<OPTION VALUE="FreeBSD">FreeBSD<OPTION VALUE="NetBSD">NetBSD<OPTION VALUE="OpenBSD">OpenBSD<OPTION VALUE="AIX">AIX<OPTION VALUE="BeOS">BeOS<OPTION VALUE="HP-UX">HP-UX<OPTION VALUE="IRIX">IRIX<OPTION VALUE="Neutrino">Neutrino<OPTION VALUE="OpenVMS">OpenVMS<OPTION VALUE="OS/2">OS/2<OPTION VALUE="OSF/1">OSF/1<OPTION VALUE="Solaris">Solaris<OPTION VALUE="SunOS">SunOS<OPTION VALUE="other">other</SELECT>

</td>
<td align="left" valign="top">
<SELECT NAME="priority" MULTIPLE SIZE="7">
<OPTION VALUE="P1">P1<OPTION VALUE="P2">P2<OPTION VALUE="P3">P3<OPTION VALUE="P4">P4<OPTION VALUE="P5">P5</SELECT>

</td>
<td align="left" valign="top">
<SELECT NAME="bug_severity" MULTIPLE SIZE="7">
<OPTION VALUE="blocker">blocker<OPTION VALUE="critical">critical<OPTION VALUE="major">major<OPTION VALUE="normal">normal<OPTION VALUE="minor">minor<OPTION VALUE="trivial">trivial<OPTION VALUE="enhancement">enhancement</SELECT>
</td>
</tr>
</table>
</td>
</tr>
</table>
</center>

<br>

The <b>status</b> and <b>resolution</b> field define and track the
life cycle of a bug. <b>Platform</b> and <b>opsys</b> describe the system
which the bug is on. <b>Priority</b> and <b>Severity</b> are for tracking purposes.

<a name="status"></a>
<p><b>Status</b> 

<ul>
<li><b>UNCONFIRMED</b> - Nobody has validated that this bug needs to be fixed.  Users who have the correct
permissions may confirm this bug, changing its state to NEW. You can view 
<a href="userprefs.cgi?bank=permissions">your permissions</a> here.
A bug may be directly resolved and marked RESOLVED but usually a bug will 
be confirmed by the person to whom it is assigned. Usually, an UNCOMFIRMED bug
will be left uncomfirmed until someone has verified that the bug the reporter
submitted actually occurrs.

<li><b>NEW</b> - This bug has recently been added to the assignee's list of bugs
and must be processed. Bugs in this state may be accepted, and
become ASSIGNED, passed on to someone else, and remain
NEW, or resolved and marked RESOLVED.

<li><b>ASSIGNED</b> - This bug is not yet resolved, but is assigned to someone who
thinks they can fix it. From here bugs can be given to another person and become
NEW, or resolved and become RESOLVED.

<li><b>REOPENED</b> - The bug was once resolved, but the resolution was deemed
incorrect.  For example, a WORKSFORME bug is
REOPENED when more information shows up and the bug is now
reproducible.  From here bugs are either marked ASSIGNED
or RESOLVED.

<li><b>RESOLVED</b> - A resolution has been made, and it is awaiting verification by
the QA. From here bugs are either re-opened and become REOPENED, are marked 
VERIFIED, or are closed for good and marked CLOSED.

<li><b>VERIFIED</b>- QA has looked at the bug and the resolution and agrees that the
appropriate action has been taken.

<li><b>CLOSED</b> - The bug is considered dead, the resolution is correct, and the product the bug
has been reported against is terminated or shipped. Any zombie
bugs who choose to walk the earth again must do so by becoming
REOPENED.  This state is rarely ever used.
</ul>

<a name="resolution"></a>
<p><b>Resolution</b> 

<p>The <b>resolution</b> field indicates what happened to this bug.

<p>No resolution yet: All bugs which are in one of the "open" states (meaning the state
has no associated resolution) 
have the resolution set to blank. All other bugs
will be marked with one of the following resolutions.

<ul>
<li><b>FIXED</b> - A fix for this bug is checked into the tree and tested.
<li><b>INVALID</b> - The problem described is not a bug 
<li><b>WONTFIX</b> - The problem described is a bug which will never be fixed.
<li><b>LATER</b> - The problem described is a bug which will not be fixed in this
version of the product.
<li><b>REMIND</b> - The problem described is a bug which will probably not be fixed in this
version of the product, but might still be.
<li><b>DUPLICATE</b> - The problem is a duplicate of an existing bug. Marking a bug
duplicate requires the bug number of the duplicate and that number will be placed in the 
bug description.
<li><b>WORKSFORME</b> - All attempts at reproducing this bug were futile, reading the
code produces no clues as to why this behavior would occur. If
more information appears later, please re-assign the bug, for
now, file it.
</ul>

Note: The LATER and REMIND resolutions are no longer used on bugzilla.mozilla.org and many
installations are phasing them out.

<a name="platform"></a>
<p><b>Platform</b>
<p>The <b>platform</b> field is the hardware platform against which the bug was reported.  Legal
platforms include but are not limited to:

<ul>
<li>All (happens on all platform; cross-platform bug)<br>
<li>Macintosh
<li>PC
<li>Sun
<li>HP
</ul>
<p><b>Note:</b> Selecting the option "All" does not select bugs assigned against all platforms.  It
merely selects bugs that <b>occur</b> on all platforms.

<a name="opsys"></a>
<p><b>Operating System</b>
<p>The <b>operating system</b> field is the operating system against which the bug was reported.  Legal
operating systems include but are not limited to:

<ul>
<li>All (happens on all operating systems; cross-platform bug)
<li>Windows 95
<li>Windows 2000
<li>Mac System 8.0
<li>Linux
<li>Other (Not in any of these OSes)<br>
</ul>

<p>Note that the operating system implies the platform, but not always.
For example, Linux can run on PC and Macintosh and others.

<a name="priority"></a>
<p><b>Priority</b>

<p>The <b>priority</b> field describes the importance and order in which a bug should be
fixed.  This field is utilized by the programmers/engineers to
prioritize their work.  The priorities are from P1 (Most important) to P5
(Least important).

<a name="severity"></a>
<p><b>Severity</b>

<p>The <b>Severity</b> field describes the impact of a bug.

<ul>
<li><b>Blocker</b> - Blocks development and/or testing work.<br>
<li><b>Critical</b> - Crashes, loss of data, severe memory leak.<br>
<li><b>Major</b> - Major loss of function.<br>
<li><b>Normal</b> - This is the run of the mill bug.<br>
<li><b>Minor</b> - Minor loss of function, or other problem where an easy workaround is present.<br>
<li><b>Trivial</b> - Cosmetic problem like misspelled words or misaligned text.<br>
<li><b>Enhancement</b> - Request for enhancement.<br>
</ul>

};












print qq{
<a name="peopleinvolved"></a>
<p><br><center><h3>People Involved</h3></center>
<center>


<table width="390" bgcolor="#00afff" border="0" cellpadding="0" cellspacing="0">
<tr>
<td height="180" align="center">

<table>
<tr>
<td valign="middle">Email:
<input name="email1" size="25" value="">&nbsp;</td><td valign="top">matching as:<br>
<SELECT NAME="emailtype1"><OPTION VALUE="regexp">regexp
<OPTION VALUE="notregexp">not regexp
<OPTION VALUE="substring">substring
<OPTION VALUE="exact">exact
</SELECT>
</td>
</tr>
<tr>
<td colspan="2" align="center">Will match any of the following selected fields:</td>
</tr>
<tr>
<td colspan=2>
<center>
<input type="checkbox" name="emailassigned_to1" value="1">Assigned To
<input type="checkbox" name="emailreporter1" value="1">Reporter

<input type="checkbox" name="emailqa_contact1" value="1">QA Contact
</center>
</td>
</tr>
<tr>
<td colspan=2 align="center">
<input type="checkbox" name="emailcc1" value="1">CC
<input type="checkbox" name="emaillongdesc1" value="1">Added comment
</td>
</tr>
</table>

</td>
</tr>
</table>
</center>
<br>

This section has been made more complicated in order to make it more powerful. Unfortunately, 
it is not the easiest to understand. What this section lets you do is search for bugs associated
with a certain email address.

<p>

To search for bugs associated with an email address:

<ul>
  <li> Type a portion of an email address into the text field.
  <li> Click the checkbox for the fields of the bug you expect the address will be within.
</ul>

<p>

You can look for up to two different email addresses. If you specify
both, then only bugs which match both emails will show up.  This is useful to
find bugs that were, for example, created by Ralph and assigned to
Fred.

<p>

You can also use the drop down menus to specify whether you want to
match addresses by doing a substring match, by using <a href="http://www.mozilla.org/bugs/text-searching.html">Regular
Expressions</a>, or by exactly matching a fully specified email address.


};











print qq{
<a name="textsearch"></a>
<p><br><center><h3>Text Search</h3></center>
<center>



<table width="610" bgcolor="#00afff" border="0" cellpadding="0" cellspacing="0">
<tr>
<td align="center" height="210" >

<table>
<tr>
<td align="right"><a href="queryhelp.cgi#summaries">Bug summary</a>:</td>
<td><input name="short_desc" size="30" value=""></td>
<td><SELECT NAME="short_desc_type">
<OPTION VALUE="substring">case-insensitive substring
<OPTION VALUE="casesubstring">case-sensitive substring
<OPTION VALUE="allwords">all words
<OPTION VALUE="anywords">any words
<OPTION VALUE="regexp">regular expression
<OPTION VALUE="notregexp">not ( regular expression )
</SELECT></TD>
</tr>
<tr>
<td align="right"><a href="queryhelp.cgi#descriptions">A description entry</a>:</td>
<td><input name="long_desc" size="30" value=""></td>
<td><SELECT NAME="long_desc_type">
<OPTION VALUE="substring">case-insensitive substring
<OPTION VALUE="casesubstring">case-sensitive substring
<OPTION VALUE="allwords">all words
<OPTION VALUE="anywords">any words
<OPTION VALUE="regexp">regular expression
<OPTION VALUE="notregexp">not ( regular expression )
</SELECT></TD>
</tr>
<tr>
<td align="right"><a href="queryhelp.cgi#url">Associated URL</a>:</td>
<td><input name="bug_file_loc" size="30" value=""></td>
<td><SELECT NAME="bug_file_loc_type">
<OPTION VALUE="substring">case-insensitive substring
<OPTION VALUE="casesubstring">case-sensitive substring
<OPTION VALUE="allwords">all words
<OPTION VALUE="anywords">any words
<OPTION VALUE="regexp">regular expression
<OPTION VALUE="notregexp">not ( regular expression )
</SELECT></TD>
</tr>
<tr>
<td align="right"><a href="queryhelp.cgi#statuswhiteboard">Status whiteboard</a>:</td>
<td><input name="status_whiteboard" size="30" value=""></td>
<td><SELECT NAME="status_whiteboard_type">
<OPTION VALUE="substring">case-insensitive substring
<OPTION VALUE="casesubstring">case-sensitive substring
<OPTION VALUE="allwords">all words
<OPTION VALUE="anywords">any words
<OPTION VALUE="regexp">regular expression
<OPTION VALUE="notregexp">not ( regular expression )
</SELECT></TD>
</tr>

<TR>
<TD ALIGN="right"><A HREF="queryhelp.cgi#keywords">Keywords</A>:</TD>
<TD><INPUT NAME="keywords" SIZE="30" VALUE=""></TD>
<TD>
<SELECT NAME="keywords_type"><OPTION VALUE="anywords">Any of the listed keywords set
<OPTION VALUE="allwords">All of the listed keywords set
<OPTION VALUE="nowords">None of the listed keywords set
</SELECT></TD></TR>
</table>
</td></tr>
</table>
</center>
<br>


<p>In this section, you can enter values that are searched for in all the bugs (or whatever you
limit the bugs to in other fields). 
You might want to look at <a href="http://www.mozilla.org/bugs/text-searching.html">Bugzilla Text Searching</a>
to see info on <a href="http://www.mozilla.org/bugs/text-searching.html">Regular Expressions</a> and text searching. The box next to these fields decides
how a match will be determined.<br>

<a name="summaries"></a>
<h4>Bug summary</h4>
<p>This lets you search the summaries. The summary is one line that attempts to sum up the bug.

<a name="descriptions"></a>
<h4>A description entry</h4>
<p>This lets you search comments. Comments can be added by anybody. Comments are the largest
searchable area in most bugs. If you really want to find a lot of matches, search the comments.

<a name="url"></a>
<h4>Associated URL</h4>
<p>This lets you search the url field. This contains the url of the web page the bug is
about.

<a name="statuswhiteboard"></a>
<h4>Status Whiteboard</h4>
<p>This lets you search the bug's status whiteboard. The status whiteboard contains general
information that engineers add.
};


print qq{
<a name="keywords"></a>
<h4>Keywords</h4>
<br><br>Each bug can have keywords specified. The bug reporter or a 
user with the proper permissions can edit these keywords. The following is a list of the keywords that are
stored on this version of Bugzilla:
};

my $tableheader = qq{
<p><table border="1" cellpadding="4" cellspacing="0">
<tr bgcolor="#6666FF">
<th align="left">Name</th>
<th align="left">Description</th>
<th align="left">Bugs</th>
</tr> 
};

print $tableheader;

my $line_count = 0;
my $max_table_size = 50;

SendSQL("SELECT keyworddefs.name, keyworddefs.description, 
                COUNT(keywords.bug_id), keywords.bug_id
         FROM keyworddefs LEFT JOIN keywords ON keyworddefs.id=keywords.keywordid
         GROUP BY keyworddefs.id
         ORDER BY keyworddefs.name");

while (MoreSQLData()) {
    my ($name, $description, $bugs, $onebug) = FetchSQLData();
    if ($bugs && $onebug) {
        # This 'onebug' stuff is silly hackery for old versions of
        # MySQL that seem to return a count() of 1 even if there are
        # no matching.  So, we ask for an actual bug number.  If it
        # can't find any bugs that match the keyword, then we set the
        # count to be zero, ignoring what it had responded.
        my $q = url_quote($name);
        $bugs = qq{<A HREF="buglist.cgi?keywords=$q">$bugs</A>};
    } else {
        $bugs = "none";
    }
    if ($line_count == $max_table_size) {
        print "</table>\n$tableheader";
        $line_count = 0;
    }
    $line_count++;
    print qq{
<tr>
<th>$name</th>
<td>$description</td>
<td align="right">$bugs</td>
</tr>
};
}

print "</table><p>\n";

quietly_check_login();

if (UserInGroup("editkeywords")) {
    print qq{<p><a href="editkeywords.cgi">Edit keywords</a>\n};
}














print qq{
<a name="moduleoptions"></a>
<p><br><center><h3>Module Options</h3></center>

<center>


<table width="710" bgcolor="#00afff" border="0" cellpadding="0" cellspacing="0">
<tr>
<td align="center" height="190" >
<table>
<tr><td>
<table>
<tr>
<th align="left"><A HREF="queryhelp.cgi#product">Product</a>:</th>
<th align="left"><A HREF="queryhelp.cgi#version">Version</a>:</th>
<th align="left"><A HREF="queryhelp.cgi#component">Component</a>:</th>
<th align="left"><A HREF="queryhelp.cgi#targetmilestone">Milestone</a>:</th>
</tr>
<tr>
<td align="left" valign="top">
<SELECT NAME="product" MULTIPLE SIZE=5>
<OPTION VALUE="Browser">Browser<OPTION VALUE="Browser Localizations">Browser Localizations<OPTION VALUE="Calendar">Calendar<OPTION VALUE="CCK">CCK<OPTION VALUE="Derivatives">Derivatives<OPTION VALUE="Directory">Directory<OPTION VALUE="Documentation">Documentation<OPTION VALUE="Grendel">Grendel<OPTION VALUE="JSS">JSS<OPTION VALUE="MailNews">MailNews<OPTION VALUE="mozilla.org">mozilla.org<OPTION VALUE="MozillaClassic">MozillaClassic<OPTION VALUE="NSPR">NSPR<OPTION VALUE="NSS">NSS<OPTION VALUE="PSM">PSM<OPTION VALUE="Rhino">Rhino<OPTION VALUE="Webtools">Webtools
</SELECT>
</td>

<td align="left" valign="top">
<SELECT NAME="version" MULTIPLE SIZE=5>
<OPTION VALUE="1.01">1.01<OPTION VALUE="1.1">1.1<OPTION VALUE="1.2">1.2<OPTION VALUE="1.3">1.3<OPTION VALUE="1.4">1.4<OPTION VALUE="1.5">1.5<OPTION VALUE="1998-03-31">1998-03-31<OPTION VALUE="1998-04-08">1998-04-08<OPTION VALUE="1998-04-29">1998-04-29<OPTION VALUE="1998-06-03">1998-06-03<OPTION VALUE="1998-07-28">1998-07-28<OPTION VALUE="1998-09-04">1998-09-04<OPTION VALUE="2.0">2.0<OPTION VALUE="2.1">2.1<OPTION VALUE="3.0">3.0<OPTION VALUE="3.0.1">3.0.1<OPTION VALUE="3.1">3.1<OPTION VALUE="3.1.1">3.1.1<OPTION VALUE="3.1.2">3.1.2<OPTION VALUE="3.1.3">3.1.3<OPTION VALUE="3.2">3.2<OPTION VALUE="3.5">3.5<OPTION VALUE="3.5.1">3.5.1<OPTION VALUE="4.0">4.0<OPTION VALUE="4.0.1">4.0.1<OPTION VALUE="4.0.2">4.0.2<OPTION VALUE="4.1">4.1<OPTION VALUE="other">other<OPTION VALUE="unspecified">unspecified
</SELECT>
</td>

<td align="left" valign="top">
<SELECT NAME="component" MULTIPLE SIZE=5>
<OPTION VALUE="Account Manager">Account Manager<OPTION VALUE="ActiveX Wrapper">ActiveX Wrapper<OPTION VALUE="Address Book">Address Book<OPTION VALUE="Addressbook/LDAP (non-UI)">Addressbook/LDAP (non-UI)<OPTION VALUE="AS-Whitebox">AS-Whitebox<OPTION VALUE="Aurora/RDF BE">Aurora/RDF BE<OPTION VALUE="Aurora/RDF FE">Aurora/RDF FE<OPTION VALUE="Berkeley DB">Berkeley DB<OPTION VALUE="Bonsai">Bonsai<OPTION VALUE="Bookmarks">Bookmarks<OPTION VALUE="Bosnian/bs-BA">Bosnian/bs-BA<OPTION VALUE="Browser Hooks">Browser Hooks<OPTION VALUE="Browser-General">Browser-General<OPTION VALUE="Bugzilla">Bugzilla<OPTION VALUE="Bugzilla: Keyword/Component Changes">Bugzilla: Keyword/Component Changes<OPTION VALUE="Build">Build<OPTION VALUE="Build Config">Build Config<OPTION VALUE="Bulgarian/bg-BG">Bulgarian/bg-BG<OPTION VALUE="CCK-CDLayout">CCK-CDLayout<OPTION VALUE="CCK-CustomShell">CCK-CustomShell<OPTION VALUE="CCK-Installation">CCK-Installation<OPTION VALUE="CCK-Shell">CCK-Shell<OPTION VALUE="CCK-Whitebox">CCK-Whitebox<OPTION VALUE="CCK-Wizard">CCK-Wizard<OPTION VALUE="chatzilla">chatzilla<OPTION VALUE="Client Library">Client Library<OPTION VALUE="Compiler">Compiler<OPTION VALUE="Composer">Composer<OPTION VALUE="Composition">Composition<OPTION VALUE="Composition (non-UI)">Composition (non-UI)<OPTION VALUE="Compositor">Compositor<OPTION VALUE="Compositor Library">Compositor Library<OPTION VALUE="config">config<OPTION VALUE="Cookies">Cookies<OPTION VALUE="Core">Core<OPTION VALUE="Daemon">Daemon<OPTION VALUE="Danish/da-DK">Danish/da-DK<OPTION VALUE="Despot">Despot<OPTION VALUE="Dialup-Account Setup">Dialup-Account Setup<OPTION VALUE="Dialup-Install">Dialup-Install<OPTION VALUE="Dialup-Mup/Muc">Dialup-Mup/Muc<OPTION VALUE="Dialup-Upgrade">Dialup-Upgrade<OPTION VALUE="Documentation">Documentation<OPTION VALUE="DOM Level 0">DOM Level 0<OPTION VALUE="DOM Level 1">DOM Level 1<OPTION VALUE="DOM Level 2">DOM Level 2<OPTION VALUE="DOM to Text Conversion">DOM to Text Conversion<OPTION VALUE="DOM Viewer">DOM Viewer<OPTION VALUE="Dutch/nl">Dutch/nl<OPTION VALUE="Editor">Editor<OPTION VALUE="Embedding APIs">Embedding APIs<OPTION VALUE="Embedding: Docshell">Embedding: Docshell<OPTION VALUE="Evangelism">Evangelism<OPTION VALUE="Event Handling">Event Handling<OPTION VALUE="Filters">Filters<OPTION VALUE="FontLib">FontLib<OPTION VALUE="Form Manager">Form Manager<OPTION VALUE="Form Submission">Form Submission<OPTION VALUE="French/fr-FR">French/fr-FR<OPTION VALUE="German-Austria/de-AT">German-Austria/de-AT<OPTION VALUE="GTK Embedding Widget">GTK Embedding Widget<OPTION VALUE="Help">Help<OPTION VALUE="History">History<OPTION VALUE="HTML Element">HTML Element<OPTION VALUE="HTML Form Controls">HTML Form Controls<OPTION VALUE="HTML to Text/PostScript Translation">HTML to Text/PostScript Translation<OPTION VALUE="HTMLFrames">HTMLFrames<OPTION VALUE="HTMLTables">HTMLTables<OPTION VALUE="Image Conversion Library">Image Conversion Library<OPTION VALUE="ImageLib">ImageLib<OPTION VALUE="Install">Install<OPTION VALUE="Installer">Installer<OPTION VALUE="Installer: XPI Packages">Installer: XPI Packages<OPTION VALUE="Installer: XPInstall Engine">Installer: XPInstall Engine<OPTION VALUE="Internationalization">Internationalization<OPTION VALUE="Italian/it-IT">Italian/it-IT<OPTION VALUE="Java APIs for DOM">Java APIs for DOM<OPTION VALUE="Java APIs to WebShell">Java APIs to WebShell<OPTION VALUE="Java Stubs">Java Stubs<OPTION VALUE="Java to XPCOM Bridge">Java to XPCOM Bridge<OPTION VALUE="Java-Implemented Plugins">Java-Implemented Plugins<OPTION VALUE="JavaScript Debugger">JavaScript Debugger<OPTION VALUE="Javascript Engine">Javascript Engine<OPTION VALUE="JPEG Image Handling">JPEG Image Handling<OPTION VALUE="Keyboard Navigation">Keyboard Navigation<OPTION VALUE="Layout">Layout<OPTION VALUE="LDAP C SDK">LDAP C SDK<OPTION VALUE="LDAP Java SDK">LDAP Java SDK<OPTION VALUE="LDAP Tools">LDAP Tools<OPTION VALUE="LibMocha">LibMocha<OPTION VALUE="Libraries">Libraries<OPTION VALUE="Library">Library<OPTION VALUE="Live Connect">Live Connect<OPTION VALUE="Localization">Localization<OPTION VALUE="LXR">LXR<OPTION VALUE="Macintosh FE">Macintosh FE<OPTION VALUE="Mail Back End">Mail Back End<OPTION VALUE="Mail Database">Mail Database<OPTION VALUE="Mail Window Front End">Mail Window Front End<OPTION VALUE="MathML">MathML<OPTION VALUE="MIME">MIME<OPTION VALUE="MIMELib">MIMELib<OPTION VALUE="Miscellaneous">Miscellaneous<OPTION VALUE="Movemail">Movemail<OPTION VALUE="Mozbot">Mozbot<OPTION VALUE="Mozilla Developer">Mozilla Developer<OPTION VALUE="MozillaTranslator">MozillaTranslator<OPTION VALUE="Necko">Necko<OPTION VALUE="NetLib">NetLib<OPTION VALUE="Netscape 6">Netscape 6<OPTION VALUE="Networking">Networking<OPTION VALUE="Networking - General">Networking - General<OPTION VALUE="Networking - IMAP">Networking - IMAP<OPTION VALUE="Networking - News">Networking - News<OPTION VALUE="Networking - POP">Networking - POP<OPTION VALUE="Networking - SMTP">Networking - SMTP<OPTION VALUE="Networking: Cache">Networking: Cache<OPTION VALUE="Networking: File">Networking: File<OPTION VALUE="Networking: FTP">Networking: FTP<OPTION VALUE="Networking: HTTP">Networking: HTTP<OPTION VALUE="NLS">NLS<OPTION VALUE="Norwegian/nno-no">Norwegian/nno-no<OPTION VALUE="NSPR">NSPR<OPTION VALUE="Offline">Offline<OPTION VALUE="OJI">OJI<OPTION VALUE="Parser">Parser<OPTION VALUE="Password Manager">Password Manager<OPTION VALUE="PerLDAP">PerLDAP<OPTION VALUE="PICS">PICS<OPTION VALUE="Plug-ins">Plug-ins<OPTION VALUE="PNG Image Handling">PNG Image Handling<OPTION VALUE="Preferences">Preferences<OPTION VALUE="Preferences: Backend">Preferences: Backend<OPTION VALUE="Printing">Printing<OPTION VALUE="Profile Manager BackEnd">Profile Manager BackEnd<OPTION VALUE="Profile Manager FrontEnd">Profile Manager FrontEnd<OPTION VALUE="Profile Migration">Profile Migration<OPTION VALUE="Protocols">Protocols<OPTION VALUE="RDF">RDF<OPTION VALUE="Registry">Registry<OPTION VALUE="Sample Code">Sample Code<OPTION VALUE="Search">Search<OPTION VALUE="Security Stubs">Security Stubs<OPTION VALUE="Security: CAPS">Security: CAPS<OPTION VALUE="Security: Crypto">Security: Crypto<OPTION VALUE="Security: General">Security: General<OPTION VALUE="Selection">Selection<OPTION VALUE="Server Operations">Server Operations<OPTION VALUE="Sidebar">Sidebar<OPTION VALUE="Skinability">Skinability<OPTION VALUE="String">String<OPTION VALUE="StubFE">StubFE<OPTION VALUE="Style System">Style System<OPTION VALUE="Subscribe">Subscribe<OPTION VALUE="SVG">SVG<OPTION VALUE="Talkback">Talkback<OPTION VALUE="Test">Test<OPTION VALUE="Tests">Tests<OPTION VALUE="Testzilla">Testzilla<OPTION VALUE="Themes">Themes<OPTION VALUE="Threading">Threading<OPTION VALUE="Tinderbox">Tinderbox<OPTION VALUE="Tools">Tools<OPTION VALUE="Tracking">Tracking<OPTION VALUE="UI">UI<OPTION VALUE="User">User<OPTION VALUE="User interface">User interface<OPTION VALUE="User Interface: Design Feedback">User Interface: Design Feedback<OPTION VALUE="Viewer App">Viewer App<OPTION VALUE="Views">Views<OPTION VALUE="Web Developer">Web Developer<OPTION VALUE="Windows FE">Windows FE<OPTION VALUE="XBL">XBL<OPTION VALUE="XFE">XFE<OPTION VALUE="XML">XML<OPTION VALUE="XP Apps">XP Apps<OPTION VALUE="XP Apps: Cmd-line Features">XP Apps: Cmd-line Features<OPTION VALUE="XP Apps: GUI Features">XP Apps: GUI Features<OPTION VALUE="XP Miscellany">XP Miscellany<OPTION VALUE="XP Toolkit/Widgets">XP Toolkit/Widgets<OPTION VALUE="XP Toolkit/Widgets: Menus">XP Toolkit/Widgets: Menus<OPTION VALUE="XP Toolkit/Widgets: Trees">XP Toolkit/Widgets: Trees<OPTION VALUE="XP Toolkit/Widgets: XUL">XP Toolkit/Widgets: XUL<OPTION VALUE="XP Utilities">XP Utilities<OPTION VALUE="XPCOM">XPCOM<OPTION VALUE="XPCOM Registry">XPCOM Registry<OPTION VALUE="XPConnect">XPConnect<OPTION VALUE="XPFC">XPFC<OPTION VALUE="xpidl">xpidl<OPTION VALUE="XSLT">XSLT
</SELECT>
</td>
<td align="left" valign="top">
<SELECT NAME="target_milestone" MULTIPLE SIZE=5>
<OPTION VALUE="---">---<OPTION VALUE="Future">Future<OPTION VALUE="M1">M1<OPTION VALUE="M2">M2<OPTION VALUE="M3">M3<OPTION VALUE="M4">M4<OPTION VALUE="M5">M5<OPTION VALUE="M6">M6<OPTION VALUE="M7">M7<OPTION VALUE="M8">M8<OPTION VALUE="M9">M9<OPTION VALUE="M10">M10<OPTION VALUE="M11">M11<OPTION VALUE="M12">M12<OPTION VALUE="M13">M13<OPTION VALUE="M14">M14<OPTION VALUE="M15">M15<OPTION VALUE="M16">M16<OPTION VALUE="M17">M17<OPTION VALUE="M18">M18<OPTION VALUE="mozilla0.6">mozilla0.6<OPTION VALUE="mozilla0.8">mozilla0.8<OPTION VALUE="mozilla0.9">mozilla0.9<OPTION VALUE="mozilla0.9.1">mozilla0.9.1<OPTION VALUE="mozilla1.0">mozilla1.0<OPTION VALUE="mozilla1.0.1">mozilla1.0.1<OPTION VALUE="mozilla1.1">mozilla1.1<OPTION VALUE="mozilla1.2">mozilla1.2<OPTION VALUE="M19">M19<OPTION VALUE="M20">M20<OPTION VALUE="M21">M21<OPTION VALUE="M22">M22<OPTION VALUE="M23">M23<OPTION VALUE="M24">M24<OPTION VALUE="M25">M25<OPTION VALUE="M26">M26<OPTION VALUE="M27">M27<OPTION VALUE="M28">M28<OPTION VALUE="M29">M29<OPTION VALUE="M30">M30<OPTION VALUE="3.0">3.0<OPTION VALUE="3.0.1">3.0.1<OPTION VALUE="3.0.2">3.0.2<OPTION VALUE="3.1">3.1<OPTION VALUE="3.1.1">3.1.1<OPTION VALUE="3.1.2">3.1.2<OPTION VALUE="3.1.3">3.1.3<OPTION VALUE="3.2">3.2<OPTION VALUE="3.3">3.3<OPTION VALUE="3.5">3.5<OPTION VALUE="3.5.1">3.5.1<OPTION VALUE="4.0">4.0<OPTION VALUE="4.0.1">4.0.1<OPTION VALUE="4.0.2">4.0.2<OPTION VALUE="4.1">4.1<OPTION VALUE="4.1.1">4.1.1<OPTION VALUE="4.2">4.2
</SELECT>
</td>

</tr>
</table>
</td>
</tr>
</table>
</td>
</tr>
</table>

</center>
<br>

<p>Module options are where you select what program, module and version the bugs you want to 
find describe. Selecting one or more of the programs, versions, components, or milestones will limit your search.

<p><a name="product"></a>
<h4>Products</h4> 


<p>Although all subprojects within the Mozilla project are similiar, there are several seperate
products being developed. Each product has its own components.

};




$line_count = 0;
$max_table_size = 50;
my @products;

$tableheader = 	qq{ <p><table border=0><tr><td>
	<table border="1" width="100%" cellpadding="4" cellspacing="0">
	<tr bgcolor="#6666FF">
	<th align="left">Product</th>
	<th align="left">Description</th></tr> };


print qq{
	$tableheader
};


SendSQL("SELECT product,description FROM products ORDER BY product");
	while (MoreSQLData()) {

	my ($product, $productdesc) = FetchSQLData();
	push (@products, $product);

	$line_count++;
	if ($line_count > $max_table_size) {
			print qq{
			</table>
			$tableheader
		};
	  	$line_count=1;
	}

	print qq{ <tr><th>$product</th><td>$productdesc</td></tr> };


}


print qq{ 	

</table></td></tr></table> };

if (UserInGroup("editcomponents")) {
    print qq{<p><a href="editproducts.cgi">Edit products</a><p>};
}

print qq{
<p><a name="version"></a>
<h4>Version</h4>

<p>This is simply the version that the bugs you want to find are marked for. 
Many of the bugs will be marked for another version and will have their milestones 
entered instead (milestones explained below).

};




 
$line_count = 0;
$tableheader = qq{ 
	<p>
	<table border="1" width="100%" cellpadding="4" cellspacing="0">
	<tr bgcolor="#6666FF">
	<th align="left">Component</th>
	<th align="left">Product</th>
	<th align="left">Description</th></tr>
};

print qq{ 	
<p><a name="component"></a>
<h4>Component</h4>
<p>Each product has components, against which bugs can be filed. Components are parts of 
the product, and are assigned to a module owner. The following lists 
components and their associated products:
		$tableheader
};
foreach $product (@products)
{

	SendSQL("SELECT value,description FROM components WHERE program=" . SqlQuote($product) . " ORDER BY value");

	while (MoreSQLData()) {

		my ($component, $compdesc) = FetchSQLData();

		$line_count++;
		if ($line_count > $max_table_size) {
				print qq{
				</table>
				$tableheader
			};
			$line_count=0;
		}
		print qq{<tr><th>$component</th><td>$product</td><td>$compdesc</td></tr>};
	}

}

print qq{</table>};
if (UserInGroup("editcomponents")) {
    print qq{<p><a href="editcomponents.cgi">Edit components</a><p>};
}

print qq{
<p><a name="targetmilestone"></a>
<h4>Milestone</h4>

<p>Choosing this section lets you search through bugs that have their target milestones set to certain 
values. Milestones are kind of like versions. They are specific tentative dates where a massive
phasing out of bugs occur and a relatively stable release is produced. Milestones used to be in the
form "M18", but now are in the form of "Mozilla0.9". <a href="http://www.mozilla.org/roadmap.html">The roadmap</a>.


};














print qq{
<a name="incexcoptions"></a>
<p><br><center><h3>Inclusion/Exclusion Options</h3></center>

<center>


<table width="480" bgcolor="#00afff" border="0" cellpadding="0" cellspacing="0">
<tr>
<td align="center"  height="260" >
<table>

<tr>
<td>
<SELECT NAME="bugidtype">
<OPTION VALUE="include">Only
<OPTION VALUE="exclude" >Exclude
</SELECT>
bugs numbered: 
<INPUT TYPE="text" NAME="bug_id" VALUE="" SIZE=15>
</td>
</tr>
<tr>
<td>
Changed in the last <INPUT NAME=changedin SIZE=3 VALUE=""> days
</td>
</tr>
<tr>
<td>
Containing at least <INPUT NAME=votes SIZE=3 VALUE=""> votes
</td>
</tr>
<tr>
<td>
Where the field(s)
<SELECT NAME="chfield" MULTIPLE SIZE=4>
<OPTION VALUE="[Bug creation]">[Bug creation]<OPTION VALUE="assigned_to">assigned_to<OPTION VALUE="bug_file_loc">bug_file_loc<OPTION VALUE="bug_severity">bug_severity<OPTION VALUE="bug_status">bug_status<OPTION VALUE="component">component<OPTION VALUE="everconfirmed">everconfirmed<OPTION VALUE="groupset">groupset<OPTION VALUE="keywords">keywords<OPTION VALUE="op_sys">op_sys<OPTION VALUE="priority">priority<OPTION VALUE="product">product<OPTION VALUE="qa_contact">qa_contact<OPTION VALUE="rep_platform">rep_platform<OPTION VALUE="reporter">reporter<OPTION VALUE="resolution">resolution<OPTION VALUE="short_desc">short_desc<OPTION VALUE="status_whiteboard">status_whiteboard<OPTION VALUE="target_milestone">target_milestone<OPTION VALUE="version">version<OPTION VALUE="votes">votes
</SELECT> changed to <INPUT NAME="chfieldvalue" SIZE="10">
</td>
</tr>
<tr>
<td colspan=2>
During dates <INPUT NAME="chfieldfrom" SIZE="10" VALUE="">
to <INPUT NAME="chfieldto" SIZE="10" VALUE="Now">
</td>
</tr>
<tr>
<td>

</td>
</tr>
</table>

</td>
</tr>
</table>

</center>
<br>

<p>Inclusion/Exclusion options is a powerful section that gives you the ability to include and
exclude bugs based on values you enter.

<P><b>[Only, Exclude] bugs numbered [text] </b>

<p>This lets you put in a comma-delimited list of bugs you want to have your results chosen from, or those
of which you want to exclude. It would be nice in the future if you could type in ranges, i.e. [1-1000] for 1
to 1000. Unfortunately, you cannot do that as of now.

<p><b>Changed in the last [text] days</b>

<p>Lets you specify how many days ago - at maximum - a bug could have changed state.

<p><b>At least [text] votes</b>

<p>With this, you can choose how many votes - at minimum - a bug has. 

<p><b>Where the field(s) [fields] changed to [text]</b>

<p>With this, you can specify values to search for in fields that exist in the bug If you choose 
one or more fields, you have to fill out one of the fields to the right. It might 
be difficult to figure out what these fields mean if you are a newbie to the query. 
They match various fields within the bug information. Optionally, you can 
also enter what value you want the field to have changed to if you only entered one field.
For instance, if the bug changed who it was assigned to from jon\@netscape.com 
to brian\@netscape.com , you could enter in
assigned_to changed to brian\@netscape.com.

<p><b>During dates [text] to [text] </b>

<p>Here, you can choose what dates the fields changed. "Now" can be used as an entry. Other entries should be in
mm/dd/yyyy or yyyy-mm-dd format.


};












print qq{
<a name="advancedquerying"></a>
<p><br><center><h3>Advanced Querying Using "Boolean Charts"</h3></center>

<p>The Bugzilla query page is designed to be reasonably easy to use.
But, with such ease of use always comes some lack of power.  The
Advanced Querying section is designed to let you do very powerful
queries, but it's not the easiest thing to learn (or explain).
<br>
<p>
<center>

<table width="780" bgcolor="#00afff" border="0" cellpadding="0" cellspacing="0">
<tr>
<td align="center"  height="140" >
<table>
<tr><td>
<table><tr><td>&nbsp;</td><td><SELECT NAME="field0-0-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="groupset">groupset
<OPTION VALUE="bug_id">Bug #
<OPTION VALUE="short_desc">Summary
<OPTION VALUE="product">Product
<OPTION VALUE="version">Version
<OPTION VALUE="rep_platform">Platform
<OPTION VALUE="bug_file_loc">URL
<OPTION VALUE="op_sys">OS/Version
<OPTION VALUE="bug_status">Status
<OPTION VALUE="status_whiteboard">Status Whiteboard
<OPTION VALUE="keywords">Keywords
<OPTION VALUE="resolution">Resolution
<OPTION VALUE="bug_severity">Severity
<OPTION VALUE="priority">Priority
<OPTION VALUE="component">Component
<OPTION VALUE="assigned_to">AssignedTo
<OPTION VALUE="reporter">ReportedBy
<OPTION VALUE="votes">Votes
<OPTION VALUE="qa_contact">QAContact
<OPTION VALUE="cc">CC
<OPTION VALUE="dependson">BugsThisDependsOn
<OPTION VALUE="blocked">OtherBugsDependingOnThis
<OPTION VALUE="attachments.description">Attachment description
<OPTION VALUE="attachments.thedata">Attachment data
<OPTION VALUE="attachments.mimetype">Attachment mime type
<OPTION VALUE="attachments.ispatch">Attachment is patch
<OPTION VALUE="target_milestone">Target Milestone
<OPTION VALUE="delta_ts">Last changed date
<OPTION VALUE="(to_days(now()) - to_days(bugs.delta_ts))">Days since bug changed
<OPTION VALUE="longdesc">Comment
</SELECT><SELECT NAME="type0-0-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="equals">equal to
<OPTION VALUE="notequals">not equal to
<OPTION VALUE="casesubstring">contains (case-sensitive) substring
<OPTION VALUE="substring">contains (case-insensitive) substring
<OPTION VALUE="notsubstring">does not contain (case-insensitive) substring
<OPTION VALUE="regexp">contains regexp
<OPTION VALUE="notregexp">does not contain regexp
<OPTION VALUE="lessthan">less than
<OPTION VALUE="greaterthan">greater than
<OPTION VALUE="anywords">any words
<OPTION VALUE="allwords">all words
<OPTION VALUE="nowords">none of the words
<OPTION VALUE="changedbefore">changed before
<OPTION VALUE="changedafter">changed after
<OPTION VALUE="changedto">changed to
<OPTION VALUE="changedby">changed by
</SELECT><INPUT NAME="value0-0-0" VALUE=""><INPUT TYPE="button" VALUE="Or" ><INPUT TYPE="button" VALUE="And" 
	NAME="cmd-add0-1-0"></td></tr>
    
		<tr><td>&nbsp;</td><td align="center">
         &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
        <INPUT TYPE="button" VALUE="Add another boolean chart" NAME="cmd-add1-0-0">
       
        
</td></tr>
</table>
</td></tr>
</table>
</td>
</tr>
</table>

</center>
<br>
<p>The Advanced Query (or Boolean Chart) starts with a single "term".  A term is a
combination of two pulldown menus and a text field.
You choose items from the menus, specifying:
<p>Field 1: Where to look for the search term<br>
Field 2: How to determine what is a match<br>
Field 3: What the search term is<br>
<br>
<center>

<table width="790" bgcolor="#00afff" border="0" cellpadding="0" cellspacing="0">
<tr>
<td align="center" height="160" >
<table>
<tr><td>
<table><tr><td>&nbsp;</td><td><SELECT NAME="field0-0-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="groupset">groupset
<OPTION VALUE="bug_id">Bug #
<OPTION VALUE="short_desc">Summary
<OPTION VALUE="product">Product
<OPTION VALUE="version">Version
<OPTION VALUE="rep_platform">Platform
<OPTION VALUE="bug_file_loc">URL
<OPTION VALUE="op_sys">OS/Version
<OPTION VALUE="bug_status">Status
<OPTION VALUE="status_whiteboard">Status Whiteboard
<OPTION VALUE="keywords">Keywords
<OPTION VALUE="resolution">Resolution
<OPTION VALUE="bug_severity">Severity
<OPTION VALUE="priority">Priority
<OPTION VALUE="component">Component
<OPTION VALUE="assigned_to">AssignedTo
<OPTION VALUE="reporter">ReportedBy
<OPTION VALUE="votes">Votes
<OPTION VALUE="qa_contact">QAContact
<OPTION VALUE="cc">CC
<OPTION VALUE="dependson">BugsThisDependsOn
<OPTION VALUE="blocked">OtherBugsDependingOnThis
<OPTION VALUE="attachments.description">Attachment description
<OPTION VALUE="attachments.thedata">Attachment data
<OPTION VALUE="attachments.mimetype">Attachment mime type
<OPTION VALUE="attachments.ispatch">Attachment is patch
<OPTION VALUE="target_milestone">Target Milestone
<OPTION VALUE="delta_ts">Last changed date
<OPTION VALUE="(to_days(now()) - to_days(bugs.delta_ts))">Days since bug changed
<OPTION VALUE="longdesc">Comment
</SELECT><SELECT NAME="type0-0-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="equals">equal to
<OPTION VALUE="notequals">not equal to
<OPTION VALUE="casesubstring">contains (case-sensitive) substring
<OPTION VALUE="substring">contains (case-insensitive) substring
<OPTION VALUE="notsubstring">does not contain (case-insensitive) substring
<OPTION VALUE="regexp">contains regexp
<OPTION VALUE="notregexp">does not contain regexp
<OPTION VALUE="lessthan">less than
<OPTION VALUE="greaterthan">greater than
<OPTION VALUE="anywords">any words
<OPTION VALUE="allwords">all words
<OPTION VALUE="nowords">none of the words
<OPTION VALUE="changedbefore">changed before
<OPTION VALUE="changedafter">changed after
<OPTION VALUE="changedto">changed to
<OPTION VALUE="changedby">changed by
</SELECT><INPUT NAME="value0-0-0" VALUE=""></td></tr><tr><td><b>OR</b></td><td><SELECT NAME="field0-0-1"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="groupset">groupset
<OPTION VALUE="bug_id">Bug #
<OPTION VALUE="short_desc">Summary
<OPTION VALUE="product">Product
<OPTION VALUE="version">Version
<OPTION VALUE="rep_platform">Platform
<OPTION VALUE="bug_file_loc">URL
<OPTION VALUE="op_sys">OS/Version
<OPTION VALUE="bug_status">Status
<OPTION VALUE="status_whiteboard">Status Whiteboard
<OPTION VALUE="keywords">Keywords
<OPTION VALUE="resolution">Resolution
<OPTION VALUE="bug_severity">Severity
<OPTION VALUE="priority">Priority
<OPTION VALUE="component">Component
<OPTION VALUE="assigned_to">AssignedTo
<OPTION VALUE="reporter">ReportedBy
<OPTION VALUE="votes">Votes
<OPTION VALUE="qa_contact">QAContact
<OPTION VALUE="cc">CC
<OPTION VALUE="dependson">BugsThisDependsOn
<OPTION VALUE="blocked">OtherBugsDependingOnThis
<OPTION VALUE="attachments.description">Attachment description
<OPTION VALUE="attachments.thedata">Attachment data
<OPTION VALUE="attachments.mimetype">Attachment mime type
<OPTION VALUE="attachments.ispatch">Attachment is patch
<OPTION VALUE="target_milestone">Target Milestone
<OPTION VALUE="delta_ts">Last changed date
<OPTION VALUE="(to_days(now()) - to_days(bugs.delta_ts))">Days since bug changed
<OPTION VALUE="longdesc">Comment
</SELECT><SELECT NAME="type0-0-1"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="equals">equal to
<OPTION VALUE="notequals">not equal to
<OPTION VALUE="casesubstring">contains (case-sensitive) substring
<OPTION VALUE="substring">contains (case-insensitive) substring
<OPTION VALUE="notsubstring">does not contain (case-insensitive) substring
<OPTION VALUE="regexp">contains regexp
<OPTION VALUE="notregexp">does not contain regexp
<OPTION VALUE="lessthan">less than
<OPTION VALUE="greaterthan">greater than
<OPTION VALUE="anywords">any words
<OPTION VALUE="allwords">all words
<OPTION VALUE="nowords">none of the words
<OPTION VALUE="changedbefore">changed before
<OPTION VALUE="changedafter">changed after
<OPTION VALUE="changedto">changed to
<OPTION VALUE="changedby">changed by
</SELECT><INPUT NAME="value0-0-1" VALUE=""><INPUT TYPE="button" VALUE="Or" NAME="cmd-add0-0-2" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"><INPUT TYPE="button" VALUE="And" 
	NAME="cmd-add0-1-0" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"></td></tr>
    
		<tr><td>&nbsp;</td><td align="center">
         &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
        <INPUT TYPE="button" VALUE="Add another boolean chart" NAME="cmd-add1-0-0" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;">
       
        
</td></tr>
</table>
</td></tr>
</table>
</td>
</tr>
</table>

</center>

<br>
<p>The real fun starts when you click on the "Or" or "And" buttons.  If
you push the "Or" button, then you get a second term right under
the first one.  You can then configure that term, and the result of
the query will be anything that matches either of the terms.
<br>
<p>
<center>

<table width="790" bgcolor="#00afff" border="0" cellpadding="0" cellspacing="0">
<tr>
<td align="center" height="180" >

<table>
<tr><td>

<table><tr><td>&nbsp;</td><td><SELECT NAME="field0-0-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="groupset">groupset
<OPTION VALUE="bug_id">Bug #
<OPTION VALUE="short_desc">Summary
<OPTION VALUE="product">Product
<OPTION VALUE="version">Version
<OPTION VALUE="rep_platform">Platform
<OPTION VALUE="bug_file_loc">URL
<OPTION VALUE="op_sys">OS/Version
<OPTION VALUE="bug_status">Status
<OPTION VALUE="status_whiteboard">Status Whiteboard
<OPTION VALUE="keywords">Keywords
<OPTION VALUE="resolution">Resolution
<OPTION VALUE="bug_severity">Severity
<OPTION VALUE="priority">Priority
<OPTION VALUE="component">Component
<OPTION VALUE="assigned_to">AssignedTo
<OPTION VALUE="reporter">ReportedBy
<OPTION VALUE="votes">Votes
<OPTION VALUE="qa_contact">QAContact
<OPTION VALUE="cc">CC
<OPTION VALUE="dependson">BugsThisDependsOn
<OPTION VALUE="blocked">OtherBugsDependingOnThis
<OPTION VALUE="attachments.description">Attachment description
<OPTION VALUE="attachments.thedata">Attachment data
<OPTION VALUE="attachments.mimetype">Attachment mime type
<OPTION VALUE="attachments.ispatch">Attachment is patch
<OPTION VALUE="target_milestone">Target Milestone
<OPTION VALUE="delta_ts">Last changed date
<OPTION VALUE="(to_days(now()) - to_days(bugs.delta_ts))">Days since bug changed
<OPTION VALUE="longdesc">Comment
</SELECT><SELECT NAME="type0-0-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="equals">equal to
<OPTION VALUE="notequals">not equal to
<OPTION VALUE="casesubstring">contains (case-sensitive) substring
<OPTION VALUE="substring">contains (case-insensitive) substring
<OPTION VALUE="notsubstring">does not contain (case-insensitive) substring
<OPTION VALUE="regexp">contains regexp
<OPTION VALUE="notregexp">does not contain regexp
<OPTION VALUE="lessthan">less than
<OPTION VALUE="greaterthan">greater than
<OPTION VALUE="anywords">any words
<OPTION VALUE="allwords">all words
<OPTION VALUE="nowords">none of the words
<OPTION VALUE="changedbefore">changed before
<OPTION VALUE="changedafter">changed after
<OPTION VALUE="changedto">changed to
<OPTION VALUE="changedby">changed by
</SELECT><INPUT NAME="value0-0-0" VALUE=""><INPUT TYPE="button" VALUE="Or" NAME="cmd-add0-0-1" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"></td></tr><tr><td>&nbsp;</td><td align="center" valign="middle"><b>AND</b></td></tr><tr><td>&nbsp;</td><td><SELECT NAME="field0-1-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="groupset">groupset
<OPTION VALUE="bug_id">Bug #
<OPTION VALUE="short_desc">Summary
<OPTION VALUE="product">Product
<OPTION VALUE="version">Version
<OPTION VALUE="rep_platform">Platform
<OPTION VALUE="bug_file_loc">URL
<OPTION VALUE="op_sys">OS/Version
<OPTION VALUE="bug_status">Status
<OPTION VALUE="status_whiteboard">Status Whiteboard
<OPTION VALUE="keywords">Keywords
<OPTION VALUE="resolution">Resolution
<OPTION VALUE="bug_severity">Severity
<OPTION VALUE="priority">Priority
<OPTION VALUE="component">Component
<OPTION VALUE="assigned_to">AssignedTo
<OPTION VALUE="reporter">ReportedBy
<OPTION VALUE="votes">Votes
<OPTION VALUE="qa_contact">QAContact
<OPTION VALUE="cc">CC
<OPTION VALUE="dependson">BugsThisDependsOn
<OPTION VALUE="blocked">OtherBugsDependingOnThis
<OPTION VALUE="attachments.description">Attachment description
<OPTION VALUE="attachments.thedata">Attachment data
<OPTION VALUE="attachments.mimetype">Attachment mime type
<OPTION VALUE="attachments.ispatch">Attachment is patch
<OPTION VALUE="target_milestone">Target Milestone
<OPTION VALUE="delta_ts">Last changed date
<OPTION VALUE="(to_days(now()) - to_days(bugs.delta_ts))">Days since bug changed
<OPTION VALUE="longdesc">Comment
</SELECT><SELECT NAME="type0-1-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="equals">equal to
<OPTION VALUE="notequals">not equal to
<OPTION VALUE="casesubstring">contains (case-sensitive) substring
<OPTION VALUE="substring">contains (case-insensitive) substring
<OPTION VALUE="notsubstring">does not contain (case-insensitive) substring
<OPTION VALUE="regexp">contains regexp
<OPTION VALUE="notregexp">does not contain regexp
<OPTION VALUE="lessthan">less than
<OPTION VALUE="greaterthan">greater than
<OPTION VALUE="anywords">any words
<OPTION VALUE="allwords">all words
<OPTION VALUE="nowords">none of the words
<OPTION VALUE="changedbefore">changed before
<OPTION VALUE="changedafter">changed after
<OPTION VALUE="changedto">changed to
<OPTION VALUE="changedby">changed by
</SELECT><INPUT NAME="value0-1-0" VALUE=""><INPUT TYPE="button" VALUE="Or" NAME="cmd-add0-1-1" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"><INPUT TYPE="button" VALUE="And" 
	NAME="cmd-add0-2-0" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"></td></tr>
    
		<tr><td>&nbsp;</td><td align="center">
         &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
        <INPUT TYPE="button" VALUE="Add another boolean chart" NAME="cmd-add1-0-0" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;">
       
        
</td></tr>
</table>
</td></tr>
</table>
</td>
</tr>
</table>

</center>
<br>
<p>You can push the "And" button, and get a new term below the
original one - seperated by the word "AND", and now the result of 
the query will be anything that matches both sets of terms.

<p>You can keep clicking "And" and "Or", and get a page with many
terms.  "Or" has higher precedence than "And".  You
can think of the lines of "Or" as having parenthesis around them. 

<br><p>
<center>

<table width="790" bgcolor="#00afff" border="0" cellpadding="0" cellspacing="0">
<tr>
<td align="center" height="170" >

<table>
<tr><td>
<table><tr><td>&nbsp;</td><td><SELECT NAME="field0-0-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="groupset">groupset
<OPTION VALUE="bug_id">Bug #
<OPTION VALUE="short_desc">Summary
<OPTION VALUE="product">Product
<OPTION VALUE="version">Version
<OPTION VALUE="rep_platform">Platform
<OPTION VALUE="bug_file_loc">URL
<OPTION VALUE="op_sys">OS/Version
<OPTION VALUE="bug_status">Status
<OPTION VALUE="status_whiteboard">Status Whiteboard
<OPTION VALUE="keywords">Keywords
<OPTION VALUE="resolution">Resolution
<OPTION VALUE="bug_severity">Severity
<OPTION VALUE="priority">Priority
<OPTION VALUE="component">Component
<OPTION VALUE="assigned_to">AssignedTo
<OPTION VALUE="reporter">ReportedBy
<OPTION VALUE="votes">Votes
<OPTION VALUE="qa_contact">QAContact
<OPTION VALUE="cc">CC
<OPTION VALUE="dependson">BugsThisDependsOn
<OPTION VALUE="blocked">OtherBugsDependingOnThis
<OPTION VALUE="attachments.description">Attachment description
<OPTION VALUE="attachments.thedata">Attachment data
<OPTION VALUE="attachments.mimetype">Attachment mime type
<OPTION VALUE="attachments.ispatch">Attachment is patch
<OPTION VALUE="target_milestone">Target Milestone
<OPTION VALUE="delta_ts">Last changed date
<OPTION VALUE="(to_days(now()) - to_days(bugs.delta_ts))">Days since bug changed
<OPTION VALUE="longdesc">Comment
</SELECT><SELECT NAME="type0-0-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="equals">equal to
<OPTION VALUE="notequals">not equal to
<OPTION VALUE="casesubstring">contains (case-sensitive) substring
<OPTION VALUE="substring">contains (case-insensitive) substring
<OPTION VALUE="notsubstring">does not contain (case-insensitive) substring
<OPTION VALUE="regexp">contains regexp
<OPTION VALUE="notregexp">does not contain regexp
<OPTION VALUE="lessthan">less than
<OPTION VALUE="greaterthan">greater than
<OPTION VALUE="anywords">any words
<OPTION VALUE="allwords">all words
<OPTION VALUE="nowords">none of the words
<OPTION VALUE="changedbefore">changed before
<OPTION VALUE="changedafter">changed after
<OPTION VALUE="changedto">changed to
<OPTION VALUE="changedby">changed by
</SELECT><INPUT NAME="value0-0-0" VALUE=""><INPUT TYPE="button" VALUE="Or" NAME="cmd-add0-0-1" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"><INPUT TYPE="button" VALUE="And" 
	NAME="cmd-add0-1-0" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"></td></tr>
    
		<tr>
		<td colspan="2"><hr></td>
		</tr><tr><td>&nbsp;</td><td>
		<SELECT NAME="field1-0-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="groupset">groupset
<OPTION VALUE="bug_id">Bug #
<OPTION VALUE="short_desc">Summary
<OPTION VALUE="product">Product
<OPTION VALUE="version">Version
<OPTION VALUE="rep_platform">Platform
<OPTION VALUE="bug_file_loc">URL
<OPTION VALUE="op_sys">OS/Version
<OPTION VALUE="bug_status">Status
<OPTION VALUE="status_whiteboard">Status Whiteboard
<OPTION VALUE="keywords">Keywords
<OPTION VALUE="resolution">Resolution
<OPTION VALUE="bug_severity">Severity
<OPTION VALUE="priority">Priority
<OPTION VALUE="component">Component
<OPTION VALUE="assigned_to">AssignedTo
<OPTION VALUE="reporter">ReportedBy
<OPTION VALUE="votes">Votes
<OPTION VALUE="qa_contact">QAContact
<OPTION VALUE="cc">CC
<OPTION VALUE="dependson">BugsThisDependsOn
<OPTION VALUE="blocked">OtherBugsDependingOnThis
<OPTION VALUE="attachments.description">Attachment description
<OPTION VALUE="attachments.thedata">Attachment data
<OPTION VALUE="attachments.mimetype">Attachment mime type
<OPTION VALUE="attachments.ispatch">Attachment is patch
<OPTION VALUE="target_milestone">Target Milestone
<OPTION VALUE="delta_ts">Last changed date
<OPTION VALUE="(to_days(now()) - to_days(bugs.delta_ts))">Days since bug changed
<OPTION VALUE="longdesc">Comment
</SELECT><SELECT NAME="type1-0-0"><OPTION SELECTED VALUE="noop">---
<OPTION VALUE="equals">equal to
<OPTION VALUE="notequals">not equal to
<OPTION VALUE="casesubstring">contains (case-sensitive) substring
<OPTION VALUE="substring">contains (case-insensitive) substring
<OPTION VALUE="notsubstring">does not contain (case-insensitive) substring
<OPTION VALUE="regexp">contains regexp
<OPTION VALUE="notregexp">does not contain regexp
<OPTION VALUE="lessthan">less than
<OPTION VALUE="greaterthan">greater than
<OPTION VALUE="anywords">any words
<OPTION VALUE="allwords">all words
<OPTION VALUE="nowords">none of the words
<OPTION VALUE="changedbefore">changed before
<OPTION VALUE="changedafter">changed after
<OPTION VALUE="changedto">changed to
<OPTION VALUE="changedby">changed by
</SELECT><INPUT NAME="value1-0-0" VALUE=""><INPUT TYPE="button" VALUE="Or" NAME="cmd-add1-0-1" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"><INPUT TYPE="button" VALUE="And" 
	NAME="cmd-add1-1-0" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"></td></tr>
    
		<tr><td>&nbsp;</td><td align="center">
         &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
        <INPUT TYPE="button" VALUE="Add another boolean chart" NAME="cmd-add2-0-0" ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;">
       
        
</td></tr>
</table>
</td></tr>
</table>
</td>
</tr>
</table>


</center>
<br>
<p>The most subtle thing to notice is the "Add another boolean chart" button.
This is almost the same thing as the "And" button.  You want to use this when
you use one of the fields where several items can be associated
with a single bug - including: "Comments", "CC", and all the
"changed [something]" entries.  If you have multiple terms that
all are about one of these fields (such as one comment), it's ambiguous whether they are
allowed to be about different instances of that field or about only that one instance.  So,
to let you have it both ways, they always mean the same instance,
unless the terms appear on different charts.

<p>For example: if you search for "priority changed to P5" and
"priority changed by person\@addr", it will only find bugs where the
given person at some time changed the priority to P5.  However, if
what you really want is to find all bugs where the milestone was
changed at some time by the person, and someone (possibly someone
else) at some time changed the milestone to P5, then you would put
the two terms in two different charts.
};


print qq{
<a name="therest"></a>
<center><h3>The Rest of the Form</h3></center>
<center>


<table width="650" bgcolor="#00afff" border="0" cellpadding="0" cellspacing="0">
<tr>
<td align="center" height="190" >
<table cellspacing="0" cellpadding="0">
<tr>
<td align="left">
<INPUT TYPE="radio" NAME="cmdtype" VALUE="editnamed"> Load the remembered query:
<select name="namedcmd"><OPTION VALUE="Assigned to me">Assigned to me</select><br>
<INPUT TYPE="radio" NAME="cmdtype" VALUE="runnamed"> Run the remembered query:<br>
<INPUT TYPE="radio" NAME="cmdtype" VALUE="forgetnamed"> Forget the remembered query:<br>
<INPUT TYPE="radio" NAME="cmdtype" VALUE="asdefault"> Remember this as the default query<br>
<INPUT TYPE="radio" NAME="cmdtype" VALUE="asnamed"> Remember this query, and name it:
<INPUT TYPE="text" NAME="newqueryname"><br>
<B>Sort By:</B>
<SELECT NAME="order">
<OPTION VALUE="Reuse same sort as last time">Reuse same sort as last time<OPTION VALUE="Bug Number">Bug Number<OPTION VALUE="'Importance'">'Importance'<OPTION VALUE="Assignee">Assignee</SELECT>
</td>
</tr>
</table>
<table>
<tr>
<td>
<INPUT TYPE="button" VALUE="Reset back to the default query">
</td>
<td>
<INPUT TYPE="button" VALUE="Submit query">
</td>
</tr>
</table>
</td>
</tr>
</table>



</center>
<br>
<p>So you have gotten all that down, but "What is this junk at the bottom of the form?" 
You can remember the current query as the default query page that is pulled up whenever you are
logged on. There is also an ability to choose how you want your results sorted. When finished, 
click "Submit".
};




print qq{

<a name="info"></a>

<br><center><h3>About This Document</h3></center>

<p>Written and adapted from some older Bugzilla documents (by Terry Weissman, Tara Hernandez and others) by <a href="mailto:netdemonz\@yahoo.com">Brian Bober</a> 
You can talk to me on irc.mozilla.org - #mozilla, #mozwebtools, #mozillazine, I go by the name netdemon.

<P>For more information than you can find in this document: 
<br><a href="http://www.mozilla.org/quality/help/beginning-duplicate-finding.html\"> 
How To Find Previously Reported Bugs</a><br>
<a href="http://www.mozilla.org/bugs/">Bugzilla General Information</a><br>
<a href="http://www.mozilla.org/quality/help/bugzilla-helper.html">Mozilla Bug Report Form</a><br>
<a href="http://www.objective2k.co.uk/goodmozillauser.htm">Sliver's How To Be a Good Mozilla User</a><br>
<a href="http://www.mozilla.org/bugs/text-searching.html">Bugzilla Text Searching</a><br>
<a href="http://www.mozilla.org/quality/bug-writing-guidelines.html">The Bug Reporting Guidelines</a><br>
<p>My main motive for writing this was to help the engineers by giving new Bugzilla users a way to learn how to use the Bugzilla Query form. I 
had done a rewrite of query.cgi, so I said, "What the heck, I'll write this too".

<p><br><center><h3>Why Use This?</h3></center>

<p>You probably looked at the Query page and said, "This page looks too difficult. Now that 
I think about it, I don't really need to do a query". It is important to make sure that a bug
doesn't have a duplicate before submitting it, as is stated clearly in 
<a href="http://www.mozilla.org/quality/bug-writing-guidelines.html">The Bug Reporting Guidelines</a>. 
The people reading your bugs are busy and usually swamped with bugs. Therefore, you are doing everyone 
a huge favor to search for a duplicate. 

<p><br><center><h3>About Bugzilla</h3></center>

Bugzilla has been around for quite a while. It is a way for bug testers and users of Mozilla to interact with 
engineers. It has been a very important part of the <a href="http://www.mozilla.org/">Mozilla Project</a> and will be for a long time to come. Bugzilla is 
<a href="http://www.mozilla.org/bugs/source.html">open source</a>, 
meaning that other people can use it and modify it if they follow Mozilla's 
license. A good example of is <a href="http://bugzilla.redhat.com/bugzilla/">
Redhat Linux</a>, although they have altered the utility more than most. This will first appear on Bugzilla version 2.12, which is being finalized as 
I write this. Ian Hickson is working on Bugzilla version 3.0 - a total rewrite with a much better structure and more functionality. 

<p><br><center><h3>The Database</h3></center>


<p>The blood and guts of Bugzilla are the over 50,000 bugs (and growing in number quickly) 
that exist in the Bugzilla Database. That is why the form is so complicated. If there were only 500 bugs, then a simple text search would probably be enough. This 
form will help you sift through all these bugs to find the one you are looking for.

};








print qq{
<a name="samplequery"></a>
<p><br><center><h3>Sample Query</h3></center>

<p>Ok. <b>So lets find a bug!</b> First, lets make a <a target="_blank" href="query.cgi">
copy</a> of the query window so you can easily switch between 
this document and the query.
<p>Do the following:
<ul>
<li>Go to the status field in Bug Settings and select all.
<li>In Text Search options, put Autoscroll in the summary and Panning in the description entry box 
(meaning that panning is somewhere in the comments and the bug's summary has Autoscroll in it).
</ul>
<p>One of the results should have been <a href="http://bugzilla.mozilla.org/show_bug.cgi?id=22775">bug 22775 - [RFE] AutoScroll/Panning support...</a>
};

print qq{
<hr>
If you like this document, then please buy my book: <b>Bugzilla: Mozilla's Flu Shot</b>
<br>It's 1521 pages of Bugzilla excitement! You will not regret it. At your local bookstore now!

<a name="bottom"></a>

</form>

};



PutFooter();
