#!/usr/bin/perl -w
# -*- perl -*-







$doc=<<EOF

<!doctype html public "-//w3c//dtd html 4.0 transitional//en">
<html>
<head>
   <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
  <STYLE TYPE="text/css"><!--
A    
{color:blue; text-decoration:none;}
A:hover
{color:blue; text-decoration:underline;}

H1, H2, H3, H4 {
   font-family:      Arial, Helvetica ;   
   background-color: #e0e0e0 ;
}

H3 {
   margin-left: 15pt ;
   margin-right: 15pt ;
}

H4 {
   margin-left: 30pt ;
   margin-right: 30pt ;
}

P.Norm
{
    margin-left: 40pt ;
    margin-right: 40pt ;
    font-family: Arial, Helvetica ; 
}
P.Norm UL, P.Norm OL, P.Norm DL,  P.Norm TABLE 
{
    margin-left: 50pt ;
    margin-right: 50pt ;
    font-family: Arial, Helvetica ; 
}
P.Norm TABLE TR TH 
{
        background-color: white ;
}
P.Hint, DIV.Hint {
    margin-left: 40pt ;
    margin-right: 40pt ;
    text-decoration:border ;
    border-width: 2pt ;
    border-color: yellow;
    color: #804040 ;
    background-color: white ;
    foreground-color: #f0e0e0 ;
    font-family: Arial, Helvetica ; 
}

P.Note, DIV.Note {
   margin-left: 80pt ;
   margin-right: 80pt ;
   background-color: white ;
   font-weight: bold ;
   border: 5pt dashed red ;
}

FONT.Note {
   font-weight: bold ;
}

--></STYLE>
</head>
<body text="#000000" bgcolor="#FFFFFF" link="#0000FF" vlink="#000099" alink="#FF0000">

<hr SIZE=8 NOSHADE WIDTH="99%" align=center>

<h1 align=center>
P4DB Help File
</h1>

<p align=center>
<b><tt><blink><font color="#FF0000" size=+3>Under construction</font></blink></tt></b>
</p>

<h2>Introduction</h2>

<P class=Norm>
    This page is a brief user manual for P4DB.

<P class=Norm>
    The reader is assumed to have working knowledge of Perforce P4 and web
    browsers.
</p>

<h3>What is P4DB?</h3>

<P Class=Norm>
    P4DB is a http depot browser for <a href="http://www.Perforce.com">Perforce
    p4</a>. The depot browser is implemented as a set of CGIs that present
    information about the status of the p4 depot using HTML.
</P>
<P Class=Norm>
    The purpose of P4DB is to provide a convenient way to browse the p4
    depot for p4 administrators, project managers, developers, testers etc.
    that need information about changes in the depot.
    <br>
    Some typical uses for P4DB are:
    <ul Class=Norm>
        <li>View diff between file revisions</li>
	<li>Find out when a line in a file was changed</li>
	<li>View labels</li>
	<li>List changes between labels or for a specific set of
	files</li>
	<li>Stay abreast with the development</li>
    </ul>
</P>
<P Class=Note>
    P4DB can only present information in the depot and have
    no access to the files in the users file space.
</P>
<P Class=Norm>
    P4DB use the standard p4 commands to retrieve information from the depot.
    P4DB does not add any other information but will often combine output
    from more than one command to get the information.
</P>
<P Class=Norm>
    P4DB produces almost only plain html. No javascript, very little CSS
    (Cascades Style Sheets), one gif. P4DB use one http cookie to store user
    preferences.
</P>

<h3>P4DB Support and Supported Platforms</h3>

<P Class=Norm>
P4DB is developed using Netscape Communicator 4.7 browser and
Apache http server. Both browser and server run on a Linux system.
<br>I perform some basic testing with Explorer but that's it.

<P Class=Norm>
There are no supported platforms basically because there is no support.
P4DB is open source software and I expect the brave p4 administrator that
installs P4DB to provide support for end users. I will of course answer
questions if I can and is more than willing to listen to suggestions or
constructive criticism but please forward all this through your administrator.
</P>


<h3>How Do I.....? (a.k.a. FAQ)</h3>

<P Class=Norm>
Here is examples on how to use P4DB to answer some questions
about files in the depot. The list is by no means complete but should give
some idea of what P4DB can be used for.

