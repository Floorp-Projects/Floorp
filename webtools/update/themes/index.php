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
<?php
require"../core/config.php";
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">

<head>
<TITLE>Mozilla Update :: Themes - Change the Look of Mozilla Software</TITLE>

<?php
include"$page_header";
?>

<div id="mBody">
    <?php
    $index="yes";
    include"inc_sidebar.php";
    ?>

	<div id="mainContent">
	<h2><?php print(ucwords($application)); ?> Themes</h2>
	<p class="first">Themes are skins for <?php print(ucwords($application)); ?>, they allow you to change the look and feel of the browser and personalize it to your tastes. A theme can simply change the colors of <?php print(ucwords($application)); ?> or it can change every piece of the browser appearance.</p>

    <?php
    //Get Current Version for Detected Application
    $sql = "SELECT `Version`, `major`, `minor`, `release`, `SubVer` FROM `applications` WHERE `AppName` = '$application' AND `public_ver` = 'YES'  ORDER BY `major` DESC, `minor` DESC, `release` DESC, `SubVer` DESC LIMIT 1";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        $row = mysql_fetch_array($sql_result);
            $version = $row["Version"];
            $subver = $row["SubVer"];
            $release = "$row[major].$row[minor]";
            if ($row["release"]) {
                $release = ".$release$row[release]";
            }
        $currentver = $release;
        $currentver_display = $version;
        unset($version,$subver,$release);
    ?>

	<h2>Top Rated <?php print(ucwords($application)); ?> Themes</h2>
	<p class="first">Ratings are based on feedback from people who use these themes.</p>
	<ol>

    <?php
    $r=0;
    $sql = "SELECT TM.ID, TM.Name, TM.Description, TM.Rating
        FROM  `main` TM
        INNER  JOIN version TV ON TM.ID = TV.ID
        INNER  JOIN applications TA ON TV.AppID = TA.AppID
    WHERE  `Type`  =  '$type' AND `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND `Rating` > '0' AND `approved` = 'YES' GROUP BY `Name` ORDER BY `Rating` DESC, `Name` ASC, TV.Version DESC";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        if (mysql_num_rows($sql_result)=="0") {
            echo"        <li>No Top Rated Themes</li>\n";
        }
        while ($row = mysql_fetch_array($sql_result)) {
            $r++;
            $s++;
            $id = $row["ID"];
            $name = $row["Name"];
            $description = $row["Description"];
            $rating = $row["Rating"];


            echo"		<li>";
            echo"<a href=\"moreinfo.php?".uriparams()."&amp;id=$id\"><strong>$name</strong></a>, $rating stars<br>";
            echo"$description";
            echo"</li>\n";

            if ($r >= "5") {
                break;
            }
        }
        unset($usednames, $usedversions, $r, $s, $i);
        ?>

	</ol>
	<h2>Most Popular <?php print(ucwords($application)); ?> Themes</h2>
	<p class="first">The most popular downloads over the last week.</p>
	<ol>
        <?php
        $i=0;
        $sql = "SELECT TM.ID, TM.Name, TM.Description, TM.TotalDownloads, TM.downloadcount FROM  `main` TM
            INNER  JOIN version TV ON TM.ID = TV.ID
            INNER  JOIN applications TA ON TV.AppID = TA.AppID
            INNER  JOIN os TOS ON TV.OSID = TOS.OSID
            WHERE  `Type`  =  '$type' AND `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND (`OSName` = '$OS' OR `OSName` = 'ALL') AND `downloadcount` > '0' AND `approved` = 'YES' ORDER BY `downloadcount` DESC ";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
            if (mysql_num_rows($sql_result)=="0") {
                echo"        <li>No Popular Themes</li>\n";
            }
            while ($row = mysql_fetch_array($sql_result)) {
                $i++;
                $id = $row["ID"];
                $vid = $row["vID"];
                $name = $row["Name"];
                $description = $row["Description"];
                $downloadcount = $row["downloadcount"];
                $totaldownloads = $row["TotalDownloads"];
                $typename="themes";
                if ($lastname == $name) {
                    $i--;
                    continue;
                }

                echo"		<li>";
                echo"<a href=\"/$typename/moreinfo.php?".uriparams()."&amp;id=$id\"><strong>$name</strong></a>, $downloadcount downloads<br>";
                echo"$description";
                echo"</li>\n";

                $lastname = $name;
                if ($i >= "5") {
                    break;
                }
            }
        ?>
	</ol>
	<a href="/rss/?application=<?php echo"$application"; ?>&amp;type=T&amp;list=newest"><img src="../images/rss.png" width="16" height="16" class="rss" alt="News Additions in RSS"></a>
	<h2>Newest <?php print(ucwords($application)); ?> Themes</h2>
	<p class="first">New and updated themes. Subscribe to <a href="/rss/?application=<?php echo"$application"; ?>&amp;type=T&amp;list=newest">our RSS feed</a> to be notified when new themes are added.</p>
	<ol>

        <?php
        $i=0;
        $sql = "SELECT TM.ID, TM.Type, TM.Description, TM.Name, TV.Version, TV.DateAdded
            FROM  `main` TM
            INNER  JOIN version TV ON TM.ID = TV.ID
            INNER  JOIN applications TA ON TV.AppID = TA.AppID
            INNER  JOIN os TOS ON TV.OSID = TOS.OSID
            WHERE  `Type`='$type' AND `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND (`OSName` = '$OS' OR `OSName` = 'ALL') AND `approved` = 'YES' ORDER BY `DateAdded` DESC ";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
            if (mysql_num_rows($sql_result)=="0") {
                echo"        <li>Nothing Recently Added</li>\n";
            }
            while ($row = mysql_fetch_array($sql_result)) {
                $i++;
                $id = $row["ID"];
                $type = $row["Type"];
                $name = $row["Name"];
                $description = $row["Description"];
                $version = $row["Version"];
                $dateadded = $row["DateAdded"];
                $dateadded = gmdate("F d, Y", strtotime("$dateadded")); 
                //$dateupdated = gmdate("F d, Y g:i:sa T", $timestamp);
                $typename = "themes";

                if ($lastname == $name) {
                    $i--;
                    continue;
                }

                echo"		<li>";
                echo"<a href=\"/$typename/moreinfo.php?".uriparams()."&amp;id=$id\"><strong>$name $version</strong></a>, $dateadded<br>";
                echo"$description";
                echo"</li>\n";

                $lastname = $name;
                if ($i >= "8") {
                    break;
                }
            }
        ?>
	</ol>
	</div>
</div>

<!-- closes #mBody-->

<?php
include"$page_footer";
?>
</body>
</html>