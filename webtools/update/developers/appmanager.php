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
$breadcrumbs[]['name'] = "Manage Application Versions";
$breadcrumbs[]['url']  = "/admin/appmanager.php";
?>
<link rel="stylesheet" type="text/css" href="/admin/core/mozupdates.css">
<title>MozUpdates :: Manage Application Versions</title>
</head>

<body>
<?php
include"$page_header";
?>

<?php
if (!$function) {

//Add Category to MySQL Table
  if ($_POST["submit"]=="Add Application") {
     if ($_POST[AppName] != "Mozilla" && $_POST[SubVer] == "final") {
       $_POST[SubVer] = "";
     }
     if ($_POST["AppName"]=="Other") { 
       $_POST["AppName"] = $_POST["othername"]; 
       $_POST["GUID"] = $_POST["otherguid"]; 
     }
     $sql = "INSERT INTO `t_applications` (`AppName`,`Release`,`SubVer`,`GUID`) VALUES ('$_POST[AppName]','$_POST[Release]','$_POST[SubVer]','$_POST[GUID]')";
     $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
}
?>

<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: solid 1px #000000; width: 95%" class="listing">
<TR><TD COLSPAN=6 STYLE="background-color: #FFFFFF; font-size:14pt; font-weight: bold;">Manage Application List:</TD></TR>
<tr>
<td class="hline" colspan="6"></td>
</tr>
<TR>
<tr>
<th></th>
<th>Release</th>
<th>Version</th>
</tr>

<?php

  $sql = "SELECT * FROM `t_applications` ORDER BY `AppName` ASC, `major` ASC, `minor` ASC, `release` ASC, `SubVer` ASC";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

  $lastname = "";

  while ($row = mysql_fetch_array($sql_result)) {

    // If it's a new AppName, start a new row
    if ($lastname != $row["AppName"]) {

      // Close out previous row if necessary
      if ($lastname != "") {
        echo"</td>";
        echo"</tr>\n";
      }

      $applications[] = $row["AppName"];
      echo"<tr>\n";
      echo"<td>".++$i."</td>\n";
      echo"<td>&nbsp;".$row["AppName"]."&nbsp;&nbsp;</td>";
      echo"<td>";

    } else {

    // Separate with a comma

      echo", ";
    }

    echo"<a href=\"?function=editcategory&appid=".$row["AppID"]."\">";
    echo $row["Version"];
    echo"</a>";

    $lastname = $row["AppName"];
  }

  echo"</td>";
  echo"</tr>\n";

?>
</table>
<div style="width: 660px; border: 1px dotted #AAA; margin-top: 2px; margin-left: 50px; font-size: 10pt; font-weight: bold">
<?php
//Add Category to MySQL Table
  if ($_POST["submit"]=="Add Application") {

     echo"<DIV style=\"text-align: center; font-size: 12pt\">The application $_POST[AppName] (Version $_POST[Release]$_POST[SubVer]) has been successfully added.</DIV>";
  }
?>
<form name="addapplication" method="post" action="?function=&action=addnewapp">
New Application
  Name: <select name="AppName">
<?php
  foreach ($applications as $application) {
    echo"<OPTION value=\"$application\">$application</OPTION>\n";
  }
  echo"<OPTION value=\"Other\">Other</OPTION>\n";
?>
  </select>
Release: <input name="Release" type="text" size="5" maxlength="5" value=""><select name="SubVer">

<?php
  $subvers = array("+"=>"+", "a"=>"alpha","b"=>"beta","final"=>"final");

  foreach($subvers as $key => $subver) {
    $subver = ucwords($subver);
    echo"<option value=\"$key\">$subver</option>\n";
  }
?>
</select>
GUID: <input name="otherguid" type="text" size="25" MAXSIZE="50" value="">
<BR>
<SPAN style="margin-left: 20px">If other, Name: <input name="othername" type="text" size="10"></span>




<input name="submit" type="submit" value="Add Application">
</form>
</div>

<?php
} else if ($function=="editcategory") {

  $appid = $_GET["appid"];
  //Post Functions
  if ($_POST["submit"] == "Update") {
    if ($_POST[AppName] != "Mozilla" && $_POST[SubVer] == "final") {
      $_POST[SubVer] = "";
    }
    $sql = "UPDATE `t_applications` SET `AppName`='$_POST[AppName]', `major`='$_POST[Major]', `minor`='$_POST[Minor]', `release`='$_POST[Release]', `SubVer`='$_POST[SubVer]',`GUID`='$_POST[GUID]' WHERE `appid`='$_POST[appid]'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

    echo"<div class=\"success\">Your update to $_POST[AppName], has been successful.</div>";

  } else if ($_POST["submit"] == "Delete Application") {
    $sql = "DELETE FROM `t_applications` WHERE `appid`='$_POST[appid]'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

    echo"<div class=\"success\">You've successfully deleted the application '$_POST[AppName]'.</div>";
}

if (!$appid) { $appid = $_POST["appid"]; }
// Show Edit Form
  $sql = "SELECT * FROM `t_applications` WHERE `appid` = '$appid' LIMIT 1";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

  $row = mysql_fetch_array($sql_result);
?>

<div class="editbox">
<h3>Edit Application:</h3>
<form name="editcategory" method="post" action="?function=editcategory">
<?php
  echo"Name:  <input name=\"AppName\" type=\"text\" size=\"30\" maxlength=\"30\" value=\"".$row["AppName"]."\" /><br />\n";
  echo"Major:  <input name=\"Major\" type=\"text\" size=\"5\" maxlength=\"5\" value=\"".$row["major"]."\" />";
  echo"Minor:  <input name=\"Minor\" type=\"text\" size=\"5\" maxlength=\"5\" value=\"".$row["minor"]."\" />";
  echo"Release:  <input name=\"Release\" type=\"text\" size=\"5\" maxlength=\"5\" value=\"".$row["release"]."\" />";
  echo"  <select name=\"SubVer\"/><br />\n";

  $subvers = array(""=>"", "+"=>"+", "a"=>"alpha", "b"=>"beta", "final"=>"final");

  foreach($subvers as $key => $subver) {
    $subver = ucwords($subver);
    echo"<option value=\"$key\"";
    if ($key==$row["SubVer"]) { echo" selected=\"selected\""; }
    echo">$subver</option>\n";
  }
  echo"</select><br />\n";

  echo"GUID:  <input name=\"GUID\" type=\"text\" size=\"50\" maxlength=\"50\" value=\"".$row["GUID"]."\" /><br />\n";

  echo"<input name=\"appid\" type=\"hidden\" value=\"".$row["AppID"]."\" />\n";
?>

<input name="submit" type="submit" value="Update" />
<input name="reset"  type="reset"  value="Reset Form" />
<input name="submit" type="submit" value="Delete Application" onclick="return confirm('Are you sure?');" />
</form>
<A HREF="?function=">&#171;&#171; Return to Application Manager</A>
</div>

<?php
} 
?>

<?php
include"$page_footer";
?>
</body>
</html>