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
?>

<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: solid 1px #000000; width: 95%" class="listing">
<TR><TD COLSPAN=5 STYLE="background-color: #FFFFFF; font-size:14pt; font-weight: bold;">Manage FAQs:</TD></TR>
<tr>
<td class="hline" colspan="5"></td>
</tr>
<TR>
<tr>
<th></th>
<th>FAQ Entry</th>
<th>Updated</th>
<th>Active</th>
</tr>

<?php
  $sql = "SELECT * FROM `t_faq` ORDER BY `index` ASC, `title` ASC";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
  while ($row = mysql_fetch_array($sql_result)) {

//Create Customizeable Timestamp
   $date = $row["lastupdated"];
	$day=substr($date,6,2);  //get the day
    $month=substr($date,4,2); //get the month
    $year=substr($date,0,4); //get the year
    $hour=substr($date,8,2); //get the hour
    $minute=substr($date,10,2); //get the minute
    $second=substr($date,12,2); //get the sec
    $timestamp = strtotime("$year-$month-$day $hour:$minute:$second");
    $lastupdated = gmdate("F d, Y g:i:sa", $timestamp);

  $applications[] = $row["AppName"];
    echo"<tr>\n";
    echo"<td>".++$i."</td>\n";
    echo"<td>&nbsp;<a href=\"?function=edit&id=".$row["id"]."\">".$row["title"]."</a></td>\n";
    echo"<td>$lastupdated</td>\n";
    echo"<td>$row[active]</td>\n";
    echo"</tr>\n";

}
?>
</table>
<div style="width: 580px; border: 1px dotted #AAA; margin-top: 2px; margin-left: 50px; font-size: 10pt; font-weight: bold">

<form name="addapplication" method="post" action="?function=addentry">
<a href="?function=addentry">New FAQ Entry</A>
  Title: <input name="title" type="text" size="20" maxlength="150" value="">
<input name="submit" type="submit" value="Add Entry"></SPAN>
</form>
</div>

<?php
} else if ($function=="edit") {

  $id = $_GET["id"];
  //Post Functions
  if ($_POST["submit"] == "Update Entry") {
    $sql = "UPDATE `t_faq` SET `title`='$_POST[title]', `index`='$_POST[index]', `alias`='$_POST[alias]', `text`='$_POST[text]', `active`='$_POST[active]' WHERE `id`='$_POST[id]'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);


    echo"<div class=\"success\">Your update to $_POST[title], has been successful.</div>";

  } else if ($_POST["submit"] == "Delete Entry") {
    $sql = "DELETE FROM `t_faq` WHERE `id`='$_POST[id]'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);

    echo"<div class=\"success\">You've successfully deleted the application '$_POST[AppName]'.</div>";
}

if (!$id) { $id = $_POST["id"]; }
// Show Edit Form
  $sql = "SELECT * FROM `t_faq` WHERE `id` = '$id' LIMIT 1";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
?>

<div class="editbox">
<h3>Edit FAQ Entry:</h3>
<form name="editfaq" method="post" action="?function=edit">
<?php
  echo"<input name=\"id\" type=\"hidden\" value=\"".$row["id"]."\" />\n";
  echo"Title: <input name=\"title\" type=\"text\" size=\"40\" maxlength=\"150\" value=\"".$row["title"]."\"><br>\n";
  echo"Index: <input name=\"index\" type=\"text\" size=\"5\" maxlength=\"5\" value=\"".$row["index"]."\">&nbsp;&nbsp;";
  echo"Alias: <input name=\"alias\" type=\"text\" size=\"5\" maxlength=\"5\" value=\"".$row["alias"]."\"><br>";
  echo"Entry Text:<BR><TEXTAREA NAME=\"text\" ROWS=10 COLS=60>$row[text]</TEXTAREA>";
$active = $row["active"];
if ($active=="YES") {
 echo"Yes: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"YES\" CHECKED>/ No: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"NO\">";
 } else if ($active=="NO") {
 echo"Yes: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"YES\">/ No: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"NO\" CHECKED>";
 } else {
 echo"Yes: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"YES\">/ No: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"NO\">";
 }
?>
<BR>
<input name="submit" type="submit" value="Update Entry" />
<input name="reset"  type="reset"  value="Reset Form" />
<input name="submit" type="submit" value="Delete Entry" onclick="return confirm('Are you sure?');" />
</form>
<A HREF="?function=">&#171;&#171; Return to FAQ Manager</A>
</div>

<?php
} else if ($function=="addentry") {

 //Add Category to MySQL Table
  if ($_POST["submit"]=="Add FAQ Entry") {

     $sql = "INSERT INTO `t_faq` (`title`,`index`,`alias`, `text`, `active`) VALUES ('$_POST[title]','$_POST[index]','$_POST[alias]', '$_POST[text]', '$_POST[active]')";
     $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);


     echo"<div class=\"success\">The entry $_POST[title] has been successfully added.</div>";
  }
?>

<div class="editbox">
<h3>Add FAQ Entry:</h3>
<form name="addfaq" method="post" action="?function=addentry">
<?php
  echo"Title: <input name=\"title\" type=\"text\" size=\"40\" maxlength=\"150\" value=\"$_POST[title]\"><br>\n";
  echo"Index: <input name=\"index\" type=\"text\" size=\"5\" maxlength=\"5\" value=\"\">&nbsp;&nbsp;";
  echo"Alias: <input name=\"alias\" type=\"text\" size=\"5\" maxlength=\"5\" value=\"\"><br>";
  echo"Entry Text:<BR><TEXTAREA NAME=\"text\" ROWS=10 COLS=60></TEXTAREA>";
  echo"Yes: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"YES\" CHECKED>/ No: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"NO\">";
?>
<BR>

<input name="submit" type="submit" value="Add FAQ Entry" />
<input name="reset"  type="reset"  value="Reset Form" />
</form>
<A HREF="?function=">&#171;&#171; Return to FAQ Manager</A>
</div>

<?php
} else {}
?>

<?php
include"$page_footer";
?>
</body>
</html>