<?php
require"../core/config.php";
require"core/sessionconfig.php";
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<TITLE>Mozilla Update :: Developer Control Panel :: Overview</TITLE>
<?php
include"$page_header";
include"inc_sidebar.php";
?>
<h2>Welcome <?php echo"$_SESSION[name]";?>!</h2>

<P>
<?php
if ($_SESSION["level"] == "admin" or $_SESSION["level"] == "moderator") {

$sql ="SELECT TM.ID FROM `t_main` TM
INNER JOIN `t_version` TV ON TM.ID = TV.ID
WHERE `approved` = '?' ORDER BY TV.DateUpdated ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $num_result = mysql_num_rows($sql_result);
?>
<SPAN STYLE="font-weight:bold">Approval Queue Status: <A HREF="approval.php?function=approvalqueue"><?php echo"$num_result"; ?> Pending Approval</A></SPAN>

<?php } ?>
<h3>My Extensions</h3>

<?php
$sql = "SELECT  TM.ID, TM.Type, TM.Name, TM.Description, TM.downloadcount, TM.TotalDownloads, TM.Rating, TU.UserEmail FROM  `t_main`  TM 
LEFT JOIN t_authorxref TAX ON TM.ID = TAX.ID
INNER JOIN t_userprofiles TU ON TAX.UserID = TU.UserID
WHERE TU.UserID = '$_SESSION[uid]' AND TM.Type ='E'
ORDER  BY  `Type` , `Name` ";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $numresults = mysql_num_rows($sql_result);
  while ($row = mysql_fetch_array($sql_result)) {

$v++;
    $id = $row["ID"];
    $type = $row["Type"];
    $name = $row["Name"];
    $dateadded = $row["DateAdded"];
    $dateupdated = $row["DateUpdated"];
    $homepage = $row["Homepage"];
    $description = nl2br($row["Description"]);
    $authors = $row["UserEmail"];
    $downloadcount = $row["downloadcount"];
    $totaldownloads = $row["TotalDownloads"];
    $rating = $row["Rating"];

echo"<h4><A HREF=\"itemoverview.php?id=$id\">$name</A></h4>\n";
echo"$description\n";
//Icon Bar
echo"<DIV style=\"margin-top: 10px; height: 34px\">";
echo"<DIV class=\"iconbar\"><A HREF=\"itemoverview.php?id=$id\"><IMG SRC=\"/images/edit.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">&nbsp;Edit Item</A></DIV>";
echo"<DIV class=\"iconbar\"><IMG SRC=\"/images/download.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">Downloads: $downloadcount<BR>&nbsp;&nbsp;$totaldownloads total</DIV>";
echo"<DIV class=\"iconbar\" title=\"$rating of 5 stars\"><A HREF=\"/extensions/moreinfo.php?id=$id&page=comments\"><IMG SRC=\"/images/ratings.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">Rated<br>&nbsp;&nbsp;$rating of 5</A></DIV>";
echo"</DIV>";

}
?>
&nbsp;&nbsp;&nbsp;&nbsp;<a href="additem.php?type=E">Add New Extension...</a>
</P>
<P>

<h3>My Themes</h3>
<?php
$sql = "SELECT  TM.ID, TM.Type, TM.Name, TM.DateAdded, TM.Description, TM.downloadcount, TM.TotalDownloads, TM.Rating, TU.UserEmail FROM  `t_main`  TM 
LEFT JOIN t_authorxref TAX ON TM.ID = TAX.ID
INNER JOIN t_userprofiles TU ON TAX.UserID = TU.UserID
WHERE TU.UserID = '$_SESSION[uid]' AND TM.Type ='T'
ORDER  BY  `Type` , `Name` ";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $numresults = mysql_num_rows($sql_result);
  while ($row = mysql_fetch_array($sql_result)) {
    $id = $row["ID"];
    $type = $row["Type"];
    $name = $row["Name"];
    $dateadded = $row["DateAdded"];
    $dateupdated = $row["DateUpdated"];
    $homepage = $row["Homepage"];
    $description = substr($row["Description"],0,75);
    $authors = $row["UserEmail"];
    $downloadcount = $row["downloadcount"];
    $totaldownloads = $row["TotalDownloads"];
    $rating = $row["Rating"];

echo"<h4><A HREF=\"itemoverview.php?id=$id\">$name</A></h4>\n";
echo"$description\n";
//Icon Bar
echo"<DIV style=\"margin-top: 10px; height: 34px\">";
echo"<DIV class=\"iconbar\"><A HREF=\"itemoverview.php?id=$id\"><IMG SRC=\"/images/edit.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">&nbsp;Edit Item</A></DIV>";
echo"<DIV class=\"iconbar\"><IMG SRC=\"/images/download.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">Downloads: $downloadcount<BR>&nbsp;&nbsp;$totaldownloads total</DIV>";
echo"<DIV class=\"iconbar\" title=\"$rating of 5 stars\"><A HREF=\"/themes/moreinfo.php?id=$id&page=comments\"><IMG SRC=\"/images/ratings.png\" BORDER=0 HEIGHT=34 WIDTH=34 ALT=\"\">Rated<br>&nbsp;&nbsp;$rating of 5</A></DIV>";
echo"</DIV>";


}
?>
&nbsp;&nbsp;&nbsp;&nbsp;<a href="additem.php?type=T">Add New Theme...</a>

</P>
</DIV>
<?php
include"$page_footer";
?>
</BODY>
</HTML>