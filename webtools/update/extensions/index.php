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

<TITLE>Mozilla Update :: Extensions - Add Features to Mozilla Software</TITLE>


<LINK REL="STYLESHEET" TYPE="text/css" HREF="/core/update.css">
</HEAD>
<BODY>
<?php
$type = "E";
if ($_GET["application"]) {$application=$_GET["application"]; }
include"$page_header";
$index="yes";
include"inc_sidebar.php";
?>
<DIV class="box">
<DIV class="boxheader" style="width: 100%">What is an Extension?</DIV>
<SPAN class="itemdescription">Extensions are small add-ons that add new functionality to <?php print(ucwords($application)); ?>.
They can add anything from a toolbar button to a completely new feature. They allow the browser to be customized to fit the
personal needs of each user if they need addtional features<?php if ($application !=="mozilla") { ?>, while keeping <?php print(ucwords($application)); ?> small
to download <?php } ?>.</SPAN>
</DIV>

<DIV class="box">
<DIV class="boxheader" style="width: 100%">Featured <?php print(ucwords($application)); ?> Extension</DIV>
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
<DIV class="box" style="width: 80%; min-height: 200px; border: 0px">
<DIV class="boxcolumns">
<DIV class="boxheader"><A HREF="showlist.php?category=Popular">Most Popular</A>:</DIV>
<?php
$i=0;
$sql = "SELECT TM.ID, TV.vID,TM.Name, TV.Version, TM.TotalDownloads, TM.downloadcount
FROM  `t_main` TM
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
WHERE  `Type`  =  '$type' AND `AppName` = '$application' AND `downloadcount` > '0' AND `approved` = 'YES' ORDER BY `downloadcount` DESC ";
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
  echo"$i - <a href=\"moreinfo.php?id=$id\">$name</a><br>\n";
  echo"<SPAN class=\"smallfont nocomment\">($downloadcount downloads)</SPAN><BR>\n";

$lastname = $name;
if ($i >= "5") {break;}
}
?>
<BR>
</DIV>
<DIV class="boxcolumns">
<DIV class="boxheader"><A HREF="showlist.php?category=Top Rated">Top Rated</A>:</DIV>
<?php
$r=0;
$usednames = array();
$sql = "SELECT TM.ID, TV.vID, TM.Name, TV.Version, TM.Rating, TU.UserName
FROM  `t_main` TM
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
INNER JOIN t_authorxref TAX ON TAX.ID = TM.ID
INNER JOIN t_userprofiles TU ON TU.UserID = TAX.UserID
WHERE  `Type`  =  '$type' AND `AppName` = '$application' AND `Rating` > '0' AND `approved` = 'YES' ORDER BY `Rating` DESC, `Name` ASC, `Version` DESC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
   $r++; $s++;
   $id = $row["ID"];
   $vid = $row["vID"];
   $name = $row["Name"];
   $version = $row["Version"];
   $rating = $row["Rating"];
  $arraysearch = array_search("$name", $usednames);
  if ($arraysearch !== false AND $usedversions[$arraysearch]['version']<$version) {$r--; continue; } //
  echo"$r - <a href=\"moreinfo.php?id=$id\">$name</a>&nbsp;";

  //$rating = round($rating);
echo"<SPAN title=\"Rated: $rating of 5\" style=\"font-size: 8pt\">";
for ($i = 1; $i <= floor($rating); $i++) {
echo"<IMG SRC=\"/images/stars/star_icon.png\" BORDER=0 ALT=\""; if ($i==1) {echo"$rating of 5 stars";} echo"\">";
}
if ($rating>floor($rating)) {
$val = ($rating-floor($rating))*10;
echo"<IMG SRC=\"/images/stars/star_0$val.png\" BORDER=0 ALT=\"\">";
$i++;
}
for ($i = $i; $i <= 5; $i++) {
echo"<IMG SRC=\"/images/stars/graystar_icon.png\" BORDER=0 ALT=\""; if ($i==1) {echo"$rating of 5 stars";} echo"\">";
}
echo"</SPAN><br>\n";
//echo"<SPAN class=\"smallfont nocomment\">By $author</SPAN><BR>\n";
$usednames[$s] = $name;
$usedversions[$s] = $version;
if ($r >= "5") {break;}
}
unset($usednames, $usedversions, $r, $s, $i);
?>
</DIV>
<DIV class="boxcolumns">
<DIV class="boxheader"><A HREF="showlist.php?category=Newest">Most Recent</A>:</DIV>
<?php
$i=0;
$sql = "SELECT TM.ID, TV.vID, TM.Name, TV.Version, TV.DateAdded
FROM  `t_main` TM
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
INNER  JOIN t_os TOS ON TV.OSID = TOS.OSID
WHERE  `Type`  =  '$type' AND `AppName` = '$application' AND (`OSName` = '$_SESSION[app_os]' OR `OSName` = 'ALL') AND `approved` = 'YES' ORDER BY `DateAdded` DESC ";
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
    $dateadded = gmdate("F d, Y g:i:sa", $timestamp); //    $dateupdated = gmdate("F d, Y g:i:sa T", $timestamp);

if ($lastname == $name) {$i--; continue; }
  echo"$i - <a href=\"moreinfo.php?id=$id&vid=$vid\">$name $version</a><BR>\n";
  echo"<SPAN class=\"smallfont nocomment\">($dateadded)</SPAN><BR>\n";

$lastname = $name;
if ($i >= "5") {break;}
}
?>
</DIV>
</DIV>
<BR>
<?php
include"$page_footer";
?>
</BODY>
</HTML>