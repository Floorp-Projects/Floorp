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
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">

<head>
<?php
//Bookmarking-Friendly Page Title
$id = escape_string($_GET["id"]);
$sql = "SELECT  Name FROM `main`  WHERE ID = '$id' AND Type = 'T' LIMIT 1";
$sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    if (mysql_num_rows($sql_result)===0) {
        $return = page_error("1","Theme ID is Invalid or Missing.");
        exit;
    }
    $row = mysql_fetch_array($sql_result);

//Page Titles
    $pagetitles = array("releases"=>"All Releases", "previews"=>"Preview Images", "comments"=>"User Comments", "staffreview"=>"Editor Review", "opinion"=>" My Opinion");
    $pagetitle = $pagetitles[$_GET["page"]];
?>

    <title>Mozilla Update :: Themes -- More Info: <?php echo"$row[Name]"; if ($pagetitle) {echo" - $pagetitle"; } ?></title>

    <?php
    installtrigger("themes");
    
    include"$page_header";
    ?>

<div id="mBody">

<?php
$type = "T";
$index = yes;
$category=$_GET["category"];
include"inc_sidebar.php";
?>

<?php
$id = escape_string($_GET["id"]);


//Get Author Data
$sql2 = "SELECT  TM.Name, TU.UserName, TU.UserID, TU.UserEmail FROM  `main`  TM 
        LEFT JOIN authorxref TAX ON TM.ID = TAX.ID
        INNER JOIN userprofiles TU ON TAX.UserID = TU.UserID
        WHERE TM.ID = '$id'
        ORDER  BY  `Type` , `Name`  ASC ";
$sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while ($row2 = mysql_fetch_array($sql_result2)) {
        $authorarray[$row2[Name]][] = $row2["UserName"];
        $authorids[$row2[UserName]] = $row2["UserID"];
   }

