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

//Various Sample User_Agents, uncomment to debug detection for one. :-)
//$_SERVER["HTTP_USER_AGENT"] = "Mozilla/5.0 (Photon; U; QNX x86pc; en-US; rv:1.6a) Gecko/20030122";
//$_SERVER["HTTP_USER_AGENT"] = "Mozilla/5.0 (BeOS; U; BeOS BePC; en-US; rv:1.4a) Gecko/20030305";
//$_SERVER["HTTP_USER_AGENT"] = "Mozilla/5.0 (X11; U; SunOS i86pc; en-US; rv:1.7b) Gecko/20040302";
//$_SERVER["HTTP_USER_AGENT"] = "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.6) Gecko/20040206 Lightninglizard/0.8";
//$_SERVER["HTTP_USER_AGENT"] = "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.7b) Gecko/20040322 Nuclearunicorn/0.8.0+ (Firefox/0.8.0+ rebrand)";
//$_SERVER["HTTP_USER_AGENT"] = "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.7) Gecko/20040626 Firefox/0.9";
//$_SERVER["HTTP_USER_AGENT"] = "Mozilla/5.0 (Macintosh; U; PPC Mac OS X Mach-O; en-US; rv:1.7) Gecko/20040803 Firefox/0.9.3";
//$_SERVER["HTTP_USER_AGENT"] = "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.7.2) Gecko/20040818 Firefox/0.9.1+";

include"browser_detection.php"; //Script that defines the browser_detection() function

$OS = browser_detection('os');
$moz_array = browser_detection('moz_version');

//Turn $OS into something usable.
if ( $moz_array[0] !== '' )
{
  switch ( $OS )
  {
    case 'win':
    case 'nt':
      $OS = 'Windows';
      break;
    case 'lin':
      $OS = 'Linux';
      break;
    case 'solaris':
    case 'sunos':
      $OS = 'Solaris';
      break;
    case 'unix':
    case 'bsd':
      $OS = 'BSD';
      break;
    case 'mac':
      $OS = 'MacOSX';
      break;
    default:
      break;
  }

//Print what it's found, debug item.
//echo ( 'Your Mozilla product is ' . $moz_array[0] . ' ' . $moz_array[1] . ' running on '. $OS . ' ');

$application = $moz_array[0];
$app_version = $moz_array[1];

} else {
//If it's not a Mozilla product, then return nothing and let the default app code work..
}

//----------------------------
//Browser & OS Detection (Default Code)
//----------------------------

//if (!$_GET["application"] or ($_GET["application"]==$application && $_GET["version"]==$moz_array[1])) {
//$app_version = $moz_array[1]; //Set app_version from Detection
//}

//Application
if (!$application) { $application="firefox"; } //Default App is Firefox

//App_Version
//if ($_GET["version"]) {$app_version = $_GET["version"]; }

if ($detection_force_version=="true") {$application=$_SESSION["application"];}
//Get Max Version for App Specified
$sql = "SELECT `major`,`minor`,`release`,`SubVer` FROM `t_applications` WHERE `AppName` = '$application' ORDER BY `major` DESC, `minor` DESC, `release` DESC, `SubVer` DESC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
  $release = "$row[major].$row[minor]";
  if ($row["release"]) {$release = ".$release$row[release]";}
    $subver = $row["SubVer"];
    if ($subver !=="final") {$release="$release$subver";}

    if (!$app_version OR $detection_force_version=="true") { $app_version = $release; }
    unset($release, $subver);

//OS
if ($_GET["os"]) {$OS = $_GET["os"];} //If Defined, set.

// 1.0PR support
if ($app_version=="1.0PR") {$app_version="0.10"; }

// 0.9.x branch support -- All non-branch (+) builds should be 0.9.
if (strpos($app_version, "+")=== false AND strpos($app_version, "0.9")===0) { $app_version="0.9";}



//Register Browser Detection Values with the session
if (!$_SESSION["application"]) { $_SESSION["application"] = $application; }
if (!$_SESSION["app_version"]) { $_SESSION["app_version"] = $app_version; }
if (!$_SESSION["app_os"]) { $_SESSION["app_os"] = $OS; }
?>