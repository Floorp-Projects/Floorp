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
#                         Terry Weissman <terry@mozilla.org>
#                         Tara Hernandez <tara@tequilarista.org>

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

<p><center><b><font size="+2">Help Using The Bugzilla Query Form</font></b><br>January, 20 2001 - 
<a href="mailto:netdemonz\@yahoo.com">Brian Bober (netdemon)</a>.  
<BR><I>Further heavy mutiliations by <a href="mailto:tara\@tequilarista.org">Tara Heranandez</A>, April 20, 2001.</I></CENTER>

<br><center><img width="329" height="220" src="ant.jpg" border="2" alt="Da Ant"></center>

<p><br><center><h3>The Sections</h3></center>

<p>The query page is broken down into the following sections:

<p><a href="#bugsettings">Bug Settings</a> 
<br><a href="#peopleinvolved">People Involved</a> 
<br><a href="#textsearch">Text Search Options</a>
<br><a href="#moduleoptions">Module Options</a> 
<br><a href="#advancedquerying">Advanced Querying</a>
<br><a href="#therest">The Bottom Of The Form</a>

<p>&quot;I already know how to use <a href="http://www.mozilla.org/bugs/">Bugzilla</a>, but would like <a href="#info">information</a> about Bugzilla and the author of this document.&quot;
<br>&quot;Ok, I am almost certain the bug I discovered isn't in Bugzilla, how do I <a href="enter_bug.cgi">submit</a> the bug?&quot; - <a href= "docs/html/Bugzilla-Guide.html#BUG_WRITING">Read the guidelines first</a>!

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
<BR><B>Note:</B>Because comments can get quite extensive in bugs, doing this particular type
of query can take a long time. 

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

ConnectToDatabase();

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












my %default;
my %type;

print qq{
<a name="moduleoptions"></a>
<p><br><center><h3>Module Options</h3></center>

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

$tableheader =         qq{ <p><table border=0><tr><td>
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
phasing out of bugs occur and a relatively stable release is produced. For example, Mozilla.org had milestones in the
form of "M10" or "M18", but now are in the form of "Mozilla0.9".  Bugzilla milestones are in the form of "Bugzilla 2.12",
"Bugzilla 2.14", etc.


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

<P>Lots of Bugzilla use documention is available through Mozilla.org and other sites:
<br><a href="http://www.mozilla.org/quality/help/beginning-duplicate-finding.html\"> 
How To Find Previously Reported Bugs</a><br>
<a href="http://www.mozilla.org/bugs/">Bugzilla General Information</a><br>
<a href="http://www.mozilla.org/quality/help/bugzilla-helper.html">Mozilla Bug Report Form</a><br>
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



};








print qq{
<a name="samplequery"></a>
<p><br><center><h3>Sample Query</h3></center>

<p>Ok. <b>So lets find a bug!</b>  We'll borrow the Mozilla.org database because it's handy. 
<BR>First, lets make a <a target="_blank" href="http://bugzilla.mozilla.org/query.cgi">
copy</a> of the query window so you can easily switch between this document and the query.
<p>Do the following:
<ul>
<li>Go to the "Status" field in and select all fields (or deselect all fields).
<li>In Text Search options, put Autoscroll in the summary and Panning in the description entry box 
(meaning that panning is somewhere in the comments and the bug's summary has Autoscroll in it).
</ul>
<p>One of the results should have been <a href="http://bugzilla.mozilla.org/show_bug.cgi?id=22775">bug 22775 - [RFE] AutoScroll/Panning support...</a>
};

print qq{
<hr>

<a name="bottom"></a>

</form>

};



PutFooter();
