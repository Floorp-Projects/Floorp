# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Tinderbox 3.
#
# The Initial Developer of the Original Code is
# John Keiser (john@johnkeiser.com).
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# ***** END LICENSE BLOCK *****

package Tinderbox3::InitialValues;

use strict;

# Tree info
our $field_short_names = 'refcount_leaks=Lk,refcount_bloat=Bl,trace_malloc_leaks=Lk,trace_malloc_maxheap=MH,trace_malloc_allocs=A,pageload=Tp,codesize=Z,xulwinopen=Txul,startup=Ts,binary_url=Binary,warnings=Warn';
our $field_processors = 'refcount_leaks=Graph,refcount_bloat=Graph,trace_malloc_leaks=Graph,trace_malloc_maxheap=Graph,trace_malloc_allocs=Graph,pageload=Graph,codesize=Graph,xulwinopen=Graph,startup=Graph,warnings=Warn,build_zip=URL,installer=URL';
our $statuses = 'open,closed,restricted,metered';
our $min_row_size = 0;
our $max_row_size = 5;
our $default_tinderbox_view = 24*60;
our $new_machines_visible = 1;
our %initial_machine_config = (
  branch => '',
  cvs_co_date => '',
  tests => 'Tp,Ts,Txul',
  cvsroot => ':pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot',
  clobber => 0,
  mozconfig => q^ac_add_options --disable-debug
ac_add_options --enable-optimize
ac_add_options --without-system-nspr
ac_add_options --without-system-zlib
ac_add_options --without-system-png
ac_add_options --without-system-mng
ac_add_options --enable-crypto
^,
);


# Sheriff info
our $header = q^<html>
<head>
<title>Tinderbox - #TREE#</title>
<style>
a img {
  border: 0px
}
body {
  background-color: #DDEEFF
}
table.tinderbox {
  background-color: white;
  width: 100%
}
table.tinderbox td {
  border: 1px solid gray;
  text-align: center;
}
table.tinderbox th {
  border: 1px solid gray;
}
.status0,.status1,.status2 {
  background-color: yellow
}
.status100,.status101,.status102,.status103 {
  background-color: lightgreen
}
th.status200,th.status201,th.status202,th.status203 {
  background: url("http://lounge.mozilla.org/tinderbox2/gif/flames1.gif");
  background-color: black;
  color: white
}
th.status200 a,th.status201 a,th.status202 a,th.status203 a {
  color: white
}
.status200,.status201,.status202,.status203 {
  background-color: red
}
.status300,.status301,.status302,.status303 {
  background-color: lightgray
}
.checkin {
  text-align: center
}
.time {
  text-align: right
}
.time_alt {
  text-align: right;
  background-color: #e7e7e7
}
.obsolete {
  text-decoration: line-through
}
#tree_status {
  font-weight: bold;
  padding: 10px
}
#tree_status span {
  font-size: x-large;
}
#tree_top {
  text-align: center;
  vertical-align: center;
  margin-bottom: 1em;
}
#tree_top span {
  font-size: x-large;
  font-weight: bold
}
#tree_info {
  border-collapse: collapse;
  background-color: white;
  margin-bottom: 1em
}
#tree_info td,th {
  border: 1px solid black
}
#checkin_info {
  border: 1px dashed black;
  background-color: white
}
#info_table td {
  vertical-align: top
}

#popup {
  border: 2px solid black;
  background-color: white;
  padding: 0.5em;
  position: fixed;
}
</style>

<script>
function closepopup() {
  document.getElementById('popup').style.display = 'none';
}
function do_popup(event,_class,str) {
  closepopup();
  var popup = document.getElementById('popup');
  popup.className = _class;
  popup.innerHTML = str;
  popup.style.left = event.clientX;
  popup.style.top = event.clientY;
  popup.style.display = 'block';
  event.preventBubble();
  return false;
}
</script>
</head>
<body>
<table WIDTH="100%" BORDER=0 CELLPADDING=0 CELLSPACING=0 onclick="closepopup()">
<tr><td>

<table id=tree_top><tr>
<td><span>Tinderbox: #TREE#</span> (#TIME#)<br>(<a href='sheriff.pl?tree=#TREE#'>Sheriff</a> | <a href='admintree.pl?tree=#TREE#'>Edit</a>)</td>
<td><a HREF="http://www.mozilla.org/"><img SRC="http://www.mozilla.org/images/mozilla-banner.gif" ALT="" BORDER=0 WIDTH=600 HEIGHT=58></a></td>
</tr></table>

<div id="popup" style="display: none" onclick="event.preventBubble()">
</div>

<table id="info_table">
<tr><td>
<table id="tree_info">
<tr><td colspan=2 id=tree_status>The tree is <span>#STATUS#</span></td></tr>
<tr><th>Sheriff:</th><td>#SHERIFF#</td></tr>
<tr><th>Build Engineer:</th><td>#BUILD_ENGINEER#</td></tr>
<tr><th>CVS pull:</th><td>#CVS_CO_DATE#</td></tr>
<tr><th>Patches:</th><td colspan=3>#PATCHES#</td></tr>
</table>
</td>
<td>
<p id="checkin_info"><strong>Tree Rules: <font color=red>Do not check in on red.</font></strong> Do not checkin without <a href="http://www.mozilla.org/hacking/reviewers.html">r=/sr= and a=</a>. Watch this Tinderbox after checkin to ensure all platforms compile and run.<br>
<strong>Checkin Comments:</strong> When you check in, be sure to include the bug number, who gave you r=/sr=/a=, and a clear description of what you did.</p>
</td>
</tr>
</table>

<div>
<a href='showbuilds.pl?tree=#TREE#&start_time=#START_TIME_MINUS(86400)#'>previous (earlier) period</a> - <a href='showbuilds.pl?tree=#TREE#&start_time=#END_TIME#'>next (later) period</a> - <a href='showbuilds.pl?tree=#TREE#'>current period</a><br>
^;

our $footer = q^<a href='showbuilds.pl?tree=#TREE#&start_time=#START_TIME_MINUS(86400)#'>previous (earlier) period</a> - <a href='showbuilds.pl?tree=#TREE#&start_time=#END_TIME#'>next (later) period</a> - <a href='showbuilds.pl?tree=#TREE#'>current period</a>
</div>
<address>Tinderbox 3: code problems to <a href='mailto:jkeiser@netscape.com'>John Keiser</a>, server problems to <a href='mailto:endico@mozilla.org'>Dawn Endico</a></address>
</td></tr></table>
</body>
</html>^;

our $sheriff = q^<a href='mailto:annlanders@thepost.com'>Miss Manners</a>, IRC: <a href='irc://irc.mozilla.org/#mozilla'>pleasedontshout</a>, AIM: <a href='aim:ChangeYourBracingStyle'>ChangeYourBracingStyle</a>^;
our $build_engineer = q^<a href='mailto:rubyrubydoo@mysteryvan.com'>Scooby Doo</a>, IRC: <a href='irc://irc.mozilla.org/#mozilla'>puppypower</a>, AIM: <a href='aim:scoobysnack'>scoobysnack</a>^;
our $special_message = q^^;

our $status = "open";

#
# bonsai defaults
#
our $display_name = "Mozilla checkins";
our $bonsai_url = "http://bonsai.mozilla.org";
our $module = "SeaMonkeyAll";
our $branch = "HEAD";
our $directory = "";
our $cvsroot = "/cvsroot";


1
