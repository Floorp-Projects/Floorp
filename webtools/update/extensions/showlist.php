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

<?php
//----------------------------
//Global General $_GET variables
//----------------------------
//Detection Override
if ($_GET["version"]) {$app_version=$_GET["version"]; $_SESSION["app_version"]=$_GET["version"];}

if ($_GET["numpg"]) {$_SESSION["items_per_page"]=$_GET["numpg"]; }
if ($_SESSION["items_per_page"]) {$items_per_page = $_SESSION["items_per_page"];} else {$items_per_page="10";}//Default Num per Page is 10

if ($_GET["category"]) { $_SESSION["category"] = $_GET["category"]; }
if ($_SESSION["category"]) {$category = $_SESSION["category"];}
if ($category=="All") {$category="";}


if (!$_GET["pageid"]) {$pageid="1"; } else { $pageid = $_GET["pageid"]; } //Default PageID is 1
$type="E"; //Default Type is E

unset($typename);
$types = array("E"=>"Extensions","T"=>"Themes","U"=>"Updates");
$typename = $types["$type"];

//RSS Autodiscovery Link Stuff
switch ($_SESSION["category"]) {
  case "Newest":
    $rsslist = "newest";
    break;
  case "Popular":
    $rsslist = "popular";
    break;
  case "Top Rated":
    $rsslist = "rated";
    break;
}

$rssfeed = "rss/?application=" . $application . "&type=" . $type . "&list=" . $rsslist;

if (!$category) {$categoryname = "All $typename"; } else {$categoryname = $category; }
?>

    <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
    <meta http-equiv="Content-Language" content="en">
    <meta http-equiv="Content-Style-Type" content="text/css">

<TITLE>Mozilla Update :: Extensions - List - <?php echo"$categoryname"; if ($pageid) {echo" - Page $pageid"; } ?></TITLE>
<LINK REL="STYLESHEET" TYPE="text/css" HREF="/core/update.css">
<?php
if ($rsslist) {
echo"<link rel=\"alternate\" type=\"application/rss+xml\" title=\"RSS\" href=\"http://$_SERVER[HTTP_HOST]/$rssfeed\">";
}
?>
</HEAD>
<BODY>
<?php
include"$page_header";

// -----------------------------------------------
// Begin Content of the Page Here
// -----------------------------------------------

include"inc_sidebar.php";

echo"<DIV id=\"content\">\n"; // Begin Content Area