<h4>Is there any help available?</h4>
<P Class=Norm>
Sure! You are looking at it, only it's not finished yet. This help page
can be reached in two ways:
<ol>
   <li>Click on the help link at the top right corner of each page.</li>
      <br>This will usually bring you to the part in the help file that describes
      this current page.
   <li>In some places in there are help links (currently a question mark but this
       may change into something more obvious).
       <br>These links brings you directly to the paragraph in the help file that
       describes the adjacent feature or selection.
</ol>

<h4>
How do I view the latest changes in the depot?</h4>
<P Class=Norm>
Easy! But there are several ways:
<ul>
<li>
By far the easiest is to click on "Submitted Changes" in the main
page. This will show the submitted changes in reverse chronological
order, that is the latest on top.
<li>
If you are <FONT Class=Note>not interested in the whole depot</FONT> you
could  select "Browse 
Depot Tree" on the main page. Using the browser you can move down the
depot tree and select "View changes".
<li>
If you are interested in a <FONT Class=Note>special file or set of
files</FONT> you could 
enter a file spec (in p4 depot notation) in the "List Changes For
File spec" field in the main page. If you, for example, enter
<tt>//.../main/.../*.doc</tt> you will see the changes for all files
with extension "<tt>.doc</tt>" below a directory named "<tt>main</tt>" 

<br>You can change file spec to anything and even use some wildcards. P4DB
accepts the perforce style wildcards ("<tt>...</tt>" for a series of any
characters and "<tt>*</tt>" for any character except "<tt>/</tt>").
<DIV Class=Hint>
<b>Hint: </b>P4DB lets you specify more than one file spec here. For
example "<tt>//...*.c + //...*.h</tt>" will show changes in and *.c and
*.h files. Multiple file specifications are separated with "&lt;optional
whitespace> + &lt;optional whitespace>".</DIV><br>
<li>
Sometimes you may be interested in changes made by a 
<FONT Class=Note>specific user or group of users</FONT>. Select
"Changes by User or Group" on the main page. You will get a page with
a list of users and a list of user groups (if any). Select the desired
users and groups and click on OK. 
<DIV Class=Hint>
In windows Ctrl-Mouseclick will let you make an arbitrary selection in
the list.</DIV><br>

<li>
A special case is when you are looking for changes with a specific
<FONT class=note>keyword in the  description</font>. If you select
"Search Changes by Descriptions" on the main page you will get a page
where you can specify a file spec and a pattern to search for. 
<li>
There may be a "shortcut" (see below) that views the changes for the
whole depot or the part of the depot you are interested in.</li>
</ul>
<P Class=Norm>
And there are more ways. If you browse around in P4DB you will see
that for virtually all items, like labels or branches,  that refer to
the depot you can select "View changes".

<h4>
"Shortcuts", what's that?</h4>

<P Class=Norm>
You administrator may create one or mote of shortcut files where each
file contains a set of shortcuts that are presented as links on the main
page. See the "Set Preferences" page to select a shortcut file, if there
are any shortcut files defined.</P>

<h4>
How to I view history for a file</h4>
<P Class=Norm>
There are many ways here too.
<UL>
<li>On the <font class=note>main page</font> you can <font
class=note>search for the file</font>. Enter the file name in the
"Search for file" field and press OK. You will now get a list of files
that matches your search. Click on the desired file to view the file
log for the file.
<li>You can <font class=note>browse the depot</font> with the "Browse
Deport" link on the main page. When the desired directory is found
just click on the file name.
</UL>
<P Class=Norm>
In general, in all pages that show a file name a click on the file
name will show the file log.

<br>
<br>
<center><hr SIZE=8 NOSHADE WIDTH="90%"></center>

<h1 align=center>HELP FOR PAGES</h1>

<h2>Common to all pages</h2>
<h3>Page header</h3>
<P Class=Norm>
To the left in the header the P4DB version information and the current
change level in the depot.
<br>
At the top center of each page is a title that describes the content
of the page. 
<br>
To the top left of the page there is a link to the paragraph in this
document that describes the current page. 
<br>
Below the current level and page title there is a field that contains
some more information about the page and sometimes links that links to
information related to the page or change the information displayed on
the page.
<br>
Below the help link there is a link to the main page, except on the
main page where this place is empty.

<h3>Page Footer</h3>
<P Class=Norm>
To the bottom left the current p4 port and host is displayed.
<br>
To the right there is one or more mail-to-links to the administrator(s)
and to the far right is a link to the page top.

