<?php
// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
//
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
//
// The Original Code is Mozilla Update.
//
// The Initial Developer of the Original Code is
// Chris "Wolf" Crews.
// Portions created by the Initial Developer are Copyright (C) 2004
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
//   Chris "Wolf" Crews <psychoticwolf@carolina.rr.com>
//
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
//
// ***** END LICENSE BLOCK *****
?>
<DIV class="sidelinks">
<?php
echo"<DIV CLASS=\"sidebartitle\">Categories:</DIV>\n";
echo"<DIV class=\"sidebartext\">";
if (!$category AND $index !="yes") {echo"<SPAN CLASS=\"title\">"; }
  echo"<SPAN class=\"sidebartitle\"><A HREF=\"showlist.php?category=All\" TITLE=\"Show All ".ucwords($typename)." Alphabetically\">All</A></SPAN><BR>\n";
if (!$category AND $index !="yes") {echo"</SPAN>"; }

// Object Categories
$sql = "SELECT `CatName`,`CatDesc` FROM `t_categories` WHERE `CatType` = '$type' ORDER BY `CatName`";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
  $catname = $row["CatName"];
  $catdesc = $row["CatDesc"];
  if (strtolower($category) == strtolower($catname)) {echo"<SPAN CLASS=\"title\">"; }
  echo"<A HREF=\"showlist.php?category=$catname\" TITLE=\"$catdesc\">$catname</A><BR>\n";
  if (strtolower($category) == strtolower($catname)) {echo"</SPAN>"; }
  }
?>
<BR>
<?php
$catname = "Editors Pick";
$catdesc = ucwords($typename)." picked by the Mozilla Update Editors";
if (strtolower($category) == strtolower($catname)) {echo"<SPAN CLASS=\"title\">"; }
  echo"<A HREF=\"showlist.php?category=$catname\" TITLE=\"$catdesc\">Editor's Pick</A><BR>\n";
if (strtolower($category) == strtolower($catname)) {echo"</SPAN>"; }

$catname = "Popular";
$catdesc = ucwords($typename)." downloaded the most";
if (strtolower($category) == strtolower($catname)) {echo"<SPAN CLASS=\"title\">"; }
  echo"<A HREF=\"showlist.php?category=$catname\" TITLE=\"$catdesc\">$catname</A><BR>\n";
if (strtolower($category) == strtolower($catname)) {echo"</SPAN>"; }

$catname = "Top Rated";
$catdesc = ucwords($typename)." rated the highest";
if (strtolower($category) == strtolower($catname)) {echo"<SPAN CLASS=\"title\">"; }
  echo"<A HREF=\"showlist.php?category=$catname\" TITLE=\"$catdesc\">$catname</A><BR>\n";
if (strtolower($category) == strtolower($catname)) {echo"</SPAN>"; }

$catname = "Newest";
$catdesc = "Most recent ".ucwords($typename);
if (strtolower($category) == strtolower($catname)) {echo"<SPAN CLASS=\"title\">"; }
  echo"<A HREF=\"showlist.php?category=$catname\" TITLE=\"$catdesc\">$catname</A><BR>\n";
if (strtolower($category) == strtolower($catname)) {echo"</SPAN>"; }
?>
</DIV>
</DIV>