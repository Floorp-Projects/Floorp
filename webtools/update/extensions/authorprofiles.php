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

<TITLE>Mozilla Update :: Extensions - Author Profile: <?php echo"$row[UserName]"; ?></TITLE>

<LINK REL="STYLESHEET" TYPE="text/css" HREF="/core/update.css">
</HEAD>
<BODY>
<?php
include"$page_header";

$type = "E";
$category = $_GET["category"];
include"inc_sidebar.php";
?>
<DIV id="content">
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
<DIV class="item">
<SPAN class="boldfont">Profile for <?php echo"$username"; ?></SPAN><BR>
<DIV class="boxheader2"></DIV>
<SPAN class="boldfont">Homepage:</SPAN> <?php 
if ($userwebsite) {echo"<A HREF=\"$userwebsite\" target=\"_blank\">$userwebsite</A>";
 } else {
echo"<SPAN CLASS=\"disabled\">Not Available for this Author</SPAN>";
}
?><BR>
<SPAN class="boldfont">E-Mail:</SPAN> <?php if ($useremailhide=="1") {
echo"<SPAN class=\"disabled\">Not Disclosed by Author</SPAN>";
} else {
echo"<SPAN class=\"emailactive\">Contact this Author via the <A HREF=\"#email\">E-Mail form</A> below</SPAN>";
} 
?>
</DIV>
&nbsp;<BR>
<DIV class="item">
<SPAN class="boldfont">All Extensions and Themes by <?php echo"$username"; ?></SPAN><BR>
<DIV class="boxheader2"></DIV>
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

echo"<DIV CLASS=\"item\">";
echo"<SPAN class=\"title itemtitle\" style=\"margin-left: 0px\"><A HREF=\"moreinfo.php?id=$id\">$name</A></SPAN><BR>";
echo"<DIV class=\"profileitemdesc\">$description</DIV>\n";
echo"<DIV class=\"baseline\">Updated: $dateupdated | Downloads: $downloadcount</DIV>\n";

echo"</DIV>\n";
echo"<BR>\n";
}
}
if ($numresults=="0") {
echo"<DIV class=\"noitems\">No Extensions or Themes in the Database for $username yet...</DIV>";
}
?>
</DIV>
&nbsp;<BR>
<?php if ($useremailhide !=="1") { ?>
<A NAME="email"></A>
<DIV class="item">
<SPAN class="boldfont">Send an E-Mail to <?php echo"$username"; ?></SPAN><BR>
<DIV class="boxheader2"></DIV>
<?php
//SendMail Returned Message Section
if ($_GET["mail"]) {
$mail = $_GET["mail"];
echo"<DIV class=\"mailresult\">";
if ($mail=="successful") {
echo"Your message has been sent successfully..."; 
} else if ($mail=="unsuccessful") {
echo"An error occured, your message was not sent... Please try again..."; 
}
echo"</DIV>\n";
}
?>
<FORM NAME="sendmail" METHOD="POST" ACTION="sendmail.php">
<INPUT NAME="senduserid" TYPE="HIDDEN" VALUE="<?php echo"$userid"; ?>">
Your Name: <INPUT NAME="fromname" TYPE="TEXT" SIZE=40 MAXLENGTH=100>&nbsp;&nbsp; Email: <INPUT NAME="fromemail" TYPE="TEXT" SIZE=40 MAXLENGTH=100><BR>
Subject: <INPUT NAME="subject" TYPE="TEXT" SIZE=40 MAXLENGTH=100><BR>
Message:<BR>
<CENTER><TEXTAREA NAME="body" ROWS=20 COLS=65></TEXTAREA><BR>
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Send Message">&nbsp;&nbsp;<INPUT NAME="reset" TYPE="RESET" VALUE="Reset Form"><BR>
</CENTER>
</FORM>
</DIV>
&nbsp;<BR>
<?php } ?>
</DIV>
<?php
include"$page_footer";
?>
</BODY>
</HTML>