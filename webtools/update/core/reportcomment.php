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

//Inappropriate Comment Reporting Tool
require"../core/config.php";


//Check and see if the CommentID/ID is valid.
$sql = "SELECT `ID`, `CommentID` 
        FROM `feedback` 
        WHERE `ID` = '".escape_string($_GET[id])."' 
          AND `CommentID`='".escape_string($_GET["commentid"])."' 
        LIMIT 1";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_ERROR);
if(mysql_num_rows($sql_result)=="0") {
  unset($_GET["id"],$_GET["commentid"],$id,$commentid);
} else {
  $id = escape_string($_GET["id"]);
  $commentid = escape_string($_GET["commentid"]);
}

//Make Sure action is as expected.
if ($_GET["action"]=="report") {
  $action="yes";
}

if (!$commentid or !$action ) {
  //No CommentID / Invalid Action --> Error.
  page_error("4","No Comment ID or Action is Invalid");
  exit;
}

//Set Flag on the Comment Record
$sql = "UPDATE `feedback` 
        SET `flag`='YES' 
        WHERE `CommentID`='$commentid' 
        LIMIT 1";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);



if ($_GET["type"]=="E") {
  $type="extensions";
} else if ($_GET["type"]=="T") {
  $type="themes";
}

$return_path="$type/moreinfo.php?id=$id&vid=$vid&".uriparams()."&page=comments&pageid=$_GET[pageid]#$commentid";
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<html lang="EN" dir="ltr">
<head>
<title>Mozilla Update :: Report a Comment</title>
</head>
 
<body>
<?php
include"$page_header";
?>
<h1>Mozilla Update :: Report a Comment Tool</h1>
You have successfully reported this comment to Mozilla Update staff.
A staff member will review your submission and take the appropriate action.<br>
Thank you for your assistance.<br><br>
To return to where you were browsing, <a href="/<?php echo"$return_path"; ?>">click this link</a>.

<?php
include"$page_footer";
?>
</body>
</html>


