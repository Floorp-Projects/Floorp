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
require"core/config.php";
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">

<head>
 <title>Mozilla Update</title>

<?php
include"$page_header";
?>

<div class="key-point cnet" id="firefox-feature">
  <script type="text/javascript">
  <!--
    var classes = new Array("cnet");
    var date = new Date();
    var seconds;
    var classid;
    seconds = date.getSeconds();
    classid = seconds % classes.length;
    document.getElementById('firefox-feature').className = 'key-point front-feature-' + classes[classid];
  -->
  </script>
  <a href="/products/firefox" title="Learn more about Firefox" id="featurelink">Learn more about Firefox</a>
  <div id="feature-content">
    <h2 style="margin: 0; font-size: 2px;"><img src="/images/t_firefox.gif" alt="Featuring: Firefox!"></h2>
    <p>Firefox 0.9 is the <a href="shelf.html">award winning</a> preview of Mozilla's next generation browser. Download Firefox entirely free or <a href="">purchase it on a CD</a> from the Mozilla store. <a href="#dfg">Learn more about Firefox...</a></p>
    <script type="text/javascript" src="products/firefox/download.js"></script>
    <script type="text/javascript">
    <!--
    writeDownloadsFrontpage();
    //-->
    </script>
    <noscript>
      <div class="download">
        <h3>Download Now</h3>
        <ul>
          <li><a href="http://ftp.mozilla.org/pub/mozilla.org/firefox/releases/0.9.2/FirefoxSetup-0.9.2.exe">Windows (4.7MB)</a> </li>
          <li><a href="http://ftp.mozilla.org/pub/mozilla.org/firefox/releases/0.9.1/firefox-0.9.1-i686-linux-gtk2+xft-installer.tar.gz">Linux (8.1MB)</a></li>
          <li><a href="http://ftp.mozilla.org/pub/mozilla.org/firefox/releases/0.9.1/firefox-0.9.1-mac.dmg.gz">Mac OS X (8.6MB)</a></li>
        </ul>
      </div>
    </noscript>
  </div>
</div>
<?php
    if ($_GET["application"]) {$application=$_GET["application"]; }

    //Temporary!! Current Version Array Code
        $currentver_array = array("firefox"=>"0.95", "thunderbird"=>"0.8", "mozilla"=>"1.7");
        $currentver_display_array = array("firefox"=>"1.0 Preview Release", "thunderbird"=>"0.8", "mozilla"=>"1.7.x");
        $currentver = $currentver_array[$application];
        $currentver_display = $currentver_display_array[$application];
?>

<div id="mBody">
  <div class="frontcolumn">
    <h2><a href="extensions/">Get Extensions</a></h2>
    <a href="products/thunderbird"><img src="images/product-front-thunderbird.png" alt="Thunderbird" class="promo" width="60" height="60"></a>
    <p>Extensions are small add-ons that add new functionality. They can add anything from a toolbar button to a completely new feature.</p>

<?php
$sql = "SELECT TM.ID
FROM  `t_main` TM
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
WHERE  `Type`  =  'E' AND `AppName` = '$application' AND `minAppVer_int`<='$currentver' AND `maxAppVer_int` >='$currentver' AND `approved` = 'YES' GROUP BY TM.ID";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $numextensions = mysql_num_rows($sql_result);
?>
     <a href="/extensions/">Browse extensions</a><BR>(<?php echo"$numextensions"; ?> available for <?php print(ucwords($application)); echo" $currentver_display"; ?>)<BR> 
  </div>
  <div class="frontcolumn">
    <h2><a href="themes/">Get Themes</a></h2>
    <a href="products/mozilla1.x"><img src="images/product-front-mozilla.png" alt="Mozilla" class="promo" width="60" height="60"></a>
    <p>Themes are skins for Firefox, they allow you to change the look and feel of the browser and personalize it to your tastes.</p>
