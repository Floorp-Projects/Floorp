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
require"core/config.php";
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">

<head>
 <title>Mozilla Update</title>
 <link rel="alternate" type="application/rss+xml" title="New <?php echo ucwords($application); ?> Additions" href="/rss/?application=<?php echo"$application"; ?>&amp;list=newest">
<?php
include"$page_header";
?>

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

<?php
$securitywarning=false;
if ($securitywarning=="true") {
?>
<!-- Don't display if no urgent security updates -->
<div class="key-point"><p class="security-update"><strong>Important Firefox Security Update:</strong><br>Lorem ipsum dolor sit amet, <a href="#securitydownload">consectetuer adipiscing</a> elit. Curabitur viverra ultrices ante. Aliquam nec lectus. Praesent vitae risus. Aenean vulputate sapien et leo. Nullam euismod tortor id wisi.</p></div>
<hr class="hide">
<!-- close security update -->
<?php } ?>

<div id="mBody">
	<div id="mainContent" class="right">
	<h2>What is Mozilla Update?</h2>
	<p class="first">Mozilla Update is the place to get extras for your <a href="http://www.mozilla.org/">Mozilla</a> products. Learn more <a href="/about/">about us</a>.</p>

    <?php
    $uriparams_skip="application";
    ?>
	<dl>
		<dt>Themes</dt>
		<dd>Themes allow you to change the way your Mozilla program looks. New graphics and colors. Browse themes for: <a href="/themes/?<?php echo"".uriparams()."&amp;"; ?>application=firefox">Firefox</a>, <a href="/themes/?<?php echo"".uriparams()."&amp;"; ?>application=thunderbird">Thunderbird</a>, <a href="/themes/?<?php echo"".uriparams()."&amp;"; ?>application=mozilla">Mozilla&nbsp;Suite</a></dd>
		<dt>Extensions</dt>
		<dd>Extensions are small add-ons that add new functionality to your Mozilla program. They can add anything from a toolbar button to a completely new feature. Browse extensions for: <a href="/extensions/?<?php echo"".uriparams()."&amp;"; ?>application=firefox">Firefox</a>, <a href="/extensions/?<?php echo"".uriparams()."&amp;"; ?>application=thunderbird">Thunderbird</a>, <a href="/extensions/?<?php echo"".uriparams()."&amp;"; ?>application=mozilla">Mozilla&nbsp;Suite</a></dd>
		<dt>Plugins</dt>
		<dd>Plugins are programs that allow websites to provide content to you and have it appear in your browser. Examples of Plugins are Flash, RealPlayer, and Java. Browse plug-ins for <a href="/plugins/">Mozilla Suite and Firefox</a></dd>
		<!--<dt>Search Engines</dt>
		<dd>In Firefox, you can add search engines that will be available in the search in the top of the browser. Browse search engines for <a href="/searchengines/">Firefox</a></dd>-->
	</dl>
    <?php
    unset($uriparams_skip);
    ?>
    <?php
    $featuredate = date("Ym");
    $sql = "SELECT TM.ID, TM.Type, TM.Name, TR.Title, TR.Body, TR.ExtendedBody, TP.PreviewURI FROM `main` TM
            INNER JOIN version TV ON TM.ID = TV.ID
            INNER JOIN applications TA ON TV.AppID = TA.AppID
            INNER JOIN os TOS ON TV.OSID = TOS.OSID
            INNER JOIN `reviews` TR ON TR.ID = TM.ID
            INNER JOIN `previews` TP ON TP.ID = TM.ID
            WHERE  `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND (`OSName` = '$OS' OR `OSName` = 'ALL') AND `approved` = 'YES' AND TR.featured = 'YES' AND TR.featuredate = '$featuredate' AND TP.preview='YES' LIMIT 1";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        while ($row = mysql_fetch_array($sql_result)) {
        $id = $row["ID"];
        $type = $row["Type"];
        if ($type=="E") {$typename = "extensions"; } else if ($type=="T") {$typename="themes"; }
        $name = $row["Name"];
        $title = $row["Title"];
        $body = nl2br($row["Body"]);
        $extendedbody = $row["ExtendedBody"];
        $previewuri = $row["PreviewURI"];
        $attr = getimagesize("$websitepath/$previewuri");
        $attr = $attr[3];

    ?>
	<h2>Currently Featuring... <?php echo"$name"; ?></a></h2>
	<a href="<?php echo"/$typename/moreinfo.php?".uriparams()."&amp;id=$id"; ?>"><img src="<?php echo"$previewuri"; ?>" <?php echo"$attr"; ?> alt="<?php echo"$name for $application"; ?>" class="imgright"></a>
    <p class="first">
    <strong><a href="<?php echo"/$typename/moreinfo.php?".uriparams()."&amp;id=$id"; ?>" style="text-decoration: none"><?php echo"$title"; ?></a></strong><br>
    <?php
    echo"$body";  
    if ($extendedbody) {
        echo" <a href=\"/$typename/moreinfo.php?".uriparams()."&amp;id=$id&amp;page=staffreview#more\">More...</a>";
    }
    ?></p>
    <?php } ?>
	</div>
	<div id="side" class="right">
	<h2>Most Popular <?php echo ucwords($application); ?> Themes</h2>
	<ol class="popularlist">

        <?php
        $i=0;
        $sql = "SELECT TM.ID, TV.vID,TM.Name, TV.Version, TM.TotalDownloads, TM.downloadcount
            FROM  `main` TM
            INNER  JOIN version TV ON TM.ID = TV.ID
            INNER  JOIN applications TA ON TV.AppID = TA.AppID
            INNER  JOIN os TOS ON TV.OSID = TOS.OSID
            WHERE  `Type`  =  'T' AND `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND (`OSName` = '$OS' OR `OSName` = 'ALL') AND `downloadcount` > '0' AND `approved` = 'YES' ORDER BY `downloadcount` DESC ";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
            if (mysql_num_rows($sql_result)=="0") {
                echo"        <li>No Popular Themes</li>\n";
            }
            while ($row = mysql_fetch_array($sql_result)) {
                $i++;
                $id = $row["ID"];
                $vid = $row["vID"];
                $name = $row["Name"];
                $version = $row["Version"];
                $downloadcount = $row["downloadcount"];
                $totaldownloads = $row["TotalDownloads"];
                $typename="themes";
                if ($lastname == $name) {
                    $i--;
                    continue;
                }

                echo"		<li>";
                echo"<a href=\"/$typename/moreinfo.php?".uriparams()."&amp;id=$id\">$name</a>";
                echo"<span class=\"downloads\"> ($downloadcount downloads)</span>";
                echo"</li>\n";

                $lastname = $name;
                if ($i >= "5") {
                    break;
                }
            }
        ?>
	</ol>
	<h2>Most Popular <?php echo ucwords($application); ?> Extensions</h2>
	<ol class="popularlist">

        <?php
        $i=0;
        $sql = "SELECT TM.ID, TV.vID,TM.Name, TV.Version, TM.TotalDownloads, TM.downloadcount
            FROM  `main` TM
            INNER  JOIN version TV ON TM.ID = TV.ID
            INNER  JOIN applications TA ON TV.AppID = TA.AppID
            INNER  JOIN os TOS ON TV.OSID = TOS.OSID
            WHERE  `Type`  =  'E' AND `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND (`OSName` = '$OS' OR `OSName` = 'ALL') AND `downloadcount` > '0' AND `approved` = 'YES' ORDER BY `downloadcount` DESC ";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
            if (mysql_num_rows($sql_result)=="0") {
                echo"        <li>No Popular Extensions</li>\n";
            }
            while ($row = mysql_fetch_array($sql_result)) {
                $i++;
                $id = $row["ID"];
                $vid = $row["vID"];
                $name = $row["Name"];
                $version = $row["Version"];
                $downloadcount = $row["downloadcount"];
                $totaldownloads = $row["TotalDownloads"];
                $typename="extensions";
                if ($lastname == $name) {
                    $i--;
                    continue;
                }

                echo"		<li>";
                echo"<a href=\"/$typename/moreinfo.php?".uriparams()."&amp;id=$id\">$name</a>";
                echo"<span class=\"downloads\"> ($downloadcount downloads)</span>";
                echo"</li>\n";

                $lastname = $name;
                if ($i >= "5") {
                    break;
                }
            }
        ?>
	</ol>
	<a href="/rss/?application=<?php echo"$application"; ?>&amp;list=newest"><img src="images/rss.png" width="16" height="16" class="rss" alt="News Additions in RSS"></a>
	<h2>New Additions</h2>
	<ol class="popularlist">

        <?php
        $i=0;
        $sql = "SELECT TM.ID, TM.Type, TV.vID, TM.Name, TV.Version, TV.DateAdded
            FROM  `main` TM
            INNER  JOIN version TV ON TM.ID = TV.ID
            INNER  JOIN applications TA ON TV.AppID = TA.AppID
            INNER  JOIN os TOS ON TV.OSID = TOS.OSID
            WHERE  `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND (`OSName` = '$OS' OR `OSName` = 'ALL') AND `approved` = 'YES' ORDER BY `DateAdded` DESC ";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
            if (mysql_num_rows($sql_result)=="0") {
                echo"        <li>Nothing Recently Added</li>\n";
            }
            while ($row = mysql_fetch_array($sql_result)) {
                $i++;
                $id = $row["ID"];
                $vid = $row["vID"];
                $type = $row["Type"];
                $name = $row["Name"];
                $version = $row["Version"];
                $dateadded = $row["DateAdded"];
                $dateadded = gmdate("M d, Y", strtotime("$dateadded")); 
                //$dateupdated = gmdate("F d, Y g:i:sa T", $timestamp);

                if ($type=="E") {
                    $typename = "extensions";
                } else if ($type=="T") {
                    $typename = "themes";
                }

                if ($lastname == $name) {
                    $i--;
                    continue;
                }

                echo"		<li>";
                echo"<a href=\"/$typename/moreinfo.php?".uriparams()."&amp;id=$id\">$name $version</a>";
                echo"<span class=\"downloads\"> ($dateadded)</span>";
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