//Assemble a display application version array
$sql = "SELECT `Version`, `major`, `minor`, `release`, `SubVer` FROM `applications` WHERE `AppName`='$application' ORDER BY `major`,`minor`";
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
    FROM  `main` TM
    INNER  JOIN version TV ON TM.ID = TV.ID
    INNER  JOIN applications TA ON TV.AppID = TA.AppID
    INNER  JOIN os TOS ON TV.OSID = TOS.OSID";

    if ($category && $category !=="%") {
        $sql .=" INNER  JOIN categoryxref TCX ON TM.ID = TCX.ID
        INNER JOIN categories TC ON TCX.CategoryID = TC.CategoryID ";
    }
    if ($editorpick=="true") {
        $sql .=" INNER JOIN reviews TR ON TM.ID = TR.ID ";
    }

    $sql .=" WHERE TM.ID = '$id'";

    if ($_GET["vid"]) {
        $vid=$_GET["vid"];
        $sql .=" AND TV.vID = '$vid' ";

    } else {

        $sql .=" AND Type = '$type' AND AppName = '$application' AND `approved` = 'YES' ";
        if ($editorpick=="true") {
            $sql .="AND TR.Pick = 'YES' ";
        }
        if ($category && $category !=="%") {
            $sql .="AND CatName LIKE '$category' ";
        }
        if ($app_version) {
            $sql .=" AND TV.MinAppVer_int <= '".strtolower($app_version)."' AND TV.MaxAppVer_int >= '".strtolower($app_version)."' ";
        }
        if ($OS) {
            $sql .=" AND (TOS.OSName = '$OS' OR TOS.OSName = 'All') ";
        }
    }

    $sql .= "\nORDER  BY  `Name` , `Version` DESC LIMIT 1";

    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    $row = mysql_fetch_array($sql_result);

        $v++;
        $vid = $row["vID"];
        $name = $row["Name"];
        $dateadded = $row["DateAdded"];
        $dateupdated = $row["DateUpdated"];
        $homepage = $row["Homepage"];
        $description = $row["Description"];
        $rating = $row["Rating"];
        $authors = $authorarray[$name];
        $appname = $row["AppName"];
        $osname = $row["OSName"];
        $verdateadded = $row["VerDateAdded"];
        $verdateupdated = $row["VerDateUpdated"];
        $filesize = $row["Size"];
        $notes = $row["Notes"];
        $version = $row["Version"];
        $uri = $row["URI"];
        $downloadcount = $row["TotalDownloads"];
        $populardownloads = $row["downloadcount"];

        if (!$_GET['vid']) {
            $_GET['vid']=$vid;
        }

        if ($appvernames[$row["MinAppVer"]]) {
            $minappver = $appvernames[$row["MinAppVer"]];
        } else {
            $minappver = $row["MinAppVer"];
        }

        if ($appvernames[$row["MaxAppVer"]]) {
            $maxappver = $appvernames[$row["MaxAppVer"]];
        } else {
            $maxappver = $row["MaxAppVer"];
        }

        if ($VerDateAdded > $dateadded) {
            $dateadded = $VerDateAdded;
        }

        if ($VerDateUpdated > $dateupdated) {
            $dateupdated = $VerDateUpdated;
        }

    //Turn Authors Array into readable string...
    $authorcount = count($authors);
    if (!$authors) {
        $authors = array();
    }
    foreach ($authors as $author) {
        $userid = $authorids[$author];
        $n++;
        $authorstring .= "<A HREF=\"authorprofiles.php?".uriparams()."&amp;id=$userid\">$author</A>";
        if ($authorcount != $n) {
            $authorstring .=", ";
        }
    }
    $authors = $authorstring;
    unset($authorstring, $n); // Clear used Vars.. 

    // Create Date String
    if ($dateupdated > $dateadded) {
        $timestamp = $dateupdated;
        $datetitle = "Last Updated: ";
    } else {
        $timestamp = $dateadded;
        $datetitle = "Released On: ";
    }

    $date = date("F d, Y g:i:sa",  strtotime("$timestamp"));
    $releasedate = date("F d, Y",  strtotime("$dateadded"));
    $datestring = "$datetitle $date";

    //Rating
    if (!$rating) {
        $rating="0";
    }

    //No Results Returned for Main Query, throw the Incompatible Error.
    if (mysql_num_rows($sql_result)=="0") {
        echo"<div id=\"mainContent\">\n";
        echo"<h1>Incompatible Theme or Theme No Longer Available</h1>\n";
        echo"The theme you requested is either incompatible with the application selected, or the version of it is no longer available on Mozilla Update.<br><br>\n";
        echo"To try your request again for a different application version, use the form below.<br>\n";

        echo"<form name=\"changeapp\" method=\"get\" action=\"?\">
            <input name=\"id\" type=\"hidden\" value=\"$id\">
            <input name=\"os\" type=\"hidden\" value=\"$OS\">
            <strong>".ucwords($application)."</strong> <input name=\"application\" type=\"hidden\" value=\"$application\">";
        echo"<select name=\"version\">";

        $sql = "SELECT `Version`,`major`,`minor`,`release`,`SubVer` FROM `applications` WHERE `AppName`='$application' and `public_ver` ='YES' ORDER BY `major` DESC, `minor` DESC, `release` DESC, `SubVer` DESC";
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
        echo"</div>\n</div>\n";
	    include"$page_footer";
        echo"</body>\n</html>\n";
        exit;
    }

    //Get Preview Image URI
    $sql3 = "SELECT `PreviewURI`, `caption` from `previews` WHERE `ID` = '$id' AND `preview`='YES' LIMIT 1";
    $sql_result3 = mysql_query($sql3, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        $row3 = mysql_fetch_array($sql_result3);
            $previewuri = $row3["PreviewURI"];
            $caption = $row3["caption"];
?>

	<div id="mainContent">
		<div class="rating">Rating: <img src="/images/stars-3-8.gif" width="90" height="20" title="3.8 Stars out of 5" alt="3.8 Stars"></div>
		<h2 class="first"><strong><?php echo"$name"; ?></strong> - <?php echo ucwords("$application")." Theme"; ?></h2>
		<p class="first"><?php echo"$name $version"; ?>, by <?php echo"$authors"; ?>, released on <?php echo"$releasedate"; ?></p>


    <?php
    //Begin Pages
    $page = $_GET["page"];
    if (!$page or $page=="general") {
    //General Page / Default
    ?>


        <?php		
        if ($previewuri) {
        ?>
        <p class="screenshot">
        <?php
        $sql = "SELECT `PreviewID` from `previews` WHERE `ID`='$id' and `preview`='NO' LIMIT 1";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
            if (mysql_num_rows($sql_result)>"0") {
        ?>
        <A HREF="?<?php echo"".uriparams()."&amp;id=$id&amp;page=previews"; ?>">
        <?php } ?>
        <?php
            list($width, $height, $attr) = getimagesize("$websitepath"."$previewuri");
            echo"<img src=\"$previewuri\" height=$height width=$width alt=\"$name preview - $caption\" title=\"$caption\">\n";
        ?>
        <?php if (mysql_num_rows($sql_result)>"0") { ?>
        </a>
        <?php } ?>
        </p>
        <?php
        }
        ?>
		
		<h3>Quick Description</h3>
		<p class="first">

        <?php
        //Categories
        $sql = "SELECT `CatName` from `categoryxref` TCX INNER JOIN `categories` TC ON TCX.CategoryID=TC.CategoryID  WHERE `ID`='$id' ORDER BY `CatName` ASC";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
            $num_results = mysql_num_rows($sql_result); $i=0;
            while ($row = mysql_fetch_array($sql_result)) {
                $i++;
                $categories .= $row["CatName"];
                if ($num_results < $i ) {
                    $categories .= ", ";
                }
            }
        ?>
    


        <?php
        echo"$description"; 
        if ($notes) {
            echo"<br><br>$notes\n";
        }
        if ($categories) {
            echo" (Categories: $categories)";
        }
        ?>
        </p>
		<p class="requires">Requires: <?php echo ucwords($appname).": $minappver - $maxappver"; ?> <img src="/images/<?php echo strtolower($appname); ?>_icon.png" width="34" height="34" alt="<?php echo ucwords($appname); ?>">
        <?php
        if($osname !=="ALL") {
            echo"on ".ucwords($osname)." <IMG SRC=\"/images/".strtolower($osname)."_icon.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"<?php echo ucwords($osname); ?>\">";
        }
        ?>
        </p>

		<div class="key-point install-box"><div class="install"><?php 

            if ($appname=="Thunderbird") { 
                echo"<A HREF=\"/core/install.php?passthrough=yes&amp;uri=$uri\" onclick=\"return installTheme(event,'$name $version for Thunderbird');\"  TITLE=\"Right-Click to Download $name $version\">";
            } else {
                echo"<a href=\"$uri\" onclick=\"return installTheme(event,'$name $version');\" TITLE=\"Install $name $version (Right-Click to Download)\">";
            }
        ?><strong>Install Now</strong> (<?php echo"$filesize"; ?>&nbsp;KB&nbsp;File)</a></div></div>

        <?php
        //Special Theme Installation Instructions for Thunderbird users
            if ($application=="thunderbird") {
                echo"<SPAN style=\"font-size: 10pt; color: #00F\">Theme Install Instructions for Thunderbird Users:</SPAN><BR>
                    <SPAN style=\"font-size: 8pt;\">(1) Right-Click the link above and choose \"Save Link As...\" to Download and save the file to your hard disk.<BR>
                    (2) In Mozilla Thunderbird, open the theme manager (Tools Menu/Themes)<BR>
                    (3) Click the Install button, and locate/select the file you downloaded and click \"OK\"</SPAN><BR>";
            }
        ?>
		
		<!-- Only Display Editor's Review if it's been written -->
        <?php
        $sql = "SELECT `Title`, `DateAdded`, `Body`, `Pick` FROM `reviews` WHERE `ID` = '$id' LIMIT 1";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
            if (mysql_num_rows($sql_result)>"0") {
                $row = mysql_fetch_array($sql_result);
                $title = $row["Title"];
                $dateadded = $row["DateAdded"];
                $body = $row["Body"];
                $pick = $row["Pick"];
                $date = gmdate("F, Y", strtotime("$dateadded")); //Create Customizeable Timestamp
        ?>
		<h3>Editor's Review</h3>
        <?php
            echo"<strong>$title\n";
            if ($pick=="YES") {
                echo"&nbsp;&#8212;&nbsp;$date Editors Pick\n";
            }
            echo"</strong><br>\n";
        ?>
		<p class="first"><?php echo"$body"; ?></p>
        <?php
            }
        ?>


        <!-- Only Display Developers Comments if they're written -->
        <?php
        $sql = "SELECT `devcomments` FROM `main` WHERE `id`='$id' LIMIT 1";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
            $row = mysql_fetch_array($sql_result);
            if ($row["devcomments"]) {
                $devcomments = nl2br($row["devcomments"]);
                echo"<h3>Developer Comments:</h3>\n";
                echo"<p class=\"first\">$devcomments</p>\n";
            }
        ?>

		<h3 id="user-comments">User Comments</h3>
		<p><strong><a href="?<?php echo"".uriparams()."&amp;id=$id&amp;page=opinion"; ?>">Add your own opinion &#187;</a></strong></p>
		<ul id="opinions">
        <?php
        $sql = "SELECT CommentName, CommentTitle, CommentNote, CommentDate, CommentVote FROM  `feedback` WHERE ID = '$id' AND CommentNote IS NOT NULL ORDER  BY `CommentDate` DESC LIMIT 5";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
            $num_results = mysql_num_rows($sql_result);
            while ($row = mysql_fetch_array($sql_result)) {
                $commentname = $row["CommentName"];
                $commenttitle = $row["CommentTitle"];
                $commentnotes = $row["CommentNote"];
                $commentdate = $row["CommentDate"];
                $rating = $row["CommentVote"];
                $commentdate = gmdate("F d, Y g:ia", strtotime("$commentdate")); //Create Customizeable Datestamp

                echo"<li>\n";
                echo"<h4>$commenttitle by $commentname</h4>\n";
                echo"<p class=\"opinions-date\"><a href=\"#permalink\">$commentdate</a></p>\n";
                echo"<p class=\"opinions-text\">$commentnotes</P>\n";
                echo"<p class=\"opinions-rating\" title=\"$rating of 5 stars\">";
                for ($i = 1; $i <= $rating; $i++) {
                    echo"<IMG SRC=\"/images/stars/star_icon.png\" WIDTH=17 HEIGHT=20 ALT=\"*\">";
                }
                for ($i = $i; $i <= 5; $i++) {
                echo"<IMG SRC=\"/images/stars/graystar_icon.png\" WIDTH=17 HEIGHT=20 ALT=\"\">";
                }
                echo"</p>\n";
                echo"</li>\n";
            }

            if ($num_results=="0") {
                echo"<li>\n";
                echo"<h4>Nobody's Commented on this Theme Yet</h4>\n";
                echo"<p class=\"opinions-text\">Be the First! <A HREF=\"moreinfo.php?".uriparams()."&amp;id=$id&amp;page=opinion\">Rate It!</A></p>";
                echo"</li>\n";
            }
        ?>
		</ul>
		<p><strong><a href="?<?php echo"".uriparams()."&amp;id=$id&amp;&amp;page=comments"; ?>">Read all opinions &#187;</a></strong></p>

		<h3>Theme Details</h3>
		<ul>
		<li><?php echo"$datestring"; // Last Updated: September 11, 2004 5:38am ?>
		<li>Total Downloads: <?php echo"$downloadcount"; ?> &nbsp;&#8212;&nbsp; Downloads this Week: <?php echo"$populardownloads"; ?></li>
		<li>See <a href="?<?php echo"".uriparams()."&amp;id=$id&amp;page=releases"; ?>">all previous releases</a> of this theme.</li>
        <?php
        if ($homepage) {
        ?>
		<li>View the Author's <a href="<?php echo"$homepage"; ?>">Homepage</a> for this Theme.</li>
        <?php
        }
        ?>
		</ul>


    <?php
    } else if ($page=="releases") {

    echo"<h3>All Releases</h3>";

    $sql = "SELECT TV.vID, TV.Version, TV.MinAppVer, TV.MaxAppVer, TV.Size, TV.URI, TV.Notes, TA.AppName, TOS.OSName
            FROM  `version` TV
            INNER  JOIN applications TA ON TV.AppID = TA.AppID
            INNER  JOIN os TOS ON TV.OSID = TOS.OSID
            WHERE TV.ID = '$id' AND `approved` = 'YES' AND TA.AppName = '$application'
            ORDER  BY  `Version` DESC, `OSName` ASC";
     $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        while ($row = mysql_fetch_array($sql_result)) {
            $vid = $row["vID"];
            if ($appvernames[$row["MinAppVer"]]) {
                $minappver = $appvernames[$row["MinAppVer"]];
            } else {
                $minappver = $row["MinAppVer"];
            }
            if ($appvernames[$row["MaxAppVer"]]) {
                $maxappver = $appvernames[$row["MaxAppVer"]];
            } else {
                $maxappver = $row["MaxAppVer"];
            }
            $filesize = $row["Size"];
            $notes = $row["Notes"];
            $version = $row["Version"];
            $uri = $row["URI"];
            $osname = $row["OSName"];
            $appname = $row["AppName"];
            $filename = basename($uri);

            echo"<DIV>"; //Open Version DIV

            //Description & Version Notes
            echo"<h3><A HREF=\"moreinfo.php?".uriparams()."&amp;id=$id&amp;vid=$vid\">$name $version</A></h3>\n";
            if ($notes) {
                echo"$notes<br><br>\n";
            }

            //Icon Bar Modules
            echo"<DIV style=\"height: 34px\">";
            echo"<DIV class=\"iconbar\">";
            if ($appname=="Thunderbird") {
                echo"<A HREF=\"/core/install.php?passthrough=yes&uri=$uri\" onclick=\"return installTheme(event,'$name $version for Thunderbird');\">";
            } else {
                echo"<a href=\"$uri\" onclick=\"return installTheme(event,'$name $version');\">";
            }
            echo"<IMG SRC=\"/images/download.png\" HEIGHT=34 WIDTH=34 TITLE=\"Install $name (Right-Click to Download)\" ALT=\"\">Install</A><BR><SPAN class=\"filesize\">Size: $filesize kb</SPAN></DIV>";
            echo"<DIV class=\"iconbar\"><IMG SRC=\"/images/".strtolower($appname)."_icon.png\" HEIGHT=34 WIDTH=34 ALT=\"\">&nbsp;For $appname:<BR>&nbsp;&nbsp;$minappver - $maxappver</DIV>";
            if($osname !=="ALL") {
                echo"<DIV class=\"iconbar\"><IMG SRC=\"/images/".strtolower($osname)."_icon.png\" HEIGHT=34 WIDTH=34 ALT=\"\">For&nbsp;$osname<BR>only</DIV>";
            }
            echo"</DIV><BR>\n";

            echo"</DIV>";
        }


    } else if ($page=="comments") {
    //Comments/Ratings Page

    if ($_GET["numpg"]) {$items_per_page=escape_string($_GET["numpg"]); } else {$items_per_page="25";} //Default Num per Page is 25
    if (!$_GET["pageid"]) {$pageid="1"; } else { $pageid = escape_string($_GET["pageid"]); } //Default PageID is 1
    $startpoint = ($pageid-1)*$items_per_page;

    $sql = "SELECT CommentID FROM  `feedback` WHERE ID = '$id'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        $num_pages = ceil(mysql_num_rows($sql_result)/$items_per_page);

    echo"<h3>User Comments:</h3>";

    if ($pageid <=$num_pages) {
        $previd=$pageid-1;
        if ($previd >"0") {
            echo"<a href=\"?".uriparams()."&amp;id=$id&amp;page=$page&amp;pageid=$previd\">&#171; Previous</A> &bull; ";
        }
    }

    echo"Page $pageid of $num_pages";
    $nextid=$pageid+1;

    if ($pageid <$num_pages) {
        echo" &bull; <a href=\"?".uriparams()."$id&amp;page=$page&amp;pageid=$nextid\">Next &#187;</a>";
    }
    echo"<BR>\n";
    ?>
		<ul id="opinions">
    <?php
    $sql = "SELECT CommentID, CommentName, CommentTitle, CommentNote, CommentDate, CommentVote, `helpful-yes`,`helpful-no` FROM  `feedback` WHERE ID = '$id' ORDER  BY  `CommentDate` DESC LIMIT $startpoint, $items_per_page";
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


                echo"<li>\n";
                echo"<a name=\"$commentid\"></a>\n";
                echo"<h4>$title by $name</h4>\n";
                echo"<p class=\"opinions-date\"><a href=\"#$commentid\">$date</a></p>\n";
                echo"<p class=\"opinions-text\">$notes</P>\n";
                echo"<p class=\"opinions-rating\" title=\"$rating of 5 stars\">";
                for ($i = 1; $i <= $rating; $i++) {
                    echo"<IMG SRC=\"/images/stars/star_icon.png\" WIDTH=17 HEIGHT=20 ALT=\"*\">";
                }
                for ($i = $i; $i <= 5; $i++) {
                echo"<IMG SRC=\"/images/stars/graystar_icon.png\" WIDTH=17 HEIGHT=20 ALT=\"\">";
                }
                if ($helpful_yes>0 or $helpful_no>0) {
                $helpful_total=$helpful_yes+$helpful_no;
                    echo"<br>$helpful_yes of $helpful_total people found this comment helpful.<br>\n";
                }

                echo"Was this comment helpful to you? <a href=\"../core/commenthelpful.php?".uriparams()."&amp;id=$id&amp;type=$type&amp;commentid=$commentid&amp;pageid=$pageid&amp;action=yes\">Yes</a>&nbsp;&nbsp;&nbsp;<a href=\"../core/commenthelpful.php?".uriparams()."&amp;id=$id&amp;type=$type&amp;commentid=$commentid&amp;pageid=$pageid&amp;action=no\">No</a>";
                echo" <span style=\"font-size: xx-small\"><a href=\"../core/reportcomment.php?".uriparams()."&amp;id=$id&amp;type=$type&amp;commentid=$commentid&amp;pageid=$pageid&amp;action=report\" ONCLICK=\"return confirm('Report this comment as inappropriate on the site?');\">(Report Comment)</a></span>";
                echo"<BR>";

                echo"</p>\n";
                echo"</li>\n";

            }

            if ($num_results=="0") {
                echo"<li>\n";
                echo"<h4>Nobody's Commented on this Theme Yet</h4>\n";
                echo"<p class=\"opinions-text\">Be the First! <A HREF=\"moreinfo.php?".uriparams()."&amp;id=$id&amp;page=opinion\">Rate It!</A></p>";
                echo"</li>\n";
            }
        ?>
    </ul>

    <?php
    // Begin Code for Dynamic Navbars
    if ($pageid <=$num_pages) {
        $previd=$pageid-1;
        if ($previd >"0") {
            echo"<a href=\"?".uriparams()."&amp;id=$id&amp;page=$page&amp;pageid=$previd\">&#171; Previous</A> &bull; ";
        }
    }

    echo"Page $pageid of $num_pages";
    $nextid=$pageid+1;

    if ($pageid <$num_pages) {
        echo" &bull; <a href=\"?".uriparams()."&amp;id=$id&amp;page=$page&amp;pageid=$nextid\">Next &#187;</a>";
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
                echo"<A HREF=\"?".uriparams()."&amp;id=$id&amp;page=$page&amp;pageid=$i\">$i</A>&nbsp;";
            }
            $i++;
        }
    }

    if ($num_pages>1) {
        echo"<br>\nComments per page: \n";
        $perpagearray = array("25","50","100");
        foreach ($perpagearray as $items_per_page) {
           echo"<A HREF=\"?".uriparams()."&amp;id=$id&amp;page=$page&amp;pageid=1\">$items_per_page</A>&nbsp;";
        }
    }


    } else if ($page=="previews") {

    // Item Previews Tab
    echo"<h2>Previews for $name</h2>\n";

    $sql = "SELECT `PreviewURI`,`caption` from `previews` WHERE `ID`='$id' and `preview`='NO' ORDER BY `PreviewID` ASC";
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

            echo"<img src=\"$uri\" alt=\"$caption\" width=\"$dest_width\" height=\"$dest_height\"><br>\n";

        }


    } else if ($page=="staffreview") {

    //Staff/Editor Review Tab
     echo"<h3>Editor Review</h3>\n";
    $sql = "SELECT TR.ID, `Title`, TR.DateAdded, `Body`, `Type`, `Pick`, TU.UserName FROM `reviews`  TR
            INNER  JOIN main TM ON TR.ID = TM.ID
            INNER  JOIN userprofiles TU ON TR.AuthorID = TU.UserID
            WHERE `Type` = 'T' AND TR.ID = '$id' ORDER BY `rID` DESC LIMIT 1";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        $num_results = mysql_num_rows($sql_result);
        while ($row = mysql_fetch_array($sql_result)) {
            $id = $row["ID"];
            $title = $row["Title"];
            $dateadded = $row["DateAdded"];
            $body = $row["Body"];
            $pick = $row["Pick"];
            $username = $row["UserName"];
            $date = gmdate("F, Y", strtotime("$dateadded")); //Create Customizeable Timestamp
            $posteddate = date("F j Y, g:i:sa", strtotime("$dateadded"));

            echo"<h3>$title\n";
            if ($pick=="YES") {
                echo"&nbsp;&#8212;&nbsp;$date Editors Pick\n";
            }
            echo"</h3>\n";
            echo"Posted on $posteddate by $username<br>\n";
            echo"<p class=\"first\">$body</p>\n";
        }

        $typename = "theme";
        if ($num_results=="0") {
            echo"This $typename has not yet been reviewed.<BR><BR>

            To see what other users think of this $typename, view the <A HREF=\"moreinfo.php?".uriparams()."&amp;id=$id&amp;page=comments\">User Comments...</A>
            ";
        }


    } else if ($page=="opinion") {

        //My Opinion Tab
        echo"<h3>Your Comments about $name:</h3>";

        if ($_GET["error"]=="norating") {
            echo"<DIV class=\"errorbox\">\n
            Your comment submission had the following error(s), please fix these errors and try again.<br>\n
            &nbsp;&nbsp;&nbsp;Rating cannot be left blank.<br>\n
            &nbsp;&nbsp;&nbsp;Review/Comments cannot be left blank.<br>\n
            </DIV>\n";
        }
    ?>
    <FORM NAME="opinon" METHOD="POST" ACTION="../core/postfeedback.php?<?php echo uriparams(); ?>">
    <DIV>
    <input name="formkey" type="hidden" value="<?php print(md5(substr(0,10,md5(mt_rand())))); ?>">
    <INPUT NAME="id" TYPE="HIDDEN" VALUE="<?php echo"$id"; ?>">
    <INPUT NAME="vid" TYPE="HIDDEN" VALUE="<?php echo"$vid"; ?>">
    <INPUT name="type" type="hidden" value="E">
    Your Name:*<BR>
    <INPUT NAME="name" TYPE="TEXT" SIZE=30 MAXLENGTH=30><BR>

    Your E-Mail (optional):<BR>
    <INPUT NAME="email" TYPE="TEXT" SIZE=30 MAXLENGTH=100><BR>

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

    Title:*<BR>
    <INPUT NAME="title" TYPE="TEXT" SIZE=30 MAXLENGTH=50><BR>

    Review/Comments:*<BR>
    <TEXTAREA NAME="comments" ROWS=5 COLS=55></TEXTAREA><BR>
    <INPUT NAME="submit" TYPE="SUBMIT" VALUE="Post">&nbsp;&nbsp;<INPUT NAME="reset" TYPE="RESET" VALUE="Reset"><BR>
    <SPAN class="smallfont">* Required Fields</SPAN>
    </DIV>
    </FORM>

    <?php

    } // End Pages
    ?>

	</div>
<!-- closes #mainContent-->

</div>

<!-- closes #mBody-->

<?php
include"$page_footer";
?>
</body>
</html>