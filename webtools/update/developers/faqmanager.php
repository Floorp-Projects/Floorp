<?php
require"core/sessionconfig.php";
require"../core/config.php";
$function = $_GET["function"];
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<TITLE>Mozilla Update :: Developer Control Panel :: FAQ Manager</TITLE>
<?php
include"$page_header";
include"inc_sidebar.php";
?>
<?php
if ($_SESSION["level"]=="admin") {
    //Do Nothing, they're good. :-)
} else {
    echo"<h1>Access Denied</h1>\n";
    echo"You do not have access to the FAQ Manager";
    include"$page_footer";
    echo"</body></html>\n";
    exit;
}
?>

<?php
if (!$function) {
?>

<h1>Manage FAQs:</h1>
<TABLE CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: 0px; width: 100%">
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

<h2><a href="?function=addentry">New FAQ Entry</A></h2>
<form name="addapplication" method="post" action="?function=addentry">

  Title: <input name="title" type="text" size="30" maxlength="150" value="">
<input name="submit" type="submit" value="Next &#187;&#187;"></SPAN>
</form>
</div>

<?php
} else if ($function=="edit") {
  $id = $_GET["id"];
  //Post Functions
  if ($_POST["submit"] == "Update Entry") {
    echo"<h2>Processing your update, please wait...</h2>\n";
    $title = $_POST["title"];
    $index = $_POST["index"];
    $alias = $_POST["alias"];
    $text = $_POST["text"];
    $active = $_POST["active"];
    $id = $_POST["id"];
    $sql = "UPDATE `t_faq` SET `title`='$title', `index`='$index', `alias`='$alias', `text`='$text', `active`='$active' WHERE `id`='$id'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
    if ($sql_result) {
        echo"Your update to '$title', has been successful.<br>";
    }

  } else if ($_POST["submit"] == "Delete Entry") {
    echo"<h2>Processing, please wait...</h2>\n";
    $id = $_POST["id"];
    $title = $_POST["title"];
    $sql = "DELETE FROM `t_faq` WHERE `id`='$id'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
    if ($sql_result) {
        echo"You've successfully deleted the FAQ Entry '$title'.";
        include"$page_footer";
        echo"</body>\n</html>\n"; 
        exit;
    }
}

// Show Edit Form
  $sql = "SELECT * FROM `t_faq` WHERE `id` = '$id' LIMIT 1";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
?>

<h3>Edit FAQ Entry:</h3>
<form name="editfaq" method="post" action="?function=edit">
<?php
  echo"<input name=\"id\" type=\"hidden\" value=\"".$row["id"]."\" />\n";
  echo"Title: <input name=\"title\" type=\"text\" size=\"40\" maxlength=\"150\" value=\"".$row["title"]."\"> ";
  echo"Alias: <input name=\"alias\" type=\"text\" size=\"8\" maxlength=\"20\" value=\"".$row["alias"]."\"><br>\n";

//List of Entry Index for User Convienience
 echo"Existing Index Reference: <SELECT name=\"titleindex\">\n";
 $sql = "SELECT `id`,`title`, `index` FROM `t_faq` ORDER BY `index` ASC";
  $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while ($row2 = mysql_fetch_array($sql_result)) {
    echo"<OPTION value=\"$row2[index]\"";
    if ($row2[id]==$id) {echo" SELECTED";}
    echo">$row2[title] (Index: $row2[index])</OPTION>\n";
    }
 echo"</SELECT><BR>\n";

  echo"Index: <input name=\"index\" type=\"text\" size=\"5\" maxlength=\"5\" value=\"".$row["index"]."\"><BR>\n";
  echo"<br>\n";

  echo"Entry Text:<BR><TEXTAREA NAME=\"text\" ROWS=10 COLS=60>$row[text]</TEXTAREA><BR>";
$active = $row["active"];
echo"Show Entry on FAQ Page: ";
if ($active=="YES") {
 echo"Yes: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"YES\" CHECKED> No: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"NO\">";
 } else if ($active=="NO") {
 echo"Yes: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"YES\"> No: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"NO\" CHECKED>";
 } else {
 echo"Yes: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"YES\"> No: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"NO\">";
 }
?>
<BR><BR>
<input name="submit" type="submit" value="Update Entry">
<input name="reset"  type="reset"  value="Reset Form">
<input name="submit" type="submit" value="Delete Entry" onclick="return confirm('Are you sure you want to delete <?php echo $row["title"]; ?>?');" />
</form>
<BR><BR>
<A HREF="?function=">&#171;&#171; Return to FAQ Manager</A>

<?php
} else if ($function=="addentry") {

 //Add Category to MySQL Table
  if ($_POST["submit"]=="Add FAQ Entry") {

    echo"<h2>Adding Entry, please wait...</h2>\n";
    $title = $_POST["title"];
    $index = $_POST["index"];
    $alias = $_POST["alias"];
    $text = $_POST["text"];
    $active = $_POST["active"];
    $id = $_POST["id"];
     $sql = "INSERT INTO `t_faq` (`title`,`index`,`alias`, `text`, `active`) VALUES ('$title','$index','$alias', '$text', '$active')";
     $sql_result = mysql_query($sql, $connection) or trigger_error("<div class=\"error\">MySQL Error ".mysql_errno().": ".mysql_error()."</div>", E_USER_NOTICE);
     if ($sql_result) {
        echo"The entry '$title' has been successfully added.<br>\n";
     }
  }
?>

<h2>Add FAQ Entry:</h2>
<form name="addfaq" method="post" action="?function=addentry">
<?php
$title = $_POST["title"];

  echo"Title: <input name=\"title\" type=\"text\" size=\"40\" maxlength=\"150\" value=\"$title\">&nbsp;\n";
  echo"Alias: <input name=\"alias\" type=\"text\" size=\"8\" maxlength=\"20\" value=\"\"><br>";

//List of Entry Index for User Convienience
 echo"<BR>Existing Index Reference: <SELECT name=\"titleindex\">\n";
 $sql = "SELECT `id`,`title`, `index` FROM `t_faq` ORDER BY `index` ASC";
  $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    while ($row2 = mysql_fetch_array($sql_result)) {
    echo"<OPTION value=\"$row2[index]\"";
    if ($row2[id]==$id) {echo" SELECTED";}
    echo">$row2[title] (Index: $row2[index])</OPTION>\n";
    }
 echo"</SELECT><BR>\n";
  echo"Index: <input name=\"index\" type=\"text\" size=\"5\" maxlength=\"5\" value=\"\"> (used for FAQ page sort order)<br><br>\n";

  echo"Entry Text:<BR><TEXTAREA NAME=\"text\" ROWS=10 COLS=60></TEXTAREA><BR>";
  echo"Show Entry on FAQ Page: ";
  echo"Yes: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"YES\" CHECKED>/ No: <INPUT NAME=\"active\" TYPE=\"RADIO\" VALUE=\"NO\">";
?>
<BR><BR>

<input name="submit" type="submit" value="Add FAQ Entry" />
<input name="reset"  type="reset"  value="Reset Form" />
</form>
<BR><BR>
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