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
    <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
    <meta http-equiv="Content-Language" content="en">
    <meta http-equiv="Content-Style-Type" content="text/css">

<?php
//Bookmarking-Friendly Page Title
$sql = "SELECT  Name FROM `t_main`  WHERE ID = '$_GET[id]' AND Type = 'T' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
if (mysql_num_rows($sql_result)===0) {
$return = page_error("1","Extension ID is Invalid or Missing.");
exit;

}
  $row = mysql_fetch_array($sql_result);

//Page Titles
$pagetitles = array("releases"=>"All Releases", "previews"=>"Preview Images", "comments"=>"User Comments", "staffreview"=>"Editor Review", "opinion"=>" My Opinion");
$pagetitle = $pagetitles[$_GET["page"]];
?>

<TITLE>Mozilla Update :: Themes -- More Info: <?php echo"$row[Name]"; if ($pagetitle) {echo" - $pagetitle"; } ?></TITLE>

<LINK REL="STYLESHEET" TYPE="text/css" HREF="/core/update.css">

<?php
include"$page_header";
$type = "T";
$category=$_GET["category"];
include"inc_sidebar.php";
?>

<?php
$id = $_GET["id"];


//Get Author Data
$sql2 = "SELECT  TM.Name, TU.UserName, TU.UserID, TU.UserEmail FROM  `t_main`  TM 
LEFT JOIN t_authorxref TAX ON TM.ID = TAX.ID
INNER JOIN t_userprofiles TU ON TAX.UserID = TU.UserID
WHERE TM.ID = '$_GET[id]'
ORDER  BY  `Type` , `Name`  ASC ";
 $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row2 = mysql_fetch_array($sql_result2)) {
     $authorarray[$row2[Name]][] = $row2["UserName"];
     $authorids[$row2[UserName]] = $row2["UserID"];
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


//Run Query and Create List
$s = "0";
$sql = "SELECT TM.ID, TM.Name, TM.DateAdded, TM.DateUpdated, TM.Homepage, TM.Description, TM.Rating, TM.TotalDownloads, TM.downloadcount, TV.vID, TV.Version, TV.MinAppVer, TV.MaxAppVer, TV.Size, TV.DateAdded AS VerDateAdded, TV.DateUpdated AS VerDateUpdated, TV.URI, TV.Notes, TA.AppName, TOS.OSName
FROM  `t_main` TM
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
INNER  JOIN t_os TOS ON TV.OSID = TOS.OSID
";

if ($category && $category !=="%") { $sql .="INNER  JOIN t_categoryxref TCX ON TM.ID = TCX.ID
INNER JOIN t_categories TC ON TCX.CategoryID = TC.CategoryID "; }
if ($editorpick=="true") { $sql .="INNER JOIN t_reviews TR ON TM.ID = TR.ID "; }
$sql .="WHERE TM.ID = '$_GET[id]'";

if ($_GET["vid"]) {$vid=$_GET["vid"]; $sql .=" AND TV.vID = '$vid' ";
} else {
$sql .=" AND Type = '$type' AND AppName = '$application' AND `approved` = 'YES' ";
if ($editorpick=="true") { $sql .="AND TR.Pick = 'YES' "; }
if ($category && $category !=="%") {$sql .="AND CatName LIKE '$category' ";}
if ($app_version) { $sql .=" AND TV.MinAppVer_int <= '".strtolower($app_version)."' AND TV.MaxAppVer_int >= '".strtolower($app_version)."' ";}
if ($OS) { $sql .=" AND (TOS.OSName = '$OS' OR TOS.OSName = 'All') "; }
}

$sql .= "\nORDER  BY  `Name` , `Version` DESC LIMIT 1";

//echo"<pre>$sql</pre><BR>\n";

 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);

  if (mysql_num_rows($sql_result)=="0") {
    echo"<h1>Incompatible Theme or Theme No Longer Available</h1>\n";
    echo"The theme you requested is either incompatible with the application selected, or the version of it is no longer available on Mozilla Update.<br><br>\n";
    echo"To try your request again for a different application version, use the form below.<br>\n";

    echo"<form name=\"changeapp\" method=\"get\" action=\"?\">
        <input name=\"id\" type=\"hidden\" value=\"$id\">
        <input name=\"os\" type=\"hidden\" value=\"$OS\">
        <strong>".ucwords($application)."</strong> <input name=\"application\" type=\"hidden\" value=\"$application\">";
    echo"<select name=\"version\">";
  $sql = "SELECT `Version`,`major`,`minor`,`release`,`SubVer` FROM `t_applications` WHERE `AppName`='$application' and `public_ver` ='YES' ORDER BY `major` DESC, `minor` DESC, `release` DESC, `SubVer` DESC";
  $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while ($row = mysql_fetch_array($sql_result)) {
        $release = "$row[major].$row[minor]";
        if ($row["release"]) {
            $release .= ".$row[release]";
        }
        $subver = $row["SubVer"];
        if ($subver !=="final") {
            $release .="$subver";
       }

        echo"<option value=\"$release\">$row[Version]</option>";

    
    }
    echo"</select>&nbsp;<input name=\"go\" type=\"submit\" value=\"Go\">";
    echo"</form>";

    include"$page_footer";
    echo"</body>\n</html>\n";
    exit;
  }


$v++;
    $id = $row["ID"];
    $name = $row["Name"];
    $dateadded = $row["DateAdded"];
    $dateupdated = $row["DateUpdated"];
    $homepage = $row["Homepage"];
    $description = $row["Description"];
    $rating = $row["Rating"];
    $authors = $authorarray[$name];
    $osname = $row["OSName"];

   $vid = $row["vID"];
if (!$_GET['vid']) {$_GET['vid']=$vid;}
   $appname = $row["AppName"];
if ($appvernames[$row["MinAppVer"]]) {$minappver = $appvernames[$row["MinAppVer"]]; } else { $minappver = $row["MinAppVer"]; }
if ($appvernames[$row["MaxAppVer"]]) {$maxappver = $appvernames[$row["MaxAppVer"]]; } else { $maxappver = $row["MaxAppVer"]; }
   $verdateadded = $row["VerDateAdded"];
   $verdateupdated = $row["VerDateUpdated"];
   $filesize = $row["Size"];
   $notes = $row["Notes"];
   $version = $row["Version"];
   $uri = $row["URI"];
   $downloadcount = $row["TotalDownloads"];
   $populardownloads = $row["downloadcount"];
   $filename = basename($uri);

$sql3 = "SELECT `PreviewURI`, `caption` from `t_previews` WHERE `ID` = '$id' AND `preview`='YES' LIMIT 1";
 $sql_result3 = mysql_query($sql3, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $row3 = mysql_fetch_array($sql_result3);
   $previewuri = $row3["PreviewURI"];
   $caption = $row3["caption"];

if ($VerDateAdded > $dateadded) {$dateadded = $VerDateAdded; }
if ($VerDateUpdated > $dateupdated) {$dateupdated = $VerDateUpdated; }

//Turn Authors Array into readable string...
$authorcount = count($authors);
foreach ($authors as $author) {
$userid = $authorids[$author];
$n++;
$authorstring .= "<A HREF=\"authorprofiles.php?".uriparams()."&id=$userid\">$author</A>";
if ($authorcount != $n) {$authorstring .=", "; }

}
$authors = $authorstring;
unset($authorstring, $n); // Clear used Vars.. 

if ($dateupdated > $dateadded) {
    $timestamp = $dateupdated;
    $datetitle = "Last Update: ";
  } else {
    $timestamp = $dateadded;
    $datetitle = "Added on: ";
  }

  $date = date("F d, Y g:i:sa",  strtotime("$timestamp"));


$datestring = "$datetitle $date";

//Rating
if (!$rating) { $rating="0"; }
?>


<H3> <?php ucwords("$application "); ?> Themes &#187; <?php echo"$row[Name]"; if ($pagetitle) {echo" :: $pagetitle"; } ?></H3>

<A HREF="?<?php echo"".uriparams()."&id=$id&vid=$vid"; ?>">More Info</A> | 
<A HREF="?<?php echo"".uriparams()."&id=$id&vid=$vid&page=releases"; ?>">All Releases</A> | 
<A HREF="?<?php echo"".uriparams()."&id=$id&vid=$vid&page=comments"; ?>">Comments</A> | 
<?php
$sql = "SELECT `PreviewID` from `t_previews` WHERE `ID`='$id' and `preview`='NO' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   if (mysql_num_rows($sql_result)>"0") {
?>
<A HREF="?<?php echo"".uriparams()."&id=$id&vid=$vid&page=previews"; ?>">Previews</A> | 
<?php
}

$sql = "SELECT `rID` from `t_reviews` WHERE `ID`='$id' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   if (mysql_num_rows($sql_result)>"0") {
?>
<A HREF="?<?php echo"".uriparams()."&id=$id&vid=$vid&page=staffreview"; ?>">Editor Review</A> | 
<?php } ?>
<A HREF="?<?php echo"".uriparams()."&id=$id&vid=$vid&page=opinion"; ?>">My Opinion</A>
<?php
echo"<DIV id=\"item\">\n";

