<?php
require"../core/config.php";
require"core/sessionconfig.php";
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<TITLE>Mozilla Update :: Developer Control Panel :: Item Overview</TITLE>
<?php
include"$page_header";
include"inc_sidebar.php";
?>
<?php
$id = $_GET["id"];
$sql = "SELECT  TM.ID, TM.GUID, TM.Name, TM.Homepage, TM.Description, TM.downloadcount, TM.TotalDownloads, TM.Rating, TU.UserEmail FROM  `t_main`  TM 
LEFT JOIN t_authorxref TAX ON TM.ID = TAX.ID
INNER JOIN t_userprofiles TU ON TAX.UserID = TU.UserID
WHERE TM.ID = '$id' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $numresults = mysql_num_rows($sql_result);
  $row = mysql_fetch_array($sql_result);
$v++;
    $id = $row["ID"];
    $guid = $row["GUID"];
    $name = $row["Name"];
    $dateadded = $row["DateAdded"];
    $dateupdated = $row["DateUpdated"];
    $homepage = $row["Homepage"];
    $description = nl2br($row["Description"]);
    $downloadcount = $row["downloadcount"];
    $totaldownloads = $row["TotalDownloads"];
    $rating = $row["Rating"];

$i=""; $categories="";
$sql = "SELECT  TC.CatName FROM  `t_categoryxref`  TCX 
INNER JOIN t_categories TC ON TCX.CategoryID = TC.CategoryID
WHERE TCX.ID = '$id'";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $numresults = mysql_num_rows($sql_result);
  while ($row = mysql_fetch_array($sql_result)) {
    $i++;
    $catname = $row["CatName"];
    $categories .="$catname";
    if ($i<$numresults) {$categories .=", ";}

  }
?>
<div id="mainContent" class="right">
<h2>Item Overview :: <?php echo"$name"; ?></h2>
<?php
echo"<a href=\"listmanager.php?function=editmain&id=$id\">Edit $name</a><br>\n";
echo"$description<br>\n";
if ($guid) {echo"GUID: $guid<br>\n"; }
if ($homepage) {echo"Homepage: <a href=\"$homepage\">$homepage</a><br>\n";}
echo"Categories: $categories<br>\n";

?>

<h3>Listed Versions</h3>
<?php
$approved_array = array("?"=>"Pending Approval", "YES"=>"Approved", "NO"=>"Denied", "DISABLED"=>"Disabled");
$sql = "SELECT vID, TV.Version, URI, OSName, approved FROM `t_version` TV
INNER JOIN t_os TOS ON TOS.OSID = TV.OSID
WHERE `ID`='$id' GROUP BY `URI` ORDER BY `Version`";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while($row = mysql_fetch_array($sql_result)) {
    $vid = $row["vID"];
    $version = $row["Version"];
    $uri = $row["URI"];
    $filename = basename($row["URI"]);
    $os = $row["OSName"];
    $approved = $row["approved"];
    $approved = $approved_array["$approved"];

echo"<h4><a href=\"listmanager.php?function=editversion&id=$id&vid=$vid\">Version $version</a> - $approved</h4>\n";
echo"$filename - for $os<br>\n";


$sql2 = "SELECT TV.Version, AppName, MinAppVer, MaxAppVer FROM `t_version` TV
    INNER JOIN t_applications TA ON TA.AppID = TV.AppID
    WHERE `ID`='$id' AND `URI`='$uri' ORDER BY TV.Version, TA.AppName";
    $sql_result2 = mysql_query($sql2, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while($row2 = mysql_fetch_array($sql_result2)) {

    $app = $row2["AppName"];
    $minappver = $row2["MinAppVer"];
    $maxappver = $row2["MaxAppVer"];


    echo"For $app:&nbsp;&nbsp;$minappver - $maxappver<BR>\n";


    }

 }
?>


	</div>
    <div id="side" class="right">
    <h2>Statistics</h2>
        <img src="/images/download.png" border=0 height=34 width=34 alt="" class="iconbar">Downloads this Week: <?php echo"$downloadcount"; ?><br>
        Total Downloads: <?php echo"$totaldownloads"; ?><BR>
    <BR>
        <img src="/images/ratings.png" border=0 height=34 width=34 alt="" class="iconbar">Rated: <?php echo"$rating"; ?> of 5<BR>&nbsp;<br>
    <BR>
        <img src="/images/edit.png" border=0 height=34 width=34 alt="" class="iconbar">Comments: <?php echo"$num_comments"; ?><BR>&nbsp;<br>

    <h2>Developer Comments</h2>
<?php
if ($_POST["submit"]=="Post Comments") {
  $id = $_POST["id"];
  $comments = $_POST["comments"];
  if (checkFormKey()) {
    $sql = "UPDATE `t_main` SET `devcomments`='$comments' WHERE `id`='$id'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    if ($sql_result) { echo"Developer Comments Updated...<br>\n"; }
  }
}

$sql = "SELECT `devcomments` FROM `t_main` WHERE `id`='$id' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
   $devcomments = $row["devcomments"];
 
?>
<form name="devcomments" method="post" action="itemoverview.php?id=<?php echo"$id"; ?>">
<?writeFormKey();?>
<input name="id" type="hidden" value="<?php echo"$id"; ?>">
<textarea name="comments" rows=10 cols=26><?php echo"$devcomments"; ?></textarea><br>
<input name="submit" type="submit" value="Post Comments">&nbsp;<input name="reset" type="reset" value="Reset">
</form>

    <h2><a href="previews.php?id=<?php echo"$id"; ?>">Previews</a></h2>
<?php
$sql = "SELECT * FROM `t_previews` TP WHERE `ID`='$id' AND `preview`='YES' ORDER BY `PreviewID` LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {
    $i++;
    $previewid = $row["PreviewID"];
    $uri = $row["PreviewURI"];
    $filename = basename($row["PreviewURI"]);
    $filename_array[$i] = $filename;
    $caption = $row["caption"];
    $preview = $row["preview"];
    list($src_width, $src_height, $type, $attr) = getimagesize("$websitepath/$uri");

    echo"<a href=\"previews.php?id=$id\"><img src=\"$uri\" border=0 $attr alt=\"$caption\"></a><br>$caption\n";
   }
   if (mysql_num_rows($sql_result)=="0") {echo"<a href=\"previews.php?id=$id\">Add a Preview</a>...<br>\n"; }
?>



    </div>


<?php
include"$page_footer";
?>
</BODY>
</HTML>