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
//   Alan Starr <alanjstarr@yahoo.com>
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
<?php
//  http://blogs.law.harvard.edu/tech/rss

switch ($type) {
  case "E":
    $listType = "Extensions";
    break;
  case "P":
    $listType = "Plugins";
    break;
  case "T":
    $listType = "Themes";
    break;
}

echo "<rss version=\"2.0\">\n";
echo "<channel>\n";

echo "  <title>" . htmlentities($sitetitle) . "::" . htmlentities($list) . " " . $listType . "</title>\n";
echo "  <link>" . htmlentities($siteurl) . "</link>\n";
echo "  <description>" . htmlentities($description) . "</description>\n";
echo "  <language>" . htmlentities($sitelanguage) . "</language>\n";
echo "  <copyright>" . htmlentities($sitecopyright) . "</copyright>\n";
echo "  <lastBuildDate>" . $currenttime . "</lastBuildDate>\n";
echo "  <ttl>" . $rssttl . "</ttl>\n";
echo "  <image>\n";
echo "    <title>" . htmlentities($sitetitle) . "</title>\n";
echo "    <link>" . htmlentities($siteurl) . "</link>\n";
echo "    <url>" . htmlentities($siteicon) . "</url>\n";
echo "    <width>16</width>\n";
echo "    <height>16</height>\n";
echo "  </image>\n";

 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
    $id = $row["ID"];
    $title = htmlentities($row["Title"]);
    $description = htmlentities($row["Description"]);
    $dateupdated = gmdate("r", strtotime($row["DateStamp"]));
    $version = $row["Version"];
    $vid = $row["vID"];
    $appname = $row["AppName"];

    echo "    <item>\n";
    echo "      <pubDate>" . $dateupdated . "</pubDate>\n";
    echo "      <title>" . $title . " " . $version . " for " . $appname . "</title>\n";
    echo "      <link>http://$sitehostname/" . strtolower($listType) . "/moreinfo.php?id=" . $id . "&amp;vid=" . $vid . "</link>\n";
    echo "      <description>" . $description . "</description>\n";
    echo "    </item>\n";

  }

echo "</channel>\n";
echo "</rss>\n";

?>