//Query for List Creation
$s = "0";
$startpoint = ($pageid-1)*$items_per_page;
if ($category=="Editors Pick" or $category=="Newest" or $category=="Popular" or $category=="Top Rated") {
if ($category =="Editors Pick") {
$editorpick="true";
} else if ($category =="Newest") {
$orderby = "TV.DateAdded DESC, `Name` ASC";
} else if ($category =="Popular") {
$orderby = "TM.TotalDownloads DESC, `Name` ASC";
} else if ($category =="Top Rated") {
$orderby = "TM.Rating DESC, `Name` ASC";
}
$catname = $category;
$category = "%";
}
if ($app_version=="0.10") {$app_version="0.95"; }
$sql = "SELECT TM.ID, TM.Name, TM.DateAdded, TM.DateUpdated, TM.Homepage, TM.Description, TM.Rating, TM.TotalDownloads, TV.vID,
SUBSTRING(MAX(CONCAT(LPAD(TV.Version, 6, '0'), TV.vID)), 7)  AS MAXvID,
MAX(TV.Version) AS Version,
TA.AppName, TOS.OSName
FROM `t_main` TM 
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
INNER  JOIN t_os TOS ON TV.OSID = TOS.OSID ";
if ($category && $category !=="%") { $sql .="INNER  JOIN t_categoryxref TCX ON TM.ID = TCX.ID
INNER JOIN t_categories TC ON TCX.CategoryID = TC.CategoryID "; }
if ($editorpick=="true") { $sql .="INNER JOIN t_reviews TR ON TM.ID = TR.ID "; }
$sql .="WHERE Type = '$type' AND AppName = '$application' AND `approved` = 'YES' ";
if ($editorpick=="true") { $sql .="AND TR.Pick = 'YES' "; }
if ($category && $category !=="%") {$sql .="AND CatName LIKE '$category' ";}
if ($app_version) { $sql .=" AND TV.MinAppVer_int <= '".strtolower($app_version)."' AND TV.MaxAppVer_int >= '".strtolower($app_version)."' ";}
if ($OS) { $sql .=" AND (TOS.OSName = '$OS' OR TOS.OSName = 'All') "; }
$sql .="GROUP BY `Name` ";
if ($orderby) {
$sql .="ORDER BY $orderby";
} else {
$sql .="ORDER  BY  `Name` , `Version` DESC ";
}

$resultsquery = $sql;
unset($sql);

//Get Total Results from Result Query & Populate Page Control Vars.
 $sql_result = mysql_query($resultsquery, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $totalresults = mysql_num_rows($sql_result);

  $num_pages = ceil($totalresults/$items_per_page); //Total # of Pages
  if ($pageid>$num_pages) {$pageid=$num_pages;} //Check PageId for Validity
  $startpoint = ($pageid-1)*$items_per_page;
if ($startpoint<0) {$startpoint=0; $startitem=0;}
  $startitem = $startpoint+1;
  $enditem = $startpoint+$items_per_page;
 if ($totalresults=="0") {$startitem = "0"; }
 if ($enditem>$totalresults) {$enditem=$totalresults;} //Verify EndItem


if ($_GET[nextnum]) {$startpoint = $_GET["nextnum"]; }
//$resultsquery = str_replace("GROUP BY `Name` ", "", $resultsquery);
$resultsquery .= " LIMIT $startpoint , $items_per_page"; //Append LIMIT clause to result query

if ($category=="%") {$category = $catname; unset($catname); }

//Now Showing Box
echo"<DIV id=\"listnav\">";
if (!$OS) {$OS="all";}
if (!$category) {$categoryname="All"; } else {$categoryname = $category;}
echo"<DIV class=\"pagenum\" "; if ($application!="mozilla") {echo" style=\"margin-right: 95px;\""; } echo">";
$previd=$pageid-1;
if ($previd >"0") {
echo"<a href=\"?pageid=$previd\">&#171; Previous</A> � ";
}
echo"Page $pageid of $num_pages";

$nextid=$pageid+1;
if ($pageid <$num_pages) {
echo" � <a href=\"?pageid=$nextid\">Next &#187;</a>";
}

echo"</DIV>\n";

echo"<SPAN class=\"listtitle\">".ucwords("$application $typename &#187; $categoryname ")."</SPAN><br>";
echo"".ucwords("$typename")." $startitem - $enditem of $totalresults";


// Modify List Form

echo"<DIV class=\"listform\">";
echo"<FORM NAME=\"listviews\" METHOD=\"GET\" ACTION=\"showlist.php\">\n";

//Items-Per-Page
echo"Show/Page: ";
$perpage = array("5","10","20","50");
echo"<SELECT name=\"numpg\">";
foreach ($perpage as $value) {
echo"<OPTION value=\"$value\"";
if ($items_per_page==$value) {echo" SELECTED"; }
echo">$value</OPTION>";
}
echo"</SELECT>\n";


// Operating Systems
echo" OS: ";
echo"<SELECT name=\"os\">\n";
$sql = "SELECT `OSName` FROM `t_os` ORDER BY `OSName`";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
  $osname = $row["OSName"];
  echo"<OPTION value=\"".strtolower($osname)."\"";
  if (strtolower($OS) == strtolower($osname)) {echo" SELECTED";}
  echo">$osname</OPTION>";
  }
echo"</SELECT>\n";


//Versions of Application
echo"Versions: ";
echo"<SELECT name=\"version\">";
if ($application != "thunderbird") {echo"<OPTION value=\"auto-detect\">Auto-Detect</OPTION>";}
$app_orig = $application; //Store original to protect against possible corruption
$sql = "SELECT `Version`, `major`, `minor`, `release`, `SubVer` FROM `t_applications` WHERE `AppName` = '$application' ORDER BY `major` DESC, `minor` DESC, `release` DESC, `SubVer` DESC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
  $version = $row["Version"];
  $subver = $row["SubVer"];
  $release = "$row[major].$row[minor]";
  if ($row["release"]) {$release = ".$release$row[release]";}
if ($app_version=="0.95") {$app_version="0.10"; }
//Firesomething Support
if ($application=="firefox" AND $row["major"]="0" AND $row["minor"]<"8") {$application="firebird";} else {$application="firefox";}

if ($subver !=="final") {$release="$release$subver";}
echo"<OPTION value=\"$release\"";
if ($app_version == $release) {echo" SELECTED"; }
echo">".ucwords($application)." $version</OPTION>";

if ($app_version=="0.10") {$app_version="0.95"; }
  }
