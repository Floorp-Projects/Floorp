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
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<html lang="EN" dir="ltr">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
    <meta http-equiv="Content-Language" content="en">
    <meta http-equiv="Content-Style-Type" content="text/css">

<TITLE>Mozilla Update :: Themes - Change the Look of Mozilla Software</TITLE>

<?php
include"$page_header";
?>

<div id="mBody">
<?php
$type = "T";
if ($_GET["application"]) {$application=$_GET["application"]; }

$index="yes";
include"inc_sidebar.php";
?>

<div class="key-point">
<h2 style="margin: 0; font-size: 2px;"><img src="/images/t_featuring.gif" alt="Featuring: Firefox!"></h2>
<?php
$sql = "SELECT TR.ID, TM.Name, `Title`, TR.DateAdded, `Body`, `Type`, `pick` FROM `t_reviews`  TR
INNER JOIN t_main TM ON TR.ID = TM.ID
INNER JOIN t_version TV ON TV.ID = TM.ID
INNER JOIN t_applications TA ON TA.AppID = TV.AppID
WHERE `Type` = '$type' AND `AppName` = '$application' AND `featured`='YES' ORDER BY `rID` DESC LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
    $id = $row["ID"];
    $name = $row["Name"];
    $title = $row["Title"];
    $dateadded = $row["DateAdded"];
    $pick = $row["pick"];
    $body = $row["Body"];
    $bodylength = strlen($body);
if ($bodylength>"250") {
 $body = substr($body,0,250);
 $body .= " <a href=\"moreinfo.php?id=$id&page=staffreview\">[More...]</a>";
 
 }

//Create Customizeable Timestamp
	$day=substr($dateadded,8,2);  //get the day
    $month=substr($dateadded,5,2); //get the month
    $year=substr($dateadded,0,4); //get the year
    $hour=substr($dateadded,11,2); //get the hour
    $minute=substr($dateadded,14,2); //get the minute
    $second=substr($dateadded,17,2); //get the sec
    $timestamp = strtotime("$year-$month-$day $hour:$minute:$second");
    $date = gmdate("F, Y", $timestamp);

echo"<a href=\"moreinfo.php?id=$id\">$name</A> -- $title";
if ($pick=="YES") {echo" - $date Editors Pick"; }
echo"<BR>\n";
echo"<SPAN class=\"itemdescription\">$body</SPAN><BR>\n";
}
?>
</DIV>

<div id="mBody">
<h3>What is a Theme?</h3>
<p>Themes are skins for <?php print(ucwords($application)); ?>, they allow you to change the look and
feel of the browser and personalize it to your tastes. A theme can simply change the colors of <?php print(ucwords($application)); ?> 
or it can change every piece of the browser appearance.</p>


<?php
//Temporary!! Current Version Array Code
$currentver_array = array("firefox"=>"0.95", "thunderbird"=>"0.8", "mozilla"=>"1.7");
$currentver_display_array = array("firefox"=>"1.0 Preview Release", "thunderbird"=>"0.8", "mozilla"=>"1.7.x");
$currentver = $currentver_array[$application];
$currentver_display = $currentver_display_array[$application];
?>

  <!-- Start News Columns -->
  <div class="frontcolumn">
<a href="http://www.mozilla.org/news.rdf"><img src="../images/rss.png" width="28" height="16" class="rss" alt="Mozilla News in RSS"></a><h2 style="margin-top: 0;"><a href="showlist.php?category=Newest" title="New Extensions on Mozilla Update">New Additions</a></h2>
<span class="newsSubline">New and Updated Themes</span>
<ul class="news">

<?php
$i=0;
//MacOSX Specific override for All+Mac themes. Bug 252294
if ($_SESSION["app_os"]=="MacOSX") { $app_os = $_SESSION["app_os"]; } else { $app_os = "ALL"; }