if ($previewuri) {
list($width, $height, $type, $attr) = getimagesize("$websitepath"."$previewuri");
echo"<DIV style=\"padding-top: 6px; float: right; padding-right: 0px\">\n";
echo"<IMG SRC=\"$previewuri\" BORDER=0 HEIGHT=$height WIDTH=$width ALT=\"$name preview - $caption\" TITLE=\"$caption\">\n";
echo"</DIV>\n";
}
echo"<h5>";
echo"<SPAN class=\"title\"><A HREF=\"moreinfo.php?".uriparams()."&id=$id&vid=$vid\">$name $version</A></SPAN><BR>";
echo"<SPAN class=\"authorline\">By $authors</SPAN>";
echo"</h5>";

//Categories
 $sql = "SELECT `CatName` from `t_categoryxref` TCX INNER JOIN `t_categories` TC ON TCX.CategoryID=TC.CategoryID  WHERE `ID`='$id' ORDER BY `CatName` ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    $num_results = mysql_num_rows($sql_result); $i=0;
    while ($row = mysql_fetch_array($sql_result)) {
    $i++;
    $categories .= $row["CatName"];
    if ($num_results < $i ) { $categories .= ", "; }
    }

//Description & Version Notes
echo"$description";
if ($categories) { echo" (Categories: $categories)"; }
echo"<BR>\n";
if ($notes) {echo"<BR>\n$notes<BR>"; }
echo"<BR>\n";