$application = $app_orig; unset($app_orig);


echo"</SELECT>\n";
echo"<INPUT NAME=\"submit\" TYPE=\"SUBMIT\" VALUE=\"Update\">";
echo"</FORM>";
echo"</DIV>";

echo"</DIV>\n";


//---------------------------------
// Begin List
//---------------------------------
//Get Author Data and Create $authorarray and $authorids
$sql = "SELECT  TM.Name, TU.UserName, TU.UserID, TU.UserEmail FROM  `t_main`  TM 
LEFT JOIN t_authorxref TAX ON TM.ID = TAX.ID
INNER JOIN t_userprofiles TU ON TAX.UserID = TU.UserID
ORDER  BY  `Type` , `Name`  ASC "; // TM.Type = 'E'
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
     $authorarray[$row[Name]][] = $row["UserName"];
     $authorids[$row[UserName]] = $row["UserID"];
   }

//Assemble a display application version array
$sql = "SELECT `Version`, `major`, `minor`, `release`, `SubVer` FROM `t_applications` WHERE `AppName`='$application' ORDER BY `major`,`minor`";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
  $version = $row["Version"];
  $subver = $row["SubVer"];
  $release = "$row[major].$row[minor]";
  if ($row["release"]) {$release = ".$release$row[release]";}
  if ($subver !=="final") {$release="$release$subver";}

  $appvernames[$release] = $version;
  }