<?php
$sql = "SELECT TM.ID FROM  `t_main` TM
INNER  JOIN t_version TV ON TM.ID = TV.ID
INNER  JOIN t_applications TA ON TV.AppID = TA.AppID
WHERE  `Type`  =  'T' AND `AppName` = '$application' AND `minAppVer_int`<='$currentver' AND `maxAppVer_int` >='$currentver' AND `approved` = 'YES' GROUP BY TM.ID";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $numthemes = mysql_num_rows($sql_result);
?>
     <a href="/themes/">Browse themes</a><BR>(<?php echo"$numthemes"; ?> available for <?php print(ucwords($application)); echo" $currentver_display"; ?>)
  </div>
  <div class="frontcolumnlast">
    <h2><a href="http://www.MozillaStore.com">Get Plugins</a></h2>
    <a href="http://www.MozillaStore.com"><img src="images/front-store.jpg" alt="Mozilla Store" class="promo" width="75" height="75"></a>
    <p>Now you can order all <a href="http://store.mozilla.org/products/software/">Mozilla software on CD</a> and purchase <a href="http://store.mozilla.org/products/clothing">Mozilla logo merchandise</a> at the <a href="http://www.MozillaStore.com">Mozilla Store</a>.</p>
  </div>

  <br style="clear: both;"><br>

  <!-- Start News Columns -->
  <div class="frontcolumn">
<a href="http://www.mozilla.org/news.rdf"><img src="images/rss.png" width="28" height="16" class="rss" alt="Mozilla News in RSS"></a><h2 style="margin-top: 0;"><a href="http://www.mozilla.org" title="the mozilla.org website">New Additions</a></h2>
<ul class="news">
<li>
<div class="date">Aug 28</div>
<a href="http://www.wired.com/wired/archive/12.09/start.html?pg=12">Firefox: Wired</a>
</li>
<li>
<div class="date">Aug 18</div>
<a href="http://www.mozilla.org/press/mozilla-2004-08-18.html">Mozilla Japan Created</a>
</li>
<li>
<div class="date">Aug 18</div>
<a href="http://www.mozilla.org/releases/#1.8a3">Mozilla 1.8 Alpha 3</a>
</li>
</ul>
</div>
<div class="frontcolumn">
<a href="http://planet.mozilla.org/rss10.xml"><img src="images/rss.png" width="28" height="16" class="rss" alt="Mozilla Weblogs in RSS"></a><h2 style="margin-top: 0;"><a href="http://planet.mozilla.org/" title="Planet Mozilla - http://planet.mozilla.org/">Most Popular</a></h2>
<ul class="news">
<li>
<div class="date">Aug 30</div>
<a href="http://weblogs.mozillazine.org/josh/archives/2004/08/gmail_invites.html">Josh Aas: gmail invites</a>
</li>
<li>
<div class="date">Aug 30</div>
<a href="http://weblogs.mozillazine.org/asa/archives/006315.html">Asa Dotzler: gmail invites gone</a>
</li>
<li>
<div class="date">Aug 30</div>
<a href="http://weblogs.mozillazine.org/asa/archives/006314.html">Asa Dotzler: extension update changes</a>
</li>
</ul>
</div>
<div class="frontcolumn">
<a href="http://www.mozillazine.org/atom.xml"><img src="images/rss.png" width="28" height="16" class="rss" alt="MozillaZine News in RSS"></a><h2 style="margin-top: 0;"><a href="http://www.mozillazine.org/" title="Your Source for Daily Mozilla News and Advocacy">This Space For Rent</a></h2>
<ul class="news">
<li>
<div class="date">Aug 25</div>
<a href="http://www.mozillazine.org/talkback.html?article=5215">Camino 0.8.1 Released</a>
</li>
<li>
<div class="date">Aug 25</div>
<a href="http://www.mozillazine.org/talkback.html?article=5213">Community Marketing Initiative Week 5</a>
</li>
<li>
<div class="date">Aug 20</div>
<a href="http://www.mozillazine.org/talkback.html?article=5200">New Beta of mozilla.org Website Available for Testing</a>
</li>
</ul>
</div>

  <!-- End News Columns -->  
  <br style="clear: both;">