$page = $_GET["page"];
if (!$page or $page=="general") {
?>


<DIV class="downloadbox">
<?php
//Create DateStamp for Version Release Date ($verdateadded)
	$day=substr($verdateadded,8,2);  //get the day
    $month=substr($verdateadded,5,2); //get the month
    $year=substr($verdateadded,0,4); //get the year
    $hour=substr($verdateadded,11,2); //get the hour
    $minute=substr($verdateadded,14,2); //get the minute
    $second=substr($verdateadded,17,2); //get the sec
    $timestamp = strtotime("$year-$month-$day $hour:$minute:$second");
    $verdateadded = gmdate("F d, Y", $timestamp);

//Calculate Download Time
$speed = "56"; //In Kbit/s
$speedinkb = "5.5"; //$speedinkb = $speed/8; //In KB/s 
$timeinsecs = round($filesize/$speedinkb);
$time_minutes = floor($timeinsecs/60);
$time_seconds = round($timeinsecs-($time_minutes*60),-1);
$time_seconds = $time_seconds+2; //Compensate for mirror overhead

$time = "About ";
if ($time_minutes>0) {
$time .= "$time_minutes minutes ";
}
$time .="$time_seconds seconds";


echo"
<SPAN style=\"itemdescription\">Released on $verdateadded</SPAN><BR>
<DIV class=\"moreinfoinstall\">";
if ($appname=="Thunderbird") { 
echo"<A HREF=\"install.php?id=$id&vid=$vid\" TITLE=\"Download $name $version\"><IMG SRC=\"/images/download.png\" BORDER=0 HEIGHT=34 WIDTH=34 STYLE=\"float:left;\" ALT=\"\">&nbsp;( Download Now )</A><BR>";
} else {
echo"<A HREF=\"javascript:void(InstallTrigger.installChrome(InstallTrigger.SKIN,'$uri','$name'))\" TITLE=\"Install $name $version (Right-Click to Download)\"><IMG SRC=\"/images/download.png\" BORDER=0 HEIGHT=34 WIDTH=34 STYLE=\"float:left;\" ALT=\"\">&nbsp;( Install Now )</A><BR>";
}
echo"
<SPAN class=\"filesize\">&nbsp;&nbsp;$filesize KB, ($time @ $speed"."k)</SPAN></DIV>
";



//Icon Bar Modules
echo"<DIV style=\"margin-top: 30px; height: 34px\">";
echo"<DIV class=\"iconbar\"><IMG SRC=\"/images/".strtolower($appname)."_icon.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">&nbsp;For $appname:<BR>&nbsp;&nbsp;$minappver - $maxappver</DIV>";
if($osname !=="ALL") { echo"<DIV class=\"iconbar\"><IMG SRC=\"/images/".strtolower($osname)."_icon.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">For&nbsp;$osname<BR>only</DIV>"; }
if ($homepage) {echo"<DIV class=\"iconbar\"><A HREF=\"$homepage\"><IMG SRC=\"/images/home.png\" BORDER=0 HEIGHT=34 WIDTH=34 TITLE=\"$name Homepage\" ALT=\"\">Homepage</A></DIV>";}
echo"<DIV class=\"iconbar\" title=\"$rating of 5 stars\"><A HREF=\"moreinfo.php?".uriparams()."&id=$id&page=comments\"><IMG SRC=\"/images/ratings.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">Rated<br>&nbsp;&nbsp;$rating of 5</A></DIV>";

echo"</DIV>";

echo"<BR>";

if ($application=="thunderbird") {
echo"<SPAN style=\"font-size: 10pt; color: #00F\">Theme Install Instructions for Thunderbird Users:</SPAN><BR>
<SPAN style=\"font-size: 8pt;\">(1) Click the link above to Download and save the file to your hard disk.<BR>
(2) In Mozilla Thunderbird, open the theme manager (Tools Menu/Themes)<BR>
(3) Click the Install button, and locate/select the file you downloaded and click \"OK\"</SPAN><BR>
";
}
if ($homepage) {echo"<SPAN style=\"font-size:10pt\">Having a problem with this theme? For Help and Technical Support, visit the <A HREF=\"$homepage\">Theme's Homepage</A>.</SPAN>";}

echo"<UL style=\"font-size:10pt\">";
if ($homepage) {echo"<LI> <A HREF=\"$homepage\">Theme Homepage</A>"; }
if ($appname !="Thunderbird") {echo"<LI> <a href=\"install.php/$filename?id=$id&vid=$vid\">Download Theme</A>"; }
echo"<LI> <A HREF=\"moreinfo.php?".uriparams()."&id=$id&vid=$vid&page=releases\">Other Versions</A>";
?>
</UL>
</DIV>

<?php
$sql = "SELECT `devcomments` FROM `t_main` WHERE `id`='$id' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
  if ($row["devcomments"]) {
    $devcomments = nl2br($row["devcomments"]);
    echo"<h3>Developer Comments:</h3>\n";
    echo"$devcomments";
    echo"<br>\n";
  }
?>

<H3>User Comments:</H3>
<?php
$sql = "SELECT CommentName, CommentTitle, CommentNote, CommentDate, CommentVote FROM  `t_feedback` WHERE ID = '$_GET[id]' AND CommentNote IS NOT NULL ORDER  BY `CommentDate` DESC LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $num_results = mysql_num_rows($sql_result);
  while ($row = mysql_fetch_array($sql_result)) {
    $commentname = $row["CommentName"];
    $commenttitle = $row["CommentTitle"];
    $commentnotes = $row["CommentNote"];
    $commentdate = $row["CommentDate"];
    $rating = $row["CommentVote"];

//Create Customizeable Datestamp
	$day=substr($commentdate,8,2);  //get the day
    $month=substr($commentdate,5,2); //get the month
    $year=substr($commentdate,0,4); //get the year
    $hour=substr($commentdate,11,2); //get the hour
    $minute=substr($commentdate,14,2); //get the minute
    $second=substr($commentdate,17,2); //get the sec
    $timestamp = strtotime("$year-$month-$day $hour:$minute:$second");
    $commentdate = gmdate("F d, Y g:ia", $timestamp);

echo"<h4>$commenttitle</h4>";
echo"<SPAN class=\"liststars\">";

for ($i = 1; $i <= $rating; $i++) {
echo"<IMG SRC=\"/images/stars/star_icon.png\" BORDER=0 WIDTH=16 HEIGHT=16 ALT=\"*\">";
}
for ($i = $i; $i <= 5; $i++) {
echo"<IMG SRC=\"/images/stars/graystar_icon.png\" BORDER=0 WIDTH=16 HEIGHT=16 ALT=\"\">";
}
echo"</SPAN>";

echo"&nbsp;&nbsp;By $commentname<BR>\n";
echo"&nbsp;<BR>\n";
echo"$commentnotes<BR>\n\n";
echo"&nbsp;<BR>\n";
echo"<DIV class=\"commentfooter\">\n";
echo"$commentdate | <A HREF=\"moreinfo.php?".uriparams()."&id=$id&vid=$vid&page=comments\">More Comments...</A> | <A HREF=\"moreinfo.php?".uriparams()."&id=$id&vid=$vid&page=opinion\">Rate It!</A>\n";
echo"</DIV>\n";
}

if ($num_results=="0") {
echo"<DIV class=\"nocomment\">";
echo"Nobody's Commented on this Extension Yet<BR>";
echo"Be the First! <A HREF=\"moreinfo.php?".uriparams()."&id=$id&vid=$vid&page=opinion\">Rate It!</A>";
echo"</DIV>";
}

?>

<?php

    echo"<DIV class=\"baseline\">$datestring";
    if ($populardownloads > 5 ) {
        echo" | Downloads Last 7 Days: $populardownloads";
    }
    if ($downloadcount) {
        echo" | Total Downloads: $downloadcount";
    }

    echo"</DIV>\n";
?>
<?php
} else if ($page=="releases") {

echo"<h3>All Releases</h3>";

$sql = "SELECT TV.vID, TV.Version, TV.MinAppVer, TV.MaxAppVer, TV.Size, TV.URI, TV.Notes, TA.AppName, TOS.OSName
FROM  `t_version` TV
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
INNER  JOIN t_os TOS ON TV.OSID = TOS.OSID
WHERE TV.ID = '$_GET[id]' AND `approved` = 'YES' AND TA.AppName = '$application'
ORDER  BY  `Version` DESC, `OSName` ASC 
LIMIT 0, 10";

 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
   $vid = $row["vID"];
if ($appvernames[$row["MinAppVer"]]) {$minappver = $appvernames[$row["MinAppVer"]]; } else { $minappver = $row["MinAppVer"]; }
if ($appvernames[$row["MaxAppVer"]]) {$maxappver = $appvernames[$row["MaxAppVer"]]; } else { $maxappver = $row["MaxAppVer"]; }
   $filesize = $row["Size"];
   $notes = $row["Notes"];
   $version = $row["Version"];
   $uri = $row["URI"];
   $osname = $row["OSName"];
   $appname = $row["AppName"];

echo"<DIV>"; //Open Version DIV

//Description & Version Notes
echo"<h4><A HREF=\"moreinfo.php?".uriparams()."&id=$id&vid=$vid\">Version $version</A></h4>\n";
if ($notes) {echo"$notes<br><br>\n"; }

//Icon Bar Modules
echo"<DIV style=\"height: 34px\">";
echo"<DIV class=\"iconbar\">";
echo"<A HREF=\"javascript:void(InstallTrigger.installChrome(InstallTrigger.SKIN,'$uri','$name'))\"><IMG SRC=\"/images/download.png\" BORDER=0 HEIGHT=34 WIDTH=34 TITLE=\"Install $name\" ALT=\"\">Install</A><BR><SPAN class=\"filesize\">Size: $filesize kb</SPAN></DIV>";
echo"<DIV class=\"iconbar\"><IMG SRC=\"/images/".strtolower($appname)."_icon.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">&nbsp;For $appname:<BR>&nbsp;&nbsp;$minappver - $maxappver</DIV>";
if($osname !=="ALL") { echo"<DIV class=\"iconbar\"><IMG SRC=\"/images/".strtolower($osname)."_icon.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">For&nbsp;$osname<BR>only</DIV>"; }
echo"</DIV><BR>\n";

echo"</DIV>";
}

//End General Page
} else if ($page=="comments") {
//Comments/Ratings Page

if ($_GET["numpg"]) {$items_per_page=$_GET["numpg"]; } else {$items_per_page="25";} //Default Num per Page is 25
if (!$_GET["pageid"]) {$pageid="1"; } else { $pageid = $_GET["pageid"]; } //Default PageID is 1
$startpoint = ($pageid-1)*$items_per_page;

    $sql = "SELECT CommentID FROM  `t_feedback` WHERE ID = '$id'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        $num_pages = ceil(mysql_num_rows($sql_result)/$items_per_page);


    //Comments/Ratings Page
    echo"<h3>User Comments:</h3>";

    if ($pageid <=$num_pages) {
        $previd=$pageid-1;
        if ($previd >"0") {
            echo"<a href=\"?".uriparams()."&id=$id&page=$page&pageid=$previd\">&#171; Previous</A> &bull; ";
        }
    }

    echo"Page $pageid of $num_pages";
    $nextid=$pageid+1;

    if ($pageid <$num_pages) {
        echo" &bull; <a href=\"?".uriparams()."&id=$id&page=$page&pageid=$nextid\">Next &#187;</a>";
    }
    echo"<BR>\n";


    $sql = "SELECT CommentID, CommentName, CommentTitle, CommentNote, CommentDate, CommentVote, `helpful-yes`,`helpful-no` FROM  `t_feedback` WHERE ID = '$id' ORDER  BY  `CommentDate` DESC LIMIT $startpoint, $items_per_page";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        $num_results = mysql_num_rows($sql_result);
        while ($row = mysql_fetch_array($sql_result)) {
            $commentid = $row["CommentID"];
            $name = $row["CommentName"];
            $title = $row["CommentTitle"];
            $notes = $row["CommentNote"];
            $helpful_yes = $row["helpful-yes"];
            $helpful_no = $row["helpful-no"];
            $date = date("l, F j Y", strtotime($row["CommentDate"]));
            $rating = $row["CommentVote"];
            if (!$title) {$title = "No Title"; }
            if (!$name) {$name = "Anonymous"; }

            echo"<a name=\"$commentid\"></a>\n";
            echo"<h4>$title</h4>";
            echo"&nbsp;&nbsp;Posted on $date by $name<br>";
            echo"$notes<BR>\n\n";

            echo"<DIV class=\"iconbar\" style=\"padding-left: 20px\" title=\"$rating of 5 stars\"><IMG SRC=\"/images/ratings.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">Rated<br>&nbsp;&nbsp;$rating of 5</DIV><BR><BR>";
            if ($helpful_yes>0 or $helpful_no>0) {
            $helpful_total=$helpful_yes+$helpful_no;
                echo"$helpful_yes of $helpful_total people found this comment helpful.<br>\n";
            }
            echo"Was this comment helpful to you? <a href=\"../core/commenthelpful.php?".uriparams()."&id=$id&type=$type&commentid=$commentid&pageid=$pageid&action=yes\">Yes</a>&nbsp;&nbsp;&nbsp;<a href=\"../core/commenthelpful.php?".uriparams()."&id=$id&type=$type&commentid=$commentid&pageid=$pageid&action=no\">No</a>";
            echo" <span style=\"font-size: xx-small\"><a href=\"../core/reportcomment.php?".uriparams()."&id=$id&type=$type&commentid=$commentid&pageid=$pageid&action=report\" ONCLICK=\"return confirm('Report this comment as inappropriate on the site?');\">(Report Comment)</a></span>";
            echo"<BR>";

        }

        if ($num_results=="0") {
            echo"<DIV class=\"nocomment\">";
            echo"Nobody has commented on this extension yet...<BR>
            Be the First!
            <A HREF=\"moreinfo.php?".uriparams()."&id=$id&vid=$vid&page=opinion\">Leave your comments</A>...";
            echo"</DIV>\n";
        }

        echo"<DIV style=\"height: 15px; border-bottom: 1px #CCC solid\"></DIV>";

// Begin Code for Dynamic Navbars
if ($pageid <=$num_pages) {
    $previd=$pageid-1;
    if ($previd >"0") {
        echo"<a href=\"?".uriparams()."&id=$id&page=$page&pageid=$previd\">&#171; Previous</A> &bull; ";
    }
}

echo"Page $pageid of $num_pages";
$nextid=$pageid+1;

if ($pageid <$num_pages) {
    echo" &bull; <a href=\"?".uriparams()."&id=$id&page=$page&pageid=$nextid\">Next &#187;</a>";
}
echo"<BR>\n";

//Skip to Page...
if ($num_pages>1) {
    echo"Jump to Page: ";
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
            echo"<A HREF=\"?".uriparams()."&id=$id&page=$page&pageid=$i\">$i</A>&nbsp;";
        }
        $i++;
    }
}

