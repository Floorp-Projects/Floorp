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

//session_name('sid');
session_name();
session_start();

if ($_GET["version"]=="auto-detect") {$_GET["version"]=""; unset($_SESSION["app_version"]);}//Clear Versioning in Session For AutoDetect 
if ($_GET["application"] AND $_GET["application"] != $_SESSION["application"]) {
$_SESSION["application"] = $_GET["application"]; //Put the selected app into session. skip detection. ;-)
unset($_SESSION["app_version"]);
$detection_force_version = "true"; //Skip application checking, just fill in the version.
}

//Get Browser Detection Data if it's not in the Session vars already
if (!$_SESSION["application"] OR !$_SESSION["app_version"]) {
include"inc_browserdetection.php";
}

//Change OS Support for showlist.php
  switch ( $_GET["os"] )
  {
    case 'windows':
      $_GET["os"] = 'Windows';
      break;
    case 'linux':
      $_GET["os"] = 'Linux';
      break;
    case 'solaris':
      $_GET["os"] = 'Solaris';
      break;
    case 'bsd':
      $_GET["os"] = 'BSD';
      break;
    case 'macosx':
      $_GET["os"] = 'MacOSX';
      break;
    case 'all':
      $_GET["os"] = 'ALL';
      break;
    default:
      unset($_GET["os"]);
      break;
  }


if (($_GET["os"] and $_SESSION["app_os"]) AND $_GET["os"] !== $_SESSION["app_os"]) {
$_SESSION["app_os"]=$_GET["os"];

}

//Got session data for browser/app detection, populate the variables
$application = $_SESSION["application"];
$app_version = $_SESSION["app_version"];
$OS = $_SESSION["app_os"];


if ($_GET["debug"]=="killappdata") { unset($_SESSION["application"],$_SESSION["app_version"]); }
?>