//Query to Generate List..
$sql = "$resultsquery";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {

    $id = $row["ID"];
    $type = $row["Type"];
    $name = $row["Name"];
    $dateadded = $row["DateAdded"];
    $dateupdated = $row["DateUpdated"];
    $homepage = $row["Homepage"];
    $description = $row["Description"];
    $rating = $row["Rating"];
    $authors = $authorarray[$name];
    $osname = $row["OSName"];
    $appname = $row["AppName"];
    $downloadcount = $row["TotalDownloads"];

//Get Version Record for Referenced MAXvID from list query
$sql2 = "SELECT TV.vID, TV.Version, TV.MinAppVer, TV.MaxAppVer, TV.Size, TV.DateAdded AS VerDateAdded, TV.DateUpdated AS VerDateUpdated, TV.URI, TV.Notes, TP.PreviewURI FROM  `t_version` TV
LEFT  JOIN t_previews TP ON TV.vID = TP.vID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
INNER  JOIN t_os TOS ON TV.OSID = TOS.OSID
WHERE TV.ID = '$id' AND TV.Version = '$row[Version]' AND TA.AppName = '$appname' AND TOS.OSName = '$osname' LIMIT 1";
 $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
$vid = $row[MAXvID];
  $row = mysql_fetch_array($sql_result2);

   $vid = $row["vID"];
if ($appvernames[$row["MinAppVer"]]) {$minappver = $appvernames[$row["MinAppVer"]]; } else { $minappver = $row["MinAppVer"]; }
if ($appvernames[$row["MaxAppVer"]]) {$maxappver = $appvernames[$row["MaxAppVer"]]; } else { $maxappver = $row["MaxAppVer"]; }
   $VerDateAdded = $row["VerDateAdded"];
   $VerDateUpdated = $row["VerDateUpdated"];
   $filesize = $row["Size"];
   $notes = $row["Notes"];
   $version = $row["Version"];
   $uri = $row["URI"];
   $previewuri = $row["PreviewURI"];

   $filename = basename($uri);

if ($VerDateAdded > $dateadded) {$dateadded = $VerDateAdded; }
if ($VerDateUpdated > $dateupdated) {$dateupdated = $VerDateUpdated; }

//Turn Authors Array into readable string...
$authorcount = count($authors);
foreach ($authors as $author) {
$userid = $authorids[$author];
$n++;
$authorstring .= "<A HREF=\"authorprofiles.php?id=$userid\">$author</A>";
if ($authorcount != $n) {$authorstring .=", "; }

}
$authors = $authorstring;
unset($authorstring, $n); // Clear used Vars.. 

//Create Customizeable Timestamp
	$day=substr($dateupdated,8,2);  //get the day
    $month=substr($dateupdated,5,2); //get the month
    $year=substr($dateupdated,0,4); //get the year
    $hour=substr($dateupdated,11,2); //get the hour
    $minute=substr($dateupdated,14,2); //get the minute
    $second=substr($dateupdated,17,2); //get the sec
    $timestamp = strtotime("$year-$month-$day $hour:$minute:$second");
    $dateupdated = gmdate("F d, Y g:i:sa", $timestamp); //gmdate("F d, Y g:i:sa T", $timestamp);


echo"<DIV class=\"item\">\n";
//echo"<DIV style=\"height: 100px\">"; //Not sure why this is here, it caused text to flood out of the box though.

if ($previewuri) {
list($width, $height, $type, $attr) = getimagesize("$websitepath"."$previewuri");

echo"<IMG SRC=\"$previewuri\" BORDER=0 HEIGHT=$height WIDTH=$width STYLE=\"float: right; padding-right: 5px\" ALT=\"$name preview\">";
}


//Upper-Right Side Box
echo"<DIV class=\"liststars\" title=\"$rating of 5 stars\" style=\"font-size: 8pt\"><A HREF=\"moreinfo.php?id=$id&page=comments\">";
for ($i = 1; $i <= floor($rating); $i++) {
echo"<IMG SRC=\"/images/stars/star_icon.png\" BORDER=0 ALT=\""; if ($i==1) {echo"$rating of 5 stars";} echo"\">";
}
if ($rating>floor($rating)) {
$val = ($rating-floor($rating))*10;
echo"<IMG SRC=\"/images/stars/star_0$val.png\" BORDER=0 ALT=\"\">";
$i++;
}
for ($i = $i; $i <= 5; $i++) {
echo"<IMG SRC=\"/images/stars/graystar_icon.png\" BORDER=0 ALT=\""; if ($i==1) {echo"$rating of 5 stars";} echo"\">";
}
echo"</A></DIV>\n";


echo"<DIV class=\"itemtitle\">";
echo"<SPAN class=\"title\"><A HREF=\"moreinfo.php?id=$id&vid=$vid\">$name $version</A></SPAN><BR>";
echo"<SPAN class=\"authorline\">By $authors</SPAN><br>";
echo"</DIV>";

//Description & Version Notes
echo"<SPAN class=\"itemdescription\">";
echo"$description<BR>";
if ($notes) {echo"$notes"; }
echo"</SPAN>\n";
echo"<BR>";
//echo"</DIV>";

//Icon Bar Modules
echo"<DIV style=\"height: 34px\">";
echo"<DIV class=\"iconbar\" style=\"width: 104px;\">";
if ($appname=="Thunderbird") {
echo"<A HREF=\"moreinfo.php?id=$id&vid=$vid\"><IMG SRC=\"/images/download.png\" BORDER=0 HEIGHT=34 WIDTH=34 STYLE=\"float:left;\" TITLE=\"More Info about $name\" ALT=\"\">More Info</A>";
} else {
echo"<A HREF=\"install.php/$filename?id=$id&vid=$vid\"><IMG SRC=\"/images/download.png\" BORDER=0 HEIGHT=34 WIDTH=34 STYLE=\"float:left;\" TITLE=\"Install $name\" ALT=\"\">Install</A>";
}
echo"<BR><SPAN class=\"filesize\">Size: $filesize kb</SPAN></DIV>";
if ($homepage) {echo"<DIV class=\"iconbar\" style=\"width: 98px;\"><A HREF=\"$homepage\"><IMG SRC=\"/images/home.png\" BORDER=0 HEIGHT=34 WIDTH=34 STYLE=\"float:left;\" TITLE=\"$name Homepage\" ALT=\"\">Homepage</A></DIV>";}
echo"<DIV class=\"iconbar\"><IMG SRC=\"/images/$appname"."_icon.png\" BORDER=0 HEIGHT=34 WIDTH=34 STYLE=\"float: left\" ALT=\"$appname\">&nbsp;Works with:<BR>&nbsp;&nbsp;$minappver - $maxappver</DIV>";
if($osname !=="ALL") { echo"<DIV class=\"iconbar\" style=\"width: 85px;\"><IMG SRC=\"/images/$osname"."_icon.png\" BORDER=0 HEIGHT=34 WIDTH=34 STYLE=\"float: left\" ALT=\"\">OS:<BR>$osname</DIV>"; }
echo"</DIV>";

echo"<DIV class=\"baseline\">Updated: $dateupdated | Total Downloads: $downloadcount<BR></DIV>\n";
echo"</DIV>\n";

} //End While Loop
if ($totalresults=="0") {
echo"<DIV class=\"item noitems\">\n";
echo"No extensions found in this category for ".ucwords($application).".\n";
echo"</DIV>\n";

}
?>