</div>
<!-- closes #mBody-->



<?php
// #################################################
//   Old Mozilla Update Layout Code
//    Particularly Editor's Pick Code. 
// #################################################
?>
<?php
//<A HREF="/faq/">Frequently Asked Questions...</A>


if ($_GET["application"]) {$application=$_GET["application"]; }
?>
<?php
//Featured Editor's Pick for Extensions for $application

$sql = "SELECT TR.ID, `Title`, TR.DateAdded, `Body`, `Type`, `pick` FROM `t_reviews`  TR
INNER JOIN t_main TM ON TR.ID = TM.ID
INNER JOIN t_version TV ON TV.ID = TM.ID
INNER JOIN t_applications TA ON TA.AppID = TV.AppID
WHERE `Type` = 'E' AND `AppName` = '$application' AND `pick`='YES' ORDER BY `rID` DESC LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
    $id = $row["ID"];
    $title = $row["Title"];
    $pick = $row["pick"];
    $dateadded = $row["DateAdded"];
    $body = $row["Body"];
    $bodylength = strlen($body);
if ($bodylength>"250") {
 $body = substr($body,0,250);
 $body .= " <a href=\"/extensions/moreinfo.php?id=$id&page=staffreview\">[More...]</a>";
 
 }

//Create Customizeable Timestamp
	$day=substr($dateadded,8,2);  //get the day
    $month=substr($dateadded,5,2); //get the month
    $year=substr($dateadded,0,4); //get the year
    $hour=substr($dateadded,11,2); //get the hour
    $minute=substr($dateadded,14,2); //get the minute
    $second=substr($dateadded,17,2); //get the sec
    $timestamp = strtotime("$year-$month-$day $hour:$minute:$second");
    $date = gmdate("F, Y", $timestamp);


echo"$title<br>&nbsp;&nbsp;&nbsp;$date";
if ($pick=="YES") {echo" Editors Pick";}
echo"<BR><BR>\n";
echo"<SPAN class=\"itemdescription\">$body</SPAN><BR>\n";
}
?>

<?php
//Featured Editor's Pick for Themes for $application
$sql = "SELECT TR.ID, `Title`, TR.DateAdded, `Body`, `Type`, `pick` FROM `t_reviews`  TR
INNER JOIN t_main TM ON TR.ID = TM.ID
INNER JOIN t_version TV ON TV.ID = TM.ID
INNER JOIN t_applications TA ON TA.AppID = TV.AppID
WHERE `Type` = 'T' AND `AppName` = '$application' AND `pick`='YES' ORDER BY `rID` DESC LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
    $id = $row["ID"];
    $title = $row["Title"];
    $pick = $row["pick"];
    $dateadded = $row["DateAdded"];
    $body = $row["Body"];
    $bodylength = strlen($body);
if ($bodylength>"250") {
 $body = substr($body,0,250);
 $body .= " <a href=\"/moreinfo.php?id=$id&page=staffreview\">[More...]</a>";
 
 }

//Create Customizeable Timestamp
	$day=substr($dateadded,8,2);  //get the day
    $month=substr($dateadded,5,2); //get the month
    $year=substr($dateadded,0,4); //get the year
    $hour=substr($dateadded,11,2); //get the hour
    $minute=substr($dateadded,14,2); //get the minute
    $second=substr($dateadded,17,2); //get the sec
    $timestamp = strtotime("$year-$month-$day $hour:$minute:$second");
    $date = gmdate("F, Y", $timestamp);

echo"$title - $date";
if ($pick=="YES") {echo" Editors Pick<BR><BR>\n";}
echo"$body<BR>\n";
}
?>


<?php
include"$page_footer";
?>
</BODY>
</HTML>