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
//   Alan Starr <alanjstarr@yahoo.com>
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

$app = strtolower($_GET["application"]);  // Firefox, Thunderbird, Mozilla
$type = $_GET["type"]; //E, T, [P]
$list = ucwords(strtolower($_GET["list"])); // Newest, Updated, [Editors], Popular

$sitetitle = "Mozilla Update";
$siteicon = "http://www.mozilla.org/images/mozilla-16.png";
$siteurl = "http://update.mozilla.org";
$sitedescription = "the way to keep your mozilla software up-to-date";
$sitelanguage = "en-us";
$sitecopyright = "Creative Commons?";
$currenttime = gmdate(r);// GMT 
$rssttl = "120"; //Life of feed in minutes

//header("Content-Type: application/octet-stream");
header("Content-Type: text/xml");

// Firefox, extensions, by date added

$select = "SELECT DISTINCT 
t_main.ID, 
t_main.Name AS Title, 
t_main.Description,  
t_version.Version, 
t_version.vID,
t_version.DateUpdated AS DateStamp,
t_applications.AppName";

$from = "FROM  t_main 
INNER  JOIN t_version ON t_main.ID = t_version.ID
INNER  JOIN t_applications ON t_version.AppID = t_applications.AppID";

$where = "`approved` = 'YES'"; // Always have a WHERE

if ($app == 'firefox' || $app == 'thunderbird' || $app == 'mozilla') {
  $where .= " AND t_applications.AppName = '$app'";
}

if ($type == 'E' || $type == 'T' || $type == 'P') {
  $where .= " AND t_main.Type = '$type'";
}

switch ($list) {
   case "Popular":
     $orderby = "t_version.DownloadCount DESC";
     break;
   case "Updated":
     $orderby = "t_main.DateUpdated DESC";
     break;
   case "Rated":
     $orderby = "t_main.Rating DESC";
     break;
   case "Newest":
   default:
     $orderby = "t_main.DateAdded DESC";
     break;
}

$sql = $select . " " . $from . " WHERE " . $where . " ORDER BY " . $orderby . " LIMIT 0, 10";

//echo $sql;

include"inc_rssfeed.php";


?>