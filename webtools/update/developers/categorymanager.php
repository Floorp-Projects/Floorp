<?php
require"core/sessionconfig.php";
require"../core/config.php";
$function = $_GET["function"];
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<TITLE>Mozilla Update :: Developer Control Panel :: Category Manager</TITLE>
<?php
include"$page_header";
include"inc_sidebar.php";
?>
<?php
if ($_SESSION["level"]=="admin") {
    //Do Nothing, they're good. :-)
} else {
    echo"<h1>Access Denied</h1>\n";
    echo"You do not have access to the Category Manager";
    include"$page_footer";
    echo"</body></html>\n";
    exit;
}
?>
<?php
if (!$function) {
?>
<?php
if ($_POST["submit"]=="Create Category") {
  if ($_POST[cattype]=="other") {$_POST["cattype"]=$_POST["othertype"];}
    $catname = $_POST["catname"];
    $catdesc = $_POST["catdesc"];
    $cattype = $_POST["cattype"];
    $catapp = $_POST["catapp"];
   	$sql = "INSERT INTO `t_categories` (`CatName`, `CatDesc`, `CatType`, `CatApp`) VALUES ('$catname', '$catdesc', '$cattype', '$catapp');";
    $result = mysql_query($sql) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
}
?>
<h1>Manage Category List</h1>
<SPAN style="font-size:8pt">&nbsp;&nbsp;&nbsp;&nbsp; Show Categories for Application: <?php $i=0;
$sql = "SELECT `AppName` from `t_applications` GROUP BY `AppName` ORDER BY `AppName` ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
 $num_results = mysql_num_rows($sql_result);
   while ($row = mysql_fetch_array($sql_result)) {
   $i++;
   $appname = $row["AppName"];
   echo"<a href=\"?application=".strtolower($appname)."\">$appname</a>";
   if ($num_results>$i) { echo" / "; }
   }

?></span>
<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: 0px; width: 100%">
<?php
$typenames = array("E"=>"Extensions", "T"=>"Themes","P"=>"Plugins");
 $sql1 = "SELECT `CatType`, `CatApp` FROM `t_categories` WHERE `CatApp`='$application' GROUP BY `CatType` ORDER BY `CatType`";
 $sql_result1 = mysql_query($sql1, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   while ($row = mysql_fetch_array($sql_result1)) {
    $type = ucwords($row["CatType"]);
    $application = strtolower($row["CatApp"]);
    $typename=$typenames["$type"];
?>
<tr>
<td colspan="4"><h2><?php echo"$typename Categories for ".ucwords($application); ?></h2></td>
</tr>
<tr>
<th></th>
<th>Name</th>
<th>Description</th>
</tr>

<?php
 $i=0;
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
    echo"</tr>\n";
}


}
?>
</table>
<h2>New Category for <?php echo ucwords($application); ?><BR></h2>
<div style="font-size: 10pt; font-weight: bold">
<form name="addapplication" method="post" action="?function=&action=addnewcategory">
<input name="catapp" type="hidden" value="<?php echo strtolower($application); ?>">
Name: <input name="catname" type="text" value="" size="30" maxlength="100"><BR>
Description: <input name="catdesc" type="text" value="" size="50" maxlength="100"><BR>

<SPAN style="margin-left: 20px">Type: <select name="cattype">";
<?php
 $sql = "SELECT `CatType` FROM `t_categories` GROUP BY `CatType` ORDER BY `CatType`";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   while ($row = mysql_fetch_array($sql_result)) {
    $type = ucwords($row["CatType"]);
    $typename = $typenames[$type];
    echo"<option value=\"$type\">$typename</option>\n";
  }
    echo"<option value=\"other\">Other</option>\n";
?>
  </select>
If other, Type: <INPUT NAME="othertype" TYPE="TEXT" SIZE=3 MAXLENGTH=1>
<input name="submit" type="submit" value="Create Category"></SPAN>
</form>
</div>

<?php
} else if ($function=="editcategory") {
$categoryid = $_GET["categoryid"];
//Post Functions
if ($_POST["submit"] == "Update") {
  echo"<h2>Processing Update, please wait...</h2>\n";
  $categoryid = $_POST["categoryid"];
  $catname = $_POST["catname"];
  $catdesc = $_POST["catdesc"];
  $cattype = $_POST["cattype"];
    $sql = "UPDATE `t_categories` SET `CatName`='$catname', `CatDesc`='$catdesc', `CatType`='$cattype' WHERE `CategoryID`='$categoryid'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

  echo"Your update to $catname, has been submitted successfully...<br>";

} else if ($_POST["submit"] == "Delete Category") {
  echo"<h2>Processing Delete Request, please wait...</h2>\n";
  $categoryid = $_POST["categoryid"];
  $sql = "DELETE FROM `t_categories` WHERE `CategoryID`='$categoryid'";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

  echo"You've successfully deleted the category '$catname'...<br>";
}

if (!$categoryid) { $categoryid = $_POST["categoryid"]; }
// Show Edit Form
  $sql = "SELECT * FROM `t_categories` WHERE `CategoryID` = '$categoryid' LIMIT 1";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
  $categoryid = $row["CategoryID"];
  $catapp = ucwords($row["CatApp"]);
  $catname = $row["CatName"];
  $catdesc = $row["CatDesc"];
  $cattype = $row["CatType"];
?>

<div class="editbox">
    <h3>Edit Category <?php echo"$catname for ".ucwords($catapp); ?>:</h3>
<form name="editcategory" method="post" action="?function=editcategory">
<?php
  echo"Name:  <input name=\"catname\" type=\"text\" value=\"$catname\" size=\"30\" maxlength=\"100\"><br>\n";
  echo"Description:  <input name=\"catdesc\" type=\"text\" value=\"$catdesc\" size=\"50\" maxlength=\"100\"><br>\n";
  echo"Type: <input name=\"cattype\" type=\"text\" value=\"$cattype\" size=\"1\" maxlength=\"1\"><br>\n";
  echo"<input name=\"categoryid\" type=\"hidden\" value=\"$categoryid\">\n";
?>
<input name="submit" type="submit" value="Update">
<input name="reset"  type="reset"  value="Reset Form">
<input name="submit" type="submit" value="Delete Category" onclick="return confirm('Are you sure you want to delete the category <?php echo"$catname"; ?> for <?php echo ucwords($catapp); ?>?');">
</form><br>
<A HREF="?function=">&#171;&#171; Return to Category Manager</A>
</div>
<?php
} else {}
?>

<?php
include"$page_footer";
?>
</BODY>
</HTML>