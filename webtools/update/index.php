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
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<html lang="EN" dir="ltr">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
    <meta http-equiv="Content-Language" content="en">
    <meta http-equiv="Content-Style-Type" content="text/css">

<TITLE>Mozilla Update</TITLE>
<LINK REL="STYLESHEET" TYPE="text/css" HREF="/core/update.css">
</HEAD>
<BODY>
<?php
include"$page_header";
?>

<DIV class="contentbox" style="margin-left: 2px;">
<DIV class="boxheader">Welcome to Mozilla Update</DIV>
<SPAN class="itemdescription">
Mozilla Update hosts Extensions and Themes for Mozilla software. On this site you can find Extensions and Themes for Mozilla Firefox,
Mozilla Thunderbird and the Mozilla 1.x suite, with more to come. The site is broken up into sections for each product, with the
extensions and themes categorized to be easy to find. They're also sorted by what version of the product you're using, so you can
browse only for Firefox 0.9 compatible extensions, for example. For more information about Mozilla Update, please read our <A HREF="/faq/">Frequently Asked Questions...</A>

</SPAN>
</DIV>
<?php include"inc_featuredupdate.php"; ?>

<?php
//include"inc_softwareupdate.php";
if ($_GET["application"]) {$application=$_GET["application"]; }
?>
<?php
//Temporary!! Current Version Array Code
$currentver_array = array("firefox"=>"0.95", "thunderbird"=>"0.8", "mozilla"=>"1.7");
$currentver_display_array = array("firefox"=>"1.0 Preview Release", "thunderbird"=>"0.8", "mozilla"=>"1.7.x");
$currentver = $currentver_array[$application];
$currentver_display = $currentver_display_array[$application];
?>


<DIV class="frontpagecontainer">
<DIV class="contentbox contentcolumns">
<DIV class="boxheader"><?php print(ucwords($application)); echo" $currentver_display"; ?> Extensions</DIV>

<?php
$sql = "SELECT TM.ID
FROM  `t_main` TM
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
WHERE  `Type`  =  'E' AND `AppName` = '$application' AND `minAppVer_int`<='$currentver' AND `maxAppVer_int` >='$currentver' AND `approved` = 'YES' GROUP BY TM.ID";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $numextensions = mysql_num_rows($sql_result);
?>
     <a href="/extensions/">Browse extensions</a> (<?php echo"$numextensions"; ?> available)<BR> 
<BR>
<?php
$sql = "SELECT TR.ID, `Title`, TR.DateAdded, `Body`, `Type`, `pick` FROM `t_reviews`  TR
INNER JOIN t_main TM ON TR.ID = TM.ID
INNER JOIN t_version TV ON TV.ID = TM.ID
INNER JOIN t_applications TA ON TA.AppID = TV.AppID
WHERE `Type` = 'E' AND `AppName` = '$application' AND `pick`='YES' ORDER BY `rID` DESC LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
    $id = $row["ID"];
    $title = $row["Title"];
    $pick = $row["pick"];
    $dateadded = $row["DateAdded"];
    $body = $row["Body"];
    $bodylength = strlen($body);
if ($bodylength>"250") {
 $body = substr($body,0,250);
 $body .= " <a href=\"/extensions/moreinfo.php?id=$id&page=staffreview\">[More...]</a>";
 
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


echo"$title<br>&nbsp;&nbsp;&nbsp;$date";
if ($pick=="YES") {echo" Editors Pick";}
echo"<BR><BR>\n";
echo"<SPAN class=\"itemdescription\">$body</SPAN><BR>\n";
}
?>
<BR>
</DIV>

<DIV class="contentbox contentcolumns">
<DIV class="boxheader"><?php print(ucwords($application)); echo" $currentver_display"; ?> Themes</DIV>
<?php
$sql = "SELECT TM.ID FROM  `t_main` TM
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
WHERE  `Type`  =  'T' AND `AppName` = '$application' AND `minAppVer_int` <='$currentver' AND `maxAppVer_int` >= '$currentver' AND `approved` = 'YES' GROUP BY TM.ID";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $numthemes = mysql_num_rows($sql_result);
?>
     <a href="/themes/">Browse themes</a> (<?php echo"$numthemes"; ?> available)<BR>
<BR>
<?php
$sql = "SELECT TR.ID, `Title`, TR.DateAdded, `Body`, `Type`, `pick` FROM `t_reviews`  TR
INNER JOIN t_main TM ON TR.ID = TM.ID
INNER JOIN t_version TV ON TV.ID = TM.ID
INNER JOIN t_applications TA ON TA.AppID = TV.AppID
WHERE `Type` = 'T' AND `AppName` = '$application' AND `pick`='YES' ORDER BY `rID` DESC LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
    $id = $row["ID"];
    $title = $row["Title"];
    $pick = $row["pick"];
    $dateadded = $row["DateAdded"];
    $body = $row["Body"];
    $bodylength = strlen($body);
if ($bodylength>"250") {
 $body = substr($body,0,250);
 $body .= " <a href=\"/moreinfo.php?id=$id&page=staffreview\">[More...]</a>";
 
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

echo"$title - $date";
if ($pick=="YES") {echo" Editors Pick<BR><BR>\n";}
echo"$body<BR>\n";
}
?>
</DIV>
</DIV>
<?php
include"$page_footer";
?>
</BODY>
</HTML>