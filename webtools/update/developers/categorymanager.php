<?php
require"core/sessionconfig.php";
$function = $_GET["function"];
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<html lang="EN" dir="ltr">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
    <meta http-equiv="Content-Language" content="en">
    <meta http-equiv="Content-Style-Type" content="text/css">
<?php
require"../core/config.php";

//Define Breadcrumbs for Header Navigation
$breadcrumbs[]['name'] = "Manage Extension Categories";
$breadcrumbs[]['url']  = "/admin/extcategories.php";
?>
<link rel="stylesheet" type="text/css" href="/admin/core/mozupdates.css">
<title>Mozilla Update :: Category Manager</title>
</head>
<body>

<?php
include"$page_header";
?>

<?php
if (!$function) {
?>
<?php
if ($_POST["submit"]=="Create Category") {
  if ($_POST[cattype]=="other") {$_POST["cattype"]=$_POST["othertype"];}
   	$sql = "INSERT INTO `t_categories` (`CatName`, `CatDesc`, `CatType`) VALUES ('$_POST[catname]', '$_POST[catdesc]', '$_POST[cattype]');";
    $result = mysql_query($sql) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
}
?>
<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: solid 1px #000000; width: 95%" class="listing">
<TR><TD COLSPAN=4 STYLE="background-color: #FFFFFF; font-size:14pt; font-weight: bold;">Manage Category List:&nbsp;&nbsp;<SPAN style="font-size: 10pt">Show: <?php
 $sql = "SELECT `CatType` FROM `t_categories` GROUP BY `CatType` ORDER BY `CatType`";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   while ($row = mysql_fetch_array($sql_result)) {
    $type = ucwords($row["CatType"]);
    echo"<a href=\"?function=&type=$type\">";
if ($types[$type]) {$type = $types[$type]; }
    echo"$type";
    echo"</A> / \n";
  }
    echo"<a href=\"?function=&type=%\">All</A>\n";
?></SPAN></TD></TR>
<tr>
<td class="hline" colspan="4"></td>
</tr>
<tr>
<th></th>
<th>Name</th>
<th>Description</th>
<th>Type</th>
</tr>

<?php
$type = $_GET["type"];
if (!$type) {$type="%"; }
 $sql = "SELECT * FROM `t_categories` WHERE `CatType` LIKE '$type' ORDER BY `CatType`,`CatName`";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   while ($row = mysql_fetch_array($sql_result)) {
    $categoryid = $row["CategoryID"];
    $catname = $row["CatName"];
    $catdesc = $row["CatDesc"];
    $cattype = $row["CatType"];

    $i++;
    echo"<tr>\n";
    echo"<td>$i</td>\n";
    echo"<td><a href=\"?function=editcategory&categoryid=$categoryid\">$catname</a></td>\n";
    echo"<td>$catdesc</a></td>\n";
    echo"<td>$cattype</td>\n";
    echo"</tr>\n";
}

?>
</table>
<div style="width: 640px; border: 1px dotted #AAA; margin-top: 2px; margin-left: 50px; font-size: 10pt; font-weight: bold">
<?php
if ($_POST["submit"]=="Create Category") {
//Disabled Here, See Above ^
//  if ($_POST[cattype]=="other") {$_POST["cattype"]=$_POST["othertype"];}
//   	$sql = "INSERT INTO `t_categories` (`CatName`, `CatDesc`, `CatType`) VALUES ('$_POST[catname]', '$_POST[catdesc]', '$_POST[cattype]');";
//    $result = mysql_query($sql) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

    echo"<div class=\"success\">The category $_POST[catname] has been Successfully Added...</div>";
}
?>
<form name="addapplication" method="post" action="?function=&action=addnewcategory">
<SPAN style="color: #00F">New Category</SPAN>

Name: <input name="catname" type="text" value="" size="30" maxlength="100">
Description: <input name="catdesc" type="text" value="" size="30" maxlength="100"><BR>

<SPAN style="margin-left: 20px">Type: <select name="cattype">";
<?php
 $sql = "SELECT `CatType` FROM `t_categories` GROUP BY `CatType` ORDER BY `CatType`";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   while ($row = mysql_fetch_array($sql_result)) {
    $type = ucwords($row["CatType"]);
    echo"<option value=\"$type\">$type</option>\n";
  }
    echo"<option value=\"other\">Other</option>\n";
?>
  </select>
If other, Type: <INPUT NAME="othertype" TYPE="TEXT" SIZE=5 MAXLENGTH=1>
<input name="submit" type="submit" value="Create Category"></SPAN>
</form>
</div>

<?php
} else if ($function=="editcategory") {
$categoryid = $_GET["categoryid"];
//Post Functions
if ($_POST["submit"] == "Update") {
  $sql = "UPDATE `t_categories` SET `CatName`='$_POST[catname]', `CatDesc`='$_POST[catdesc]', `CatType`='$_POST[cattype]' WHERE `CategoryID`='$_POST[categoryid]'";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

  echo"<div class=\"success\">Your update to $_POST[catname], has been submitted successfully...</div>";

} else if ($_POST["submit"] == "Delete Category") {
  $sql = "DELETE FROM `t_categories` WHERE `CategoryID`='$_POST[categoryid]'";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

  echo"<div class=\"success\">You've successfully deleted the category '$_POST[catname]'...</div>";
}

if (!$categoryid) { $categoryid = $_POST["categoryid"]; }
// Show Edit Form
  $sql = "SELECT * FROM `t_categories` WHERE `CategoryID` = '$categoryid' LIMIT 1";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

  $row = mysql_fetch_array($sql_result);
  $categoryid = $row["CategoryID"];
  $catname = $row["CatName"];
  $catdesc = $row["CatDesc"];
  $cattype = $row["CatType"];
?>

<div class="editbox">
    <h3>Edit Category:</h3>
<form name="editcategory" method="post" action="?function=editcategory">
<?php
  echo"Name:  <input name=\"catname\" type=\"text\" value=\"$catname\" size=\"30\" maxlength=\"100\"><br />\n";
  echo"Description:  <input name=\"catdesc\" type=\"text\" value=\"$catdesc\" size=\"30\" maxlength=\"100\"><br />\n";
  echo"Type: <input name=\"cattype\" type=\"text\" value=\"$cattype\" size=\"1\" maxlength=\"1\">\n";
  echo"<input name=\"categoryid\" type=\"hidden\" value=\"$categoryid\"><br />\n";
?>
<input name="submit" type="submit" value="Update">
<input name="reset"  type="reset"  value="Reset Form">
<input name="submit" type="submit" value="Delete Category" onclick="return confirm('Are you sure?');">
</form>
<A HREF="?function=">&#171;&#171; Return to Category Manager</A>
</div>
<?php
//} else if ($function=="addcategory") {

 //Add Category to MySQL Table
//if ($_POST["submit"]=="Create Category") {
//   	$sql = "INSERT INTO `t_categories` (`CatName`, `CatDesc`, `CatType`) VALUES ('$_POST[catname]', '$_POST[catdesc]', '$_POST[cattype]');";
//    $result = mysql_query($sql) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

//    echo"<div class=\"success\">The category $_POST[catname] has been Successfully Added...</div>";
//}
?>
<?php
//<div class="editbox">
//<h3>Add New Category:</h3>
//<form name="addcategory" method="post" action="?function=addcategory">
//Name:  <input name="catname" type="text" value="" size="30" maxlength="100"><br />
//Description:  <input name="catdesc" type="text" value="" size="30" maxlength="100"><br />
//Type: <input name="cattype" type="text" value="E" size="1" maxlength="1"><br />
//<input name="submit" type="submit" value="Create Category">
//<input name="reset" type="reset" value="Reset Form">
//</form>
?>
<?php
} else {}
?>

<?php
include"$page_footer";
?>
</BODY>
</HTML>