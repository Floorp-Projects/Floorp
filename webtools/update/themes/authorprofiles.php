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
<?php
//Bookmarking-Friendly Page Title
$sql = "SELECT  UserName FROM `t_userprofiles`  WHERE UserID = '$_GET[id]' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
if (mysql_num_rows($sql_result)===0) {
$return = page_error("2","Author ID is Invalid or Missing.");
exit;

}
  $row = mysql_fetch_array($sql_result);
?>

    <TITLE>Mozilla Update :: Themes - Author Profile: <?php echo"$row[UserName]"; ?></TITLE>


<?php
include"$page_header";
?>

<div id="mBody">
    <?php
    $index="yes";
    include"inc_sidebar.php";
    ?>

	<div id="mainContent">

<?php
$userid = $_GET["id"];
 $sql = "SELECT * FROM `t_userprofiles` WHERE `UserID` = '$userid' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $row = mysql_fetch_array($sql_result);
    $userid = $row["UserID"];
    $username = $row["UserName"];
    $useremail = $row["UserEmail"];
    $userwebsite = $row["UserWebsite"];
    $userpass = $row["UserPass"];
    $userrole = $row["UserRole"];
    $useremailhide = $row["UserEmailHide"];
?>

<h2>Author Profile &#187; <?php echo"$username"; ?></h2>

Homepage: <?php 
if ($userwebsite) {echo"<A HREF=\"$userwebsite\">$userwebsite</A>";
 } else {
echo"Not Available for this Author";
}
?><BR>
E-Mail: <?php if ($useremailhide=="1") {
echo"Not Disclosed by Author";
} else {
echo"<A HREF=\"mailto:$useremail\">$useremail</A>\n";
} 
?>

&nbsp;<BR>
<h2>All Extensions and Themes by <?php echo"$username"; ?></h2>
<?php
$sql = "SELECT  TM.ID, TM.Type, TM.Name, TM.Description, TM.DateUpdated, TM.TotalDownloads, TU.UserEmail FROM  `t_main`  TM 
LEFT JOIN t_authorxref TAX ON TM.ID = TAX.ID
INNER JOIN t_userprofiles TU ON TAX.UserID = TU.UserID
WHERE TU.UserID = '$userid' AND TM.Type !='P'
ORDER  BY  `Type` , `Name` ";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $numresults = mysql_num_rows($sql_result);
  while ($row = mysql_fetch_array($sql_result)) {


$sql2 = "SELECT `vID`, `Version` FROM `t_version` WHERE `ID` = '$row[ID]' AND `approved` = 'YES' ORDER BY `Version` ASC LIMIT 1";
 $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row2 = mysql_fetch_array($sql_result2)) {
   $vid = $row2["vID"];
   $version = $row2["Version"];

$v++;
    $id = $row["ID"];
    $type = $row["Type"];
    $name = $row["Name"];
    $dateadded = $row["DateAdded"];
    $dateupdated = $row["DateUpdated"];
    $homepage = $row["Homepage"];
    $description = $row["Description"];
    $authors = $row["UserEmail"];
    $downloadcount = $row["TotalDownloads"];

	$day=substr($dateupdated,8,2);  //get the day
    $month=substr($dateupdated,5,2); //get the month
    $year=substr($dateupdated,0,4); //get the year
    $hour=substr($dateupdated,11,2); //get the hour
    $minute=substr($dateupdated,14,2); //get the minute
    $second=substr($dateupdated,17,2); //get the sec
    $timestamp = strtotime("$year-$month-$day $hour:$minute:$second");
    $dateupdated = gmdate("F d, Y g:i:sa", $timestamp); //gmdate("F d, Y", $dutimestamp);

                if ($type=="E") {
                    $typename = "extensions";
                } else if ($type=="T") {
                    $typename = "themes";
                }

echo"<h3><A HREF=\"/$typename/moreinfo.php?".uriparams()."&amp;id=$id\">$name</A></h3>";
echo"$description<br>\n";
}
}
if ($numresults=="0") {
echo"No Extensions or Themes in the Database for $username";
}
?>
</DIV>
&nbsp;<BR>

</DIV>
<?php
include"$page_footer";
?>
</BODY>
</HTML>