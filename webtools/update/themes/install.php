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
//Get Full Information for the file requested.
$sql = "SELECT `URI` FROM `t_version` WHERE `ID`='$_GET[id]' AND `vID`='$_GET[vid]' LIMIT 1";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
 $row = mysql_fetch_array($sql_result);
$uri=$row["URI"]; 

// New DownloadCount management Code..
//Check for user, to see if they recently accessed this file (filters duplicate/triplicate+ requests in a short period).
  $maxlife = date("YmdHis", mktime(date("H"), date("i")-10, date("s"), date("m"), date("d"), date("Y")));
  $sql = "SELECT `dID` FROM `t_downloads` WHERE `ID`='$_GET[id]' AND `vID`='$_GET[vid]' AND `user_ip`='$_SERVER[REMOTE_ADDR]' AND `user_agent` = '$_SERVER[HTTP_USER_AGENT]' AND `date`>'$maxlife' AND `type`='download' LIMIT 1";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    if (mysql_num_rows($sql_result)=="0") {
     //Insert a record of this download for the next 10 minutes anyway. :-)
     $today=date("YmdHis");
      $sql = "INSERT INTO `t_downloads` (`ID`,`date`,`vID`, `user_ip`, `user_agent`, `type`) VALUES ('$_GET[id]','$today','$_GET[vid]', '$_SERVER[REMOTE_ADDR]', '$_SERVER[HTTP_USER_AGENT]', 'download');";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);

    //Cleanup the Individual Downloads part of the table for old records
    $sql = "DELETE FROM `t_downloads` WHERE `date`<'$maxlife' AND `type`='download'";
      $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);

$today=date("Ymd")."000000";
  //Per day download tracking -- Record hits for this day in the record (if it doesn't exist create it)
  $sql = "SELECT `dID` FROM `t_downloads` WHERE `ID`='$_GET[id]' AND `date`='$today' AND `type`='count' LIMIT 1";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    if (mysql_num_rows($sql_result)=="0") {
      $sql = "INSERT INTO `t_downloads` (`ID`,`date`,`downloadcount`,`type`) VALUES ('$_GET[id]','$today','1','count');";
    } else {
        $row = mysql_fetch_array($sql_result);
      $sql = "UPDATE `t_downloads` SET `downloadcount`=downloadcount+1 WHERE `dID`='$row[dID]' LIMIT 1";
    }
      $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);


//Download Statistic Management Code
// Maintain the last 7 days record count
// This is also where the weekly w/e code would go. if that feature is ever created.
   $mindate = date("Ymd", mktime(0, 0, 0, date("m"), date("d")-7, date("Y")))."000000";
   $downloadcount="0";
  $sql = "SELECT `downloadcount` FROM `t_downloads` WHERE `ID`='$_GET[id]' AND `date`>='$mindate' AND `type`='count' LIMIT 1";
   $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while ($row = mysql_fetch_array($sql_result)) {
     $downloadcount = $downloadcount+$row["downloadcount"];
    }
  //Update the 7 day count in the main record.
  $sql = "UPDATE `t_main` SET `downloadcount`='$downloadcount' WHERE `ID`='$_GET[id]' LIMIT 1";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);

  //Clean up the Counts per day for >8 days. (and in the future, compile the week/ending records for this data)
    $sql = "DELETE FROM `t_downloads` WHERE `ID`='$_GET[id]' AND `date`<'$mindate' AND `type`='count'";
      $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   
    }

//Send User on their way to the file...
header("Location: $uri")
?>