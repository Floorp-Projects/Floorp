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
if (!$_POST[rating] && $_POST[rating] !=="0") {
//No Rating Defined, send user back...
$return_path="extensions/moreinfo.php?id=$_POST[id]&vid=$_POST[vid]&page=opinion&error=norating";
header("Location: http://$_SERVER[HTTP_HOST]/$return_path");
exit;
}

if (!$_POST[comments]) {
  $_POST[comments]="NULL";
  } else {
  //Comments is not null, format comments and get default name/title.. if needed.
  $_POST["comments"]="'$_POST[comments]'";
  if (!$_POST[name]) {$_POST[name]=="Anonymous"; }
  if (!$_POST[title]) {$_POST[title]=="My Comments..."; }
  }

$_POST["name"] = strip_tags($_POST["name"]);
$_POST["title"] = strip_tags($_POST["title"]);
$_POST["comments"] = strip_tags($_POST["comments"]);


$sql = "INSERT INTO `t_feedback` (`ID`, `CommentName`, `CommentVote`, `CommentTitle`, `CommentNote`, `CommentDate`) VALUES ('$_POST[id]', '$_POST[name]', '$_POST[rating]', '$_POST[title]', $_POST[comments], NOW(NULL));";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);


//Get Rating Data and Create $ratingarray
$sql = "SELECT ID, CommentVote FROM  `t_feedback` WHERE `ID` = '$_POST[id]' ORDER  BY `CommentDate` ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
     $ratingarray[$row[ID]][] = $row["CommentVote"];
   }

//Compile Rating Average
$id = $_POST[id];
if (!$ratingarray[$id]) {$ratingarray[$id] = array(); }
$numratings = count($ratingarray[$id]);
$sumratings = array_sum($ratingarray[$id]);
if ($numratings>0) {
$rating = round($sumratings/$numratings, 1);
} else {
$rating="0"; } //Default Rating


$sql = "UPDATE `t_main` SET `Rating`='$rating' WHERE `ID`='$id' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);


$return_path="extensions/moreinfo.php?id=$_POST[id]&vid=$_POST[vid]&page=comments&action=postsuccessfull";
header("Location: http://$_SERVER[HTTP_HOST]/$return_path");
exit;
?>