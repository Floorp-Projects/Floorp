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
    <div id="side">
    <?php
    $type="T";
    $types = array("E"=>"Extensions","T"=>"Themes");
    $typename = $types["$type"];
    $uriparams_skip="category";

    echo"<ul id=\"nav\">\n";

    echo"        <li"; if (!$category AND $index !="yes") { echo" class=\"selected\""; }  echo"><A HREF=\"showlist.php?".uriparams()."&amp;category=All\" TITLE=\"Show All ".ucwords($typename)." Alphabetically\"><strong>All Themes</strong></A></li>\n";

    echo"        <li><ul>\n";

    // Object Categories
    $sql = "SELECT `CatName`,`CatDesc` FROM `categories` WHERE `CatType` = '$type' and `CatApp` = '$application' ORDER BY `CatName`";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        while ($row = mysql_fetch_array($sql_result)) {
            $catname = $row["CatName"];
            $catdesc = $row["CatDesc"];
            echo"        <li"; if (strtolower($category) == strtolower($catname)) { echo" class=\"selected\""; }  echo"><a href=\"showlist.php?".uriparams()."&amp;category=$catname\" title=\"$catdesc\">$catname</a></li>\n";
        }

    echo"        </ul></li>\n";

    $catname = "Editors Pick";
    $catdesc = ucwords($typename)." picked by the Mozilla Update Editors";
    echo"    <li"; if (strtolower($category) == strtolower($catname)) { echo" class=\"selected\""; }  echo"><a href=\"showlist.php?".uriparams()."&amp;category=$catname\" title=\"$catdesc\"><strong>Editor's Pick</strong></a></li>\n";

    $catname = "Popular";
    $catdesc = ucwords($typename)." downloaded the most over the last week.";
    echo"    <li"; if (strtolower($category) == strtolower($catname)) { echo" class=\"selected\""; }  echo"><a href=\"showlist.php?".uriparams()."&amp;category=$catname\" title=\"$catdesc\"><strong>$catname</strong></a></li>\n";

    $catname = "Top Rated";
    $catdesc = ucwords($typename)." rated the highest by site visitors";
    echo"    <li"; if (strtolower($category) == strtolower($catname)) { echo" class=\"selected\""; }  echo"><a href=\"showlist.php?".uriparams()."&amp;category=$catname\" title=\"$catdesc\"><strong>$catname</strong></a></li>\n";

    $catname = "Newest";
    $catdesc = "Most recent ".ucwords($typename);
    echo"<li"; if (strtolower($category) == strtolower($catname)) { echo" class=\"selected\""; }  echo"><a href=\"showlist.php?".uriparams()."&amp;category=$catname\" title=\"$catdesc\"><strong>$catname</strong></a></li>\n";
    ?>
    </ul>
    </div>