if ($num_pages>1) {
    echo"<br>\nComments per page: \n";
    $perpagearray = array("25","50","100");
    foreach ($perpagearray as $items_per_page) {
       echo"<A HREF=\"?".uriparams()."&id=$id&page=$page&pageid=1\">$items_per_page</A>&nbsp;";
    }
}





} else if ($page=="previews") {
// Item Previews Tab
echo"<h2>Previews for $name</h2>\n";

$sql = "SELECT `PreviewURI`,`caption` from `t_previews` WHERE `ID`='$id' and `preview`='NO' ORDER BY `PreviewID` ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
    $uri = $row["PreviewURI"];
    $caption = $row["caption"];
   echo"<h4>$caption</h4>";
    list($src_width, $src_height, $type, $attr) = getimagesize("$websitepath/$uri");

    //Scale Image Dimensions
    $dest_width="690"; // Destination Width /$tn_size_width
    $dest_height_fixed="520"; // Destination Height / $tn_size_height (Fixed)
    if ($src_width<=$dest_width AND $src_height<=$dest_width) {
    $dest_width = $src_width;
    $dest_height = $src_height;
    } else {
        $dest_height= ($src_height * $dest_width) / $src_width; // (Aspect Ratio Variable Height
        if ($dest_height>$dest_height_fixed) {
            $dest_height = $dest_height_fixed;
            $dest_width = ($src_width * $dest_height) / $src_height;
        }
    }

    echo"<img src=\"$uri\" alt=\"$caption\" width=\"$dest_width\" height=\"$dest_height\" border=0><br>\n";

  }


} else if ($page=="staffreview") {
//Staff/Editor Review Tab
echo"<h3>Editor Review:</h3>";

echo"<DIV class=\"reviewbox\">\n";
$sql = "SELECT TR.ID, `Title`, TR.DateAdded, `Body`, `Type`, `Pick`, TU.UserName FROM `t_reviews`  TR
INNER  JOIN t_main TM ON TR.ID = TM.ID
INNER  JOIN t_userprofiles TU ON TR.AuthorID = TU.UserID
WHERE `Type` = 'E' AND TR.ID = '$_GET[id]' ORDER BY `rID` DESC LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
 $num_results = mysql_num_rows($sql_result);
  while ($row = mysql_fetch_array($sql_result)) {
    $id = $row["ID"];
    $title = $row["Title"];
    $dateadded = $row["DateAdded"];
    $body = $row["Body"];
    $pick = $row["Pick"];
    $username = $row["UserName"];

//Create Customizeable Timestamp
    $timestamp = strtotime("$dateadded");
    $date = gmdate("F, Y", $timestamp);
    $posteddate = date("F j Y, g:i:sa", $timestamp);


echo"$title<br>\n";
if ($pick=="YES") {echo"<SPAN class=\"itemdescription\">&nbsp;&nbsp;&nbsp;$date Editors Pick<BR>\n"; }
echo"<BR>\n";
echo"$body</SPAN><BR>\n";
echo"<DIV class=\"commentfooter\">Posted on $posteddate by $username</DIV>\n";
}
$typename = "extension";
if ($num_results=="0") {
echo"

This $typename has not yet been reviewed.<BR><BR>

To see what other users think of this theme, view the <A HREF=\"moreinfo.php?".uriparams()."&id=$id&page=comments\">User Comments...</A>
";


}
echo"</DIV>\n";

} else if ($page=="opinion") {
//My Opinion Tab
echo"<h3>Your opinion of $name:</h3>";
?>
<?php
if ($_GET["error"]=="norating") {
echo"<DIV class=\"errorbox\">\n
Your comment submission had the following error(s), please fix these errors and try again.<br>\n
&nbsp;&nbsp;&nbsp;Rating cannot be left blank.<br>\n
&nbsp;&nbsp;&nbsp;Review/Comments cannot be left blank.<br>\n
</DIV>\n";
}
?>
<DIV class="opinionform">
<FORM NAME="opinon" METHOD="POST" ACTION="../core/postfeedback.php?<?php echo uriparams(); ?>">
<INPUT NAME="id" TYPE="HIDDEN" VALUE="<?php echo"$id"; ?>">
<INPUT NAME="vid" TYPE="HIDDEN" VALUE="<?php echo"$vid"; ?>">
    <INPUT name="type" type="hidden" value="E">
Your Name:<BR>
<INPUT NAME="name" TYPE="TEXT" SIZE=30 MAXLENGTH=30><BR>

Rating:*<BR>
<SELECT NAME="rating">
    <OPTION value="">Rating:
	<OPTION value="5">5 Stars
	<OPTION value="4">4 Stars
	<OPTION value="3">3 Stars
	<OPTION value="2">2 Stars
	<OPTION value="1">1 Star
    <OPTION value="0">0 Stars
</SELECT><BR>

Title:<BR>
<INPUT NAME="title" TYPE="TEXT" SIZE=30 MAXLENGTH=50><BR>

Review/Comments:*<BR>
<TEXTAREA NAME="comments" ROWS=5 COLS=55></TEXTAREA><BR>
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Post">&nbsp;&nbsp;<INPUT NAME="reset" TYPE="RESET" VALUE="Reset"><BR>
<SPAN class="smallfont">* Required Fields</SPAN>
</FORM>

</DIV>
<?php
} // End Pages

echo"</DIV>\n";
echo"<BR>\n";
?>
</DIV>
<?php
include"$page_footer";
?>
</BODY>
</HTML>