<h2><a NAME="index"></a><a href="index.cgi">Main
page (or index page)</a></h2>
<P class=norm>
This is the start page for P4DB. The start page contains four
sections:
<h4>
Links to browser pages</h4>
<P class=norm>
A set of links to pages that lets you browse the depot, view pending changes,
view branches or labels etc.
<br>For detailed information click on the link and select help in the page
header.
<h4>
(Optional) Shortcuts</h4>
<P class=norm>
This section will only exist if the P4DB administrator have created shortcut
file and you have selected one in "Set Preferences".
<br>The section contains a set of shortcut links to various pages.
<h4>
Advanced searches</h4>
<P class=norm>
This section contains a set of advanced searches with text fields for user
input.

<h4>
<a NAME="index_listCh"></a>List changes for file spec</h4>
<P class=norm>
View changes for the file spec.
<br>The file spec may contain wildcards (p4 style).
<br>Two or more file specifications may be specified if they are separated
by a "<tt>+</tt>"-sign.
<h4>
<a NAME="index_fileSrch"></a>Search for file</h4>
<P class=Norm>
View all files in the depot matching a file specification. The file specification
will typically contain wildcards (p4 style).
<h4>
<a NAME="index_viewCh"></a>View change</h4>
<P class=Norm>
View a change specified by change number.

<h4>
Miscellaneous</h4>
<P class=Norm>
This section contains three links.
<ol>
<li>
A link to a documentation page targeting the system administrators.</li>

<li>
A link to the "Set Preferences" page</li>

<li>
A link to "The Great Submit Race".</li>
</ol>

<h2>

<a NAME="SetPreferences"></a><a href="SetPreferences.cgi">Set Preferences</a></h2>

<P class=norm>
P4DB allows the user to set some preferences. Some of the choices
are defined in the configuration file by the P4DB administrator.
<P Class=Norm>
The preferences are:
<P Class=Norm>
<TABLE Class=Norm BORDER>
<tr class=Norm NOSAVE>
<th class=Norm NOSAVE>P4 Depot</th>

<td class=Norm NOSAVE>Select depot from a set of depots defined in the configuration
file by the P4DB administrator. A typical installation will probably only
have one single depot to choose.</td>
</tr>

<tr NOSAVE>
<th NOSAVE>Shortcut file to use</th>

<td NOSAVE>The P4DB administrator can define a set of shortcut files that
provides handy shortcuts for the user.</td>
</tr>

<tr NOSAVE>
<th NOSAVE>Shortcuts on top of main page</th>

<td>If a shortcut file is selected the shortcuts can be displayed on top
of page or not.</td>
</tr>

<tr NOSAVE>
<th NOSAVE>Ignore case (like NT)</th>

<td>Set to "yes" if you work in a Windows environment.</td>
</tr>

<tr NOSAVE>
<th NOSAVE>Max changes to show</th>

<td>When changes are listed it is convenient to limit the number of changes
per page or the page might take very long to load. I have found 100 to
be a reasonable number in a fast LAN and probably about 20 or so if you
work over modem.</td>
</tr>

<tr NOSAVE>
<th NOSAVE>Default tab stop</th>

<td>Default tab stop for your source code. If not set correctly the indentation
will look funny when you view your source code.</td>
</tr>

<tr NOSAVE>
<th NOSAVE>View files with colors</th>

<td>The file viewer in P4DB can colorize HTML, perl and C code. Set this to Yes to turn 
this function on. Set to off (No) if you want to use a color scheme with dark background.</td>
</tr>

<tr NOSAVE>
<th NOSAVE>Underline links</th>

<td>Some people want their links underlined some don't.</td>
</tr>

<tr NOSAVE>
<th NOSAVE>Color scheme</th>

<td>Select a color scheme to use. There are a few predefined to choose from.</td>
</tr>

<tr NOSAVE>
<th NOSAVE>Enable experimental java depot tree browser</th>

<td>Once I wrote this small java applet to get a GUI to go with P4DB. Does
not work in all installations but neat to have when it does.</td>
</tr>

<tr NOSAVE>
<th NOSAVE>Print log information</th>

<td>Print a log at the bottom of each page. Really only useful for debugging.</td>
</tr>
</table>


<h2>
<a NAME="depotTreeBrowser"></a>
<a href="depotTreeBrowser.cgi">Depot Tree Browser</a></h2>

<P class=norm>
Used to browse the file tree in the depot.
<br>The Depot Tree Browser presents data in three sections.
<ol>
<li>
The top section shows the current directory in the depot.</li>

