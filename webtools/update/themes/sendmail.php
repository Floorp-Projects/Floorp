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
//Mozilla Update Message System
//Send Mail script... 
exit;
require"../core/config.php";

if (!$_POST["senduserid"]) {
exit("<B>Error: no valid user to e-mail, possible attempt to spam detected...</B>");
}

//Get E-Mail Address from DB based on passed data..
  $sql = "SELECT `UserEmail` FROM `t_userprofiles` WHERE `UserID` = '$_POST[senduserid]' AND `UserEmailHide`='0' LIMIT 1";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
  $to_address=$row["UserEmail"]; 


//All From_, To_, and subject variables are passed from the form
// and do not need to be defined here.. unless debugging..

$from_name = $_POST["fromname"];
$from_address = $_POST["fromemail"];
$subject = $_POST["subject"];

//Anti-Abuse
$findme  = '@';
$pos = strpos($to_address, $findme);

if ($pos === false) {
//This isn't a valid e-mail address being passed... 
//Send the e-mail message to the $from_address, just for fun..
$to_address = $from_address;
}

$message = $_POST["body"];

//Message Footer (Auto-Appended to Messages sent using this form.
$message .= "\n\n";
$message .= "____________________________________\n";
$message .= "Message sent through the Mozilla Update Message system.\n
The system allows visitors to send you e-mail without revealing your e-mail address to them.
If you no longer wish to receive e-mail from visitors, you may change your preferences online at http;//update.mozilla.org.\n";


$headers .= "MIME-Version: 1.0\r\n";
$headers .= "Content-type: text/plain; charset=iso-8859-1\r\n";
$headers .= "From: ".$from_name." <".$from_address.">\r\n";
$headers .= "Reply-To: ".$from_name." <".$from_address.">\r\n";
$headers .= "X-Priority: 3\r\n";
$headers .= "X-MSMail-Priority: Normal\r\n";
$headers .= "X-Mailer: Mozilla Update Message System 1.0";

$mailstatus = mail($to_address, $subject, $message, $headers);



if ($mailstatus===FALSE) {
//Message Unsuccessful
$return_path="extensions/authorprofiles.php?id=$_POST[senduserid]&mail=unsuccessful";

} else if ($mailstatus===TRUE) {
//Message Successful
$return_path="extensions/authorprofiles.php?id=$_POST[senduserid]&mail=successful";

}
header("Location: http://$_SERVER[HTTP_HOST]/$return_path#email");
exit;

?>