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
$breadcrumbs[]['name'] = "Manage Operating Systems";
$breadcrumbs[]['url']  = "/admin/oscategories.php";
?>
<link rel="stylesheet" type="text/css" href="/admin/core/mozupdates.css">
<title>MozUpdates :: Manage Operating Systems</title>
</head>

<body>
<?php
include"$page_header";
?>

<?php
if (!$function) {
?>
<?php
 //Add Category to MySQL Table
  if ($_POST["submit"]=="Create OS") {
     $sql = "INSERT INTO `t_os` (`OSname`) VALUES ('$_POST[osname]')";
     $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
  }
?>
<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: solid 1px #000000; width: 95%" class="listing">
<TR><TD COLSPAN=2 STYLE="background-color: #FFFFFF; font-size:14pt; font-weight: bold;">Manage OS List:</TD></TR>
<tr>
<td class="hline" colspan="2"></td>
</tr>
<tr>
<th></th>
<th>Name</th>
</tr>

<?php
  $sql = "SELECT * FROM `t_os` ORDER BY `OSName` ASC";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);


  while ($row = mysql_fetch_array($sql_result)) {
    echo"<tr>\n";
    echo"<td>".++$i."</td>\n";
    echo"<td><a href=\"?function=editcategory&osid=".$row["OSID"]."\">".$row["OSName"]."</a></td>\n";
    echo"</tr>\n";
  }
?>

</table>
<div style="width: 580px; border: 1px dotted #AAA; margin-top: 2px; margin-left: 50px; font-size: 10pt; font-weight: bold">
<?php
 //Add Category to MySQL Table
  if ($_POST["submit"]=="Create OS") {
//Disabled Here, See Above...
//     $sql = "INSERT INTO `t_os` (`OSname`) VALUES ('$_POST[osname]')";
//     $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

     echo"<div class=\"success\">The OS $_POST[osname] has been successfully added.</div>";
  }
?>
<form name="addcategory" method="post" action="?function=">
<SPAN style="color: #00F">Add New OS</SPAN>:

Name:  <input name="osname" type="text" size="30" maxlength="100" value="">
<input name="submit" type="submit" value="Create OS">
</form>
</div>
<?php
} else if ($function=="editcategory") {

  $osid = $_GET["osid"];
  //Post Functions
  if ($_POST["submit"] == "Update") {
    $sql = "UPDATE `t_os` SET `OSName`='$_POST[osname]' WHERE `OSID`='$_POST[osid]'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);


    echo"<div class=\"success\">Your update to $_POST[osname], has been successful.</div>";

  } else if ($_POST["submit"] == "Delete OS") {
    $sql = "DELETE FROM `t_os` WHERE `OSID`='$_POST[osid]'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

    echo"<div class=\"success\">You've successfully deleted the operating system '$_POST[osname]'.</div>";
}

if (!$osid) { $osid = $_POST["osid"]; }
// Show Edit Form
  $sql = "SELECT * FROM `t_os` WHERE `OSID` = '$osid' LIMIT 1";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);


  $row = mysql_fetch_array($sql_result);
?>

<div class="editbox">
<h3>Edit OS:</h3>
<form name="editcategory" method="post" action="?function=editcategory">
<?php
  echo"Name:  <input name=\"osname\" type=\"text\" size=\"30\" maxlength=\"100\" value=\"".$row["OSName"]."\">\n";
  echo"<input name=\"osid\" type=\"hidden\" value=\"".$row["OSID"]."\"><br />\n";
?>

<input name="submit" type="submit" value="Update" />
<input name="reset"  type="reset"  value="Reset Form" />
<input name="submit" type="submit" value="Delete OS" onclick="return confirm('Are you sure?');" />
</form>
<A HREF="?function=">&#171;&#171; Return to OS Manager</A>
</div>

<?php
//} else if ($function=="addcategory") {

 //Add Category to MySQL Table
//  if ($_POST["submit"]=="Create OS") {
//     $sql = "INSERT INTO `t_os` (`OSname`) VALUES ('$_POST[osname]')";
//     $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
//

//     echo"<div class=\"success\">The OS $_POST[osname] has been successfully added.</div>";
//  }
?>
<?php
//<div class="editbox">
//<h3>Add New Operating System:</h3>
//<form name="addcategory" method="post" action="?function=addcategory">
//Name:  <input name="osname" type="text" size="30" maxlength="100" value=""><br />
//<input name="submit" type="submit" value="Create OS" />
//<input name="reset"  type="reset"  value="Reset Form" />
//</form>
//</div>
?>
<?php
} else {}
?>

<?php
include"$page_footer";
?>
</body>
</html>