<?php
// Begin PHP Code for Dynamic Navbars
if ($pageid <=$num_pages) {
echo"<DIV id=\"listnav\">";


echo"<DIV class=\"pagenum\">";
$previd=$pageid-1;
if ($previd >"0") {
echo"<a href=\"?pageid=$previd\">&#171; Previous</A> � ";
}
echo"Page $pageid of $num_pages";


$nextid=$pageid+1;
if ($pageid <$num_pages) {
echo" � <a href=\"?pageid=$nextid\">Next &#187;</a>";
}
echo"<BR>\n";


//Skip to Page...
if ($num_pages>1) {
echo"Jump to: ";
$pagesperpage=9; //Plus 1 by default..
$i = 01;
//Dynamic Starting Point
if ($pageid>11) {
$nextpage=$pageid-10;
}
$i=$nextpage;

//Dynamic Ending Point
$maxpagesonpage=$pageid+$pagesperpage;
//Page #s
while ($i <= $maxpagesonpage && $i <= $num_pages) {

if ($i==$pageid) { 
    echo"<SPAN style=\"color: #FF0000\">$i</SPAN>&nbsp;";
  } else {
    echo"<A HREF=\"?pageid=$i\">$i</A>&nbsp;";

}

    $i++;
}
}

echo"</DIV>\n";

echo"<SPAN class=\"listtitle\">".ucwords("$application $typename &#187; $categoryname ")."</SPAN><br>";
echo"".ucwords("$typename")." $startitem - $enditem of $totalresults";

echo"</DIV>\n";
}

echo"</DIV>\n"; //End Content
?>
<?php
include"$page_footer";
?>
</BODY>
</HTML>