<br>Click on a directory in the path to ascend to it. 
<li>
The middle section shows the currently available subdirs.</li>

<br>Click on a subdir to descend to it.
<li>
The bottom section shows the files in the current directory. Deleted files
are marked with a strike through style.

<br>The file name links to the file log for the file. The version number
links to the file viewer for latest version.
</ol></li>

<P Class=Norm>
In the top of the page there is a link to view changes below current
directory and a link to toggle show/hide deleted files.

<h2>
<a NAME="userList"></a><a href="userList.cgi">P4 Users (and Groups)</a></h2>

<P Class=Norm>
Lists all users and user groups.
<P Class=Norm>For each user the p4-user-name. name as specified in user description,
email address and last access time is displayed. If the user has not been
accessed for more than 10 weeks a message about this is printed.
<P Class=Norm>Click on p4 user name to get details about user.
<br>Click on e-mail address to send email to user.
<P Class=Norm>If any groups are defined they are listed below the users. Click on
group name to see details about group.


<h2>
<a NAME="changeList"></a><a href="changeList.cgi?FSPC=//...">Change List</a></h2>

<P Class=Norm>
This page lists changes with different selection criteria.
The selection criteria is displayed in the header.
<br>Since the list may be very long it is limited to a specific number
of changes (see <a href="#SetPreferences">Set Preferences</a>). If there
are more changes there is a "More..." link at the bottom of the page.
<ul>
<li>
Each change is presented as a change number, date, user and client followed
by the description.</li>

<li>
The change number is a link to view details about the change. There is
a switch in the preferences (see <a href="#SetPreferences">Set Preferences</a>)
that makes the change show up in separate window.</li>

<li>
The user and client links to user and client views.</li>

<li>
If change number are mentioned in the description they are converted to
a link to that specific change, e.g. "change 123" is converted to "<a href="changeView.cgi?CH=123">change
123</a>". <br>P4DB parses the description and tries to find references to
other changes to convert to links. The algorithm assumes english and
can sometimes mistake other numbers in the description for references
to changes.</li>
</ul>
<P Class=Norm>
At the top of the page is a link that automatically lists the change descriptions
for change the numbers referred to in the change description (try it and
you will understand what I mean).

<h2>
<a NAME="searchPattern"></a><a href="searchPattern.cgi">Search Descriptions</a></h2>


<P Class=Norm>
Search changes for text pattern in description.
<P Class=Norm>
The pattern fields contains a pattern that is searched for in the change
descriptions.
<P Class=Norm>
The search is not case sensitive and the following wildcard characters
are accepted:
<dl>
<dt>
&lt;white space></dt>

<dd>
Matches one or more whitespace characters or newlines</dd>

<dt>
*</dt>

<dd>
Matches zero or more characters</dd>

<dt>
?</dt>

<dd>
Matches exactly one character</dd>
</dl>


<h2>
<a NAME="DepotStats"></a><a href="depotStats.cgi">Depot Statistics</a></h2>

<P Class=Norm>
Shows some statistics from the depot or part of the depot.
<ul>
<li>
The first part shows general statistics from the depot such as counters
and number of users, clients etc.</li>

<li>
The second part shows submit and files statistics for the selected part
of the depot</li>

<li>
The third part shows weekly submit rate for the selected part of the depot.
Weeks are here defined as Sunday to Saturday.</li>

<li>
The fourth parts shows number of submits per user sorted by number of submits</li>
</ul>
<P Class=Norm>
I can't honestly say what all this is good for other than satisfying your
curiosity.


<h2>
<a NAME="P4Race"></a><a href="p4race.cgi">The Great Submit Race</a></h2>

<P Class=Norm>
This is the great submit race. The Submit Race reads the last
499 submits and ranks the users after the number of submits.<br>
There is also some assessment of the "speed" based on the average position
of the users submits in the list.
<br>The last user among the 499 changes gets her submit number in red.
This means that this number will be decremented the next time somebody submits
a change.
<br>The user that made the most recent submit gets a green background to
her submit number.
<P Class=Norm>To be able to enjoy the race there is a link that replays the last 30
submits.
<P Class=Norm>Please don't take the Submit Race to seriously.....
<br>
<br>
<center><hr SIZE=8 NOSHADE WIDTH="99%"></center>

</body>
</html>

EOF

;

print $doc
