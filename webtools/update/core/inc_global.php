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

//inc_global.php -- Stuff that needs to be done globally to all of Mozilla Update

// ---------------------------
// quote_smart() --  Quote a variable to make it safe
// ---------------------------
function quote_smart($value)
{
   // Stripslashes if we need to
   if (get_magic_quotes_gpc()) {
       $value = stripslashes($value);
   }

   // Quote it if it's not an integer
   if (!is_int($value)) {
       $value = "'" . mysql_real_escape_string($value) . "'";
   }

   return $value;
}



//Attempt to fix Bug 246743 (strip_tags) and Bug 248242 (htmlentities)
foreach ($_GET as $key => $val) {
$_GET["$key"] = htmlentities(str_replace("\\","",strip_tags($_GET["$key"])));
}

//Set Debug Mode session Variable
if ($_GET["debug"]=="true") {$_SESSION["debug"]=$_GET["debug"]; } else if ($_GET["debug"]=="false") {unset($_SESSION["debug"]);}

// Bug 250596 Fixes for incoming $_GET variables.
if ($_GET["application"]) {
$_GET["application"] = strtolower($_GET["application"]);
$sql = "SELECT AppID FROM  `t_applications` WHERE `AppName` = ".quote_smart(ucwords(strtolower($_GET["application"])))." LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   if (mysql_num_rows($sql_result)===0) {unset($_GET["application"]);}
}

//if ($_GET["version"]) {
//$sql = "SELECT AppID FROM  `t_applications` WHERE `Release` = '$_GET[version]' LIMIT 1";
// $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
//   if (mysql_num_rows($sql_result)===0) {unset($_GET["application"]);}
//}

if ($_GET["category"] AND $_GET["category"] !=="All"
    AND $_GET["category"] !=="Editors Pick" AND $_GET["category"] !=="Popular"
    AND $_GET["category"] !=="Top Rated" AND $_GET["category"] !=="Newest") {
$sql = "SELECT CatName FROM  `t_categories` WHERE `CatName` = '".ucwords(strtolower($_GET["category"]))."' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   if (mysql_num_rows($sql_result)===0) {unset($_GET["category"]);}
}

if (!is_numeric($_GET["id"])) { unset($_GET["id"]); }
if (!is_numeric($_GET["vid"])) { unset($_GET["vid"]); }
if (!is_numeric($_GET["pageid"])) { unset($_GET["pageid"]); }
if (!is_numeric($_GET["numpg"])) { unset($_GET["numpg"]); }

// page_error() function

function page_error($reason, $custom_message) {
global $page_header, $page_footer;
echo"<TITLE>Mozilla Update :: Error</TITLE>\n";
echo"<LINK REL=\"STYLESHEET\" TYPE=\"text/css\" HREF=\"/core/update.css\">\n";
include"$page_header";

echo"<DIV class=\"contentbox\" style=\"border-color: #F00; width: 90%; margin: auto; min-height: 250px; margin-bottom: 5px\">\n";
echo"<DIV class=\"boxheader\">Mozilla Update :: Error</DIV>\n";
echo"<SPAN style=\"font-size: 12pt\">\n";
echo"Mozilla Update has encountered an error and is unable to fulfill your request. Please try your request again later. If the
problem continues, please contact the Mozilla Update staff. More information about the error may be found at the end of this
message.<BR><BR>
Error $reason: $custom_message<BR><BR>
&nbsp;&nbsp;&nbsp;<A HREF=\"javascript:history.back()\">&#171;&#171; Go Back to Previous Page</A>";
echo"</SPAN></DIV>\n";
include"$page_footer";
exit;
}
?>