$sql = "SELECT TM.ID, TV.vID, TM.Name, TV.Version, TV.DateAdded
FROM  `t_main` TM
INNER JOIN t_version TV ON TM.ID = TV.ID
INNER JOIN t_applications TA ON TV.AppID = TA.AppID
INNER JOIN t_os TOS ON TV.OSID = TOS.OSID
WHERE `Type`  =  '$type' AND `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND (`OSName` = '$_SESSION[app_os]' OR `OSName` = '$app_os') AND `approved` = 'YES' ORDER BY `DateAdded` DESC ";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
   $i++;
   $id = $row["ID"];
   $vid = $row["vID"];
   $name = $row["Name"];
   $version = $row["Version"];
   $dateadded = $row["DateAdded"];
//Create Customizeable Datestamp
    $timestamp = strtotime("$dateadded");
    $dateadded = gmdate("M d", $timestamp); //    $dateupdated = gmdate("F d, Y g:i:sa T", $timestamp);

if ($lastname == $name) {$i--; continue; }
  echo"<li>\n";
  echo"<div class=\"date\">$dateadded</div>\n";
  echo"<a href=\"moreinfo.php?id=$id&vid=$vid\">$name $version</a><BR>\n";
  echo"</li>\n";

$lastname = $name;
if ($i >= "5") {break;}
}
?>
</ul>
</div>
<div class="frontcolumn">
<a href="http://planet.mozilla.org/rss10.xml"><img src="../images/rss.png" width="28" height="16" class="rss" alt="Mozilla Weblogs in RSS"></a><h2 style="margin-top: 0;"><a href="showlist.php?category=Popular" title="Most Popular Extensions, based on Downloads over the last week">Most Popular</a></h2>
<span class="newsSubline">Downloads over the last week</span>
<ul class="news">

<?php
$i=0;
$sql = "SELECT TM.ID, TV.vID,TM.Name, TV.Version, TM.TotalDownloads, TM.downloadcount
FROM  `t_main` TM
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
WHERE  `Type`  =  '$type' AND `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND `DownloadCount` > '0' AND `approved` = 'YES' ORDER BY `DownloadCount` DESC ";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
   $i++;
   $id = $row["ID"];
   $vid = $row["vID"];
   $name = $row["Name"];
   $version = $row["Version"];
   $downloadcount = $row["downloadcount"];
   $totaldownloads = $row["TotalDownloads"];
if ($lastname == $name) {$i--; continue; }
  echo"<li>\n";
  echo"<div class=\"date\">$i</div>\n";
  echo"<a href=\"moreinfo.php?id=$id\">$name</a><br>\n";
  echo"<span class=\"newsSubline\">($downloadcount downloads)</span>\n";
  echo"</li>\n";
$lastname = $name;
if ($i >= "5") {break;}
}
?>
</ul>
</div>

<div class="frontcolumn">
<a href="http://www.mozillazine.org/atom.xml"><img src="../images/rss.png" width="28" height="16" class="rss" alt="MozillaZine News in RSS"></a><h2 style="margin-top: 0;"><a href="showlist.php?category=Top Rated" title="Highest Rated Extensions by the Community">Top Rated</a></h2>
<span class="newsSubline">Based on feedback from visitors</span>
<ul class="news">

<?php
$r=0;
$usednames = array();
$sql = "SELECT TM.ID, TV.vID, TM.Name, TV.Version, TM.Rating, TU.UserName
FROM  `t_main` TM
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
INNER JOIN t_authorxref TAX ON TAX.ID = TM.ID
INNER JOIN t_userprofiles TU ON TU.UserID = TAX.UserID
WHERE  `Type`  =  '$type' AND `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND `Rating` > '0' AND `approved` = 'YES' ORDER BY `Rating` DESC, `Name` ASC, `Version` DESC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
   $r++; $s++;
   $id = $row["ID"];
   $vid = $row["vID"];
   $name = $row["Name"];
   $version = $row["Version"];
   $rating = $row["Rating"];
  $arraysearch = array_search("$name", $usednames);
  if ($arraysearch !== false AND $usedversions[$arraysearch]['version']<$version) {$r--; continue; }

  echo"<li>\n";
  echo"<div class=\"date\">$rating stars</div>\n";
  echo"<a href=\"moreinfo.php?id=$id\">$name</a>\n";
  echo"</li>\n";


// Code for the OLD Graphical Star System. 
//$rating = round($rating);
//echo"<SPAN title=\"Rated: $rating of 5\" style=\"font-size: 8pt\">";
//for ($i = 1; $i <= floor($rating); $i++) {
//echo"<IMG SRC=\"/images/stars/star_icon.png\" BORDER=0 ALT=\""; if ($i==1) {echo"$rating of 5 stars";} echo"\">";
//}
//if ($rating>floor($rating)) {
//$val = ($rating-floor($rating))*10;
//echo"<IMG SRC=\"/images/stars/star_0$val.png\" BORDER=0 ALT=\"\">";
//$i++;
//}
//for ($i = $i; $i <= 5; $i++) {
//echo"<IMG SRC=\"/images/stars/graystar_icon.png\" BORDER=0 ALT=\""; if ($i==1) {echo"$rating of 5 stars";} echo"\">";
//}
//echo"</SPAN><br>\n";
$usednames[$s] = $name;
$usedversions[$s] = $version;
if ($r >= "5") {break;}
}
unset($usednames, $usedversions, $r, $s, $i);
?>
</ul>
</div>

  <!-- End News Columns -->  
<br style="clear: both;">


</div>
</div>
<BR>
<?php
include"$page_footer";
?>
</BODY>
</HTML>