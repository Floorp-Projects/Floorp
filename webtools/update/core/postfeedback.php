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

//Submit Review/Rating Feedback to Table
require"../core/config.php";


//Check and see if the ID/vID is valid.
$sql = "SELECT TM.ID, TV.vID FROM `t_main` TM INNER JOIN `t_version` TV ON TM.ID=TV.ID WHERE TM.ID = '".escape_string($_POST[id])."' AND `vID`='".escape_string($_POST["vid"])."' LIMIT 1";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_ERROR);
    if(mysql_num_rows($sql_result)=="0") {
        unset($_POST["id"],$_POST["vid"],$id,$vid);
    } else {
        $id = escape_string($_POST["id"]);
        $vid = escape_string($_POST["vid"]);
    }

    $name = escape_string(strip_tags($_POST["name"]));
    $title = escape_string(strip_tags($_POST["title"]));
    $rating = escape_string($_POST["rating"]);
    $comments = nl2br(strip_tags(escape_string($_POST["comments"])));
    $email = escape_string($_POST["email"]);
    if (!$name) {
        $name="Anonymous";
    }
    if (!$title) {
        $title="No Title";
    }

    //Make Sure Rating is as expected.
    if (is_numeric($rating) and $rating<=5 and $rating>=0) {
    } else {
        unset($rating);
    }

    if (!$rating or !$comments ) {
    //No Rating or Comment Defined, throw an error.
        page_error("3","Comment is Blank or Rating is Null.");
        exit;
    }


//Compile Info about What Version of the item this comment is about.
$sql = "SELECT TV.Version, `OSName`, `AppName` FROM `t_version` TV
        INNER JOIN `t_os` TOS ON TOS.OSID=TV.OSID
        INNER JOIN `t_applications` TA ON TA.AppID=TV.AppID
        WHERE TV.ID = '$id' AND TV.vID='$vid' LIMIT 1";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_ERROR);
    $row = mysql_fetch_array($sql_result);
    $version = $row["Version"];
    $os = $row["OSName"];
    $appname = $row["AppName"];
    $versiontagline = "version $version for $appname";
    if ($os !=="ALL") {$versiontagline .=" on $os"; }

//Are we behind a proxy and given the IP via an alternate enviroment variable? If so, use it.
    if ($_SERVER["HTTP_X_FORWARDED_FOR"]) {
        $remote_addr = $_SERVER["HTTP_X_FORWARDED_FOR"];
    } else {
        $remote_addr = $_SERVER["REMOTE_ADDR"];
    }

//Check the Formkey against the DB, and see if this has already been posted...
$formkey = escape_string($_POST["formkey"]);
$date = date("Y-m-d H:i:s", mktime(0, 0, 0, date("m"), date("d")-1, date("Y")));
$sql = "SELECT `CommentID` FROM  `t_feedback` WHERE `formkey` = '$formkey' AND `CommentDate`>='$date'";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_ERROR);
if (mysql_num_rows($sql_result)=="0") {

//FormKey doesn't exist, go ahead and add their comment.
    $sql = "INSERT INTO `t_feedback` (`ID`, `CommentName`, `CommentVote`, `CommentTitle`, `CommentNote`, `CommentDate`, `commentip`, `email`, `formkey`, `VersionTagline`) VALUES ('$id', '$name', '$rating', '$title', '$comments', NOW(NULL), '$remote_addr', '$email', '$formkey', '$versiontagline');";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);


    //Get Rating Data and Create $ratingarray
    $date = date("Y-m-d H:i:s", mktime(0, 0, 0, date("m"), date("d")-30, date("Y")));
    $sql = "SELECT ID, CommentVote FROM  `t_feedback` WHERE `ID` = '$id' AND `CommentDate`>='$date' AND `CommentVote` IS NOT NULL";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while ($row = mysql_fetch_array($sql_result)) {
        $ratingarray[$row[ID]][] = $row["CommentVote"];
    }

    //Compile Rating Average
    if (!$ratingarray[$id]) {
        $ratingarray[$id] = array();
    }
    $numratings = count($ratingarray[$id]);
    $sumratings = array_sum($ratingarray[$id]);

    if ($numratings>0) {
        $rating = round($sumratings/$numratings, 1);
    } else {
        $rating="2.5"; //Default Rating
    }


    $sql = "UPDATE `t_main` SET `Rating`='$rating' WHERE `ID`='$id' LIMIT 1";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);

}


if ($_POST["type"]=="E") {
    $type="extensions";
} else if ($_POST["type"]=="T") {
    $type="themes";
}

$return_path="$type/moreinfo.php?id=$id&vid=$vid&page=comments&action=postsuccessfull";
header("Location: http://$sitehostname/$return_path");
exit;
?>