<?php
require"core/sessionconfig.php";
$function = $_GET["function"];

//Access Level: "user" code, to keep user from altering other profiles but their own.
if ($_SESSION["level"] !=="admin") {
//Kill access to add user.
if ($function=="adduser" or $function=="postnewuser") {unset($function);}

if (!$function) { $function="edituser"; }
$userid=$_SESSION["uid"];
}

?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<?php
require"../core/config.php";

//Define Breadcrumbs for Header Navigation
$breadcrumbs[]['name'] = "Manage Users";
$breadcrumbs[]['url']  = "/admin/users.php";

if ($function=="edituser") {
$breadcrumbs[]['name'] = "Edit User";
$breadcrumbs[]['url']  = "/admin/users.php?function=edituser&userid=$_GET[userid]";
if ($_POST[submit]=="Update") { $breadcrumbs[]['name'] = "Update User";
} else if ($_POST[submit]=="Delete User") { $breadcrumbs[]['name'] = "Delete User"; }

} else if ($function=="adduser") {
$breadcrumbs[]['name'] = "Add User";
$breadcrumbs[]['url']  = "/admin/users.php?function=adduser";

} else if ($function=="changepassword") {
$breadcrumbs[]['name'] = "Change Password";
$breadcrumbs[]['url']  = "/admin/users.php?function=changepassword&userid=$_GET[userid]";
}
?>
<LINK REL="STYLESHEET" TYPE="text/css" HREF="/admin/core/mozupdates.css">
<TITLE>MozUpdates :: Manage Users</TITLE>
</HEAD>
<BODY>
<?php
include"$page_header";
?>

<?php
if (!$function) {
?>

<?php
if ($_POST["submit"] && $_GET["action"]=="update") {
?>
<div style="width: 80%; border: 1px dotted #AAA; margin: auto; margin-bottom: 10px; font-size: 10pt; font-weight: bold">
<?php

//Process Post Data, Make Changes to User Table.
//Begin General Updating
for ($i=1; $i<=$_POST[maxuserid]; $i++) {
$admin = $_POST["admin$i"];
$editor = $_POST["editor$i"];
$trusted = $_POST["trusted$i"];
$disabled = $_POST["disabled$i"];
//echo"$i - $admin - $editor - $trusted<br>\n";

if ($admin=="TRUE") { $mode="A"; 
} else if ($editor=="TRUE") { $mode="E"; 
} else if ($disabled=="TRUE") {$mode="D";
} else { $mode="U"; }

if ($trusted !=="TRUE") {$trusted="FALSE"; }

$sql = "UPDATE `t_userprofiles` SET `UserMode`= '$mode', `UserTrusted`= '$trusted' WHERE `UserID`='$i'";
 $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);

}
unset($i);

echo"<SPAN STYLE=\"font-size:14pt\">Your changes to the User List have been succesfully completed</SPAN><BR>";

//Do Special Disable, Delete, Enable Account Operations
if ($_POST["selected"] AND $_POST["submit"] !=="Update") {
//$selected = $_POST["selected"];

if ($_POST["submit"]=="Disable Selected") {
$sql = "UPDATE `t_userprofiles` SET `UserMode`= 'D' WHERE `UserID`='$_POST[selected]'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
echo"User Account for User Number $_POST[selected] Disabled<br>\n";

} else if ($_POST["submit"]=="Delete Selected") {
$sql = "DELETE FROM `t_userprofiles` WHERE `UserID`='$_POST[selected]' LIMIT 1";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
echo"User Account for User Number $_POST[selected] Deleted<br>\n";

} else if ($_POST["submit"]=="Enable Selected") {
$sql = "UPDATE `t_userprofiles` SET `UserMode`= 'U' WHERE `UserID`='$_POST[selected]'";
  $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
echo"User Account for User Number $_POST[selected] Enabled, User Mode set to User<br>\n";
}

}

echo"</DIV>\n";
}
?>
<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: solid 1px #000000; width: 95%" class="listing">
<TR><TD COLSPAN=5 STYLE="background-color: #FFFFFF; font-size:14pt; font-weight: bold;">Manage User List:</TD></TR>
<tr>
<td class="hline" colspan="5"></td>
</tr>
<TR>
<TH></TH>
<TH><B>Name</B></TH>
<TH><B>E-Mail Address</B></TH>
<TH><B>S  E  A  T</B></TH>
</TR>
<FORM NAME="updateusers" METHOD="POST" ACTION="?function=&action=update">
<?php
 $sql = "SELECT * FROM `t_userprofiles` ORDER BY `UserMode`, `UserName` ASC";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   while ($row = mysql_fetch_array($sql_result)) {
    $userid = $row["UserID"];
    $username = $row["UserName"];
    $useremail = $row["UserEmail"];
    $userwebsite = $row["UserWebsite"];
    $usermode = $row["UserMode"];
    $useremailhide = $row["UserEmailHide"];
    $t = $row["UserTrusted"];
    if ($userid>$maxuserid) {$maxuserid =$userid;}

if ($usermode=="A") {$a="TRUE"; $e="TRUE";
} else if ($usermode=="E") {$e="TRUE"; $a="FALSE";
} else if ($usermode=="U") {$e="FALSE"; $a="FALSE";
} else if ($usermode=="D") {$d="TRUE";}
    $i++;
    echo"<TR>";
    echo"<TD CLASS=\"tablehighlight\" ALIGN=CENTER><B>$i</B></TD>\n";
    echo"<TD CLASS=\"tablehighlight\"><B>&nbsp;&nbsp;<A HREF=\"?function=edituser&userid=$userid\">$username</A></B></TD>\n";
    echo"<TD CLASS=\"tablehighlight\"><B>&nbsp;&nbsp;<A HREF=\"mailto:$useremail\">$useremail</A></B></TD>\n";
    echo"<TD CLASS=\"tablehighlight\"><INPUT NAME=\"selected\" TYPE=\"RADIO\" VALUE=\"$userid\" TITLE=\"Selected User\">";
    echo"<INPUT NAME=\"editor$userid\" TYPE=\"CHECKBOX\" VALUE=\"TRUE\" "; if ($e=="TRUE") {echo"CHECKED=\"CHECKED\""; } if ($a=="TRUE" or $d=="TRUE" ) {echo" DISABLED=\"DISABLED\"";} echo" TITLE=\"Editor\">";
    echo"<INPUT NAME=\"admin$userid\" TYPE=\"CHECKBOX\" VALUE=\"TRUE\" "; if ($a=="TRUE") {echo"CHECKED=\"CHECKED\""; } if ($d=="TRUE" ) {echo" DISABLED=\"DISABLED\"";} echo" TITLE=\"Administrator\">";
    echo"<INPUT NAME=\"trusted$userid\" TYPE=\"CHECKBOX\" VALUE=\"TRUE\" "; if ($t=="TRUE") {echo"CHECKED=\"CHECKED\""; } if ($d=="TRUE" ) {echo" DISABLED=\"DISABLED\"";}echo" TITLE=\"Trusted User\">";
    if ($d=="TRUE") {echo"<INPUT NAME=\"disabled$userid\" TYPE=\"HIDDEN\" VALUE=\"TRUE\">\n"; }
    echo"</TD>\n";
    echo"</TR>\n";

  unset($a,$e,$t);
}

echo"<INPUT NAME=\"maxuserid\" TYPE=\"HIDDEN\" VALUE=\"$maxuserid\">";

?>
<TR><TD COLSPAN=3 ALIGN=CENTER>
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Disable Selected" ONCLICK="return confirm('Disabling this account will hide all their extensions and themes from view and prevent them from logging in. Do you want to procede and disable this account?');">
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Enable Selected">
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Delete Selected" ONCLICK="return confirm('Deleting this account will permanently remove all their extensions and account information from Mozilla Update. This cannot be undone. Do you want to procede and delete this account?');">
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Update">
</TD>
<TD><INPUT NAME="selected" TYPE="RADIO" VALUE="" CHECKED TITLE="Selected User"></TD>
</TR>
</FORM>
</TABLE>
<div style="width: 580px; border: 1px dotted #AAA; margin-top: 2px; margin-left: 50px; font-size: 10pt; font-weight: bold">

<form name="adduser" method="post" action="?function=adduser">
<a href="?function=adduser">New User</A>
  E-Mail: <input name="email" type="text" size="20" maxlength="150" value="">
<input name="submit" type="submit" value="Add User"></SPAN>
</form>
</div>

<?php
} else if ($function=="edituser") {
if (!$userid) {$userid = $_GET["userid"];}

//Process Submitted Values if this is a return with $_POST data...
if ($_POST["submit"]=="Update") {
if ($_SESSION["level"] !=="admin" && $_SESSION["uid"] !== $_POST["userid"]) {$_POST["userid"]=$_SESSION["uid"];}
  $_POST["username"] = htmlspecialchars($_POST["username"]);

$admin = $_POST["admin"];
$editor = $_POST["editor"];
$trusted = $_POST["trusted"];
$usermode = $_POST["usermode"];

if ($admin=="TRUE") { $mode="A"; 
} else if ($editor=="TRUE") { $mode="E"; 
} else if ($disabled=="TRUE") {$mode="D";
} else { $mode="U"; }

if (!$admin AND $usermode=="A") {$mode="A";} //Ensure possible mode change during session doesn't wipe out changes.
if ($usermode=="D") {$mode="D"; $trusted="FALSE";}

if ($trusted !=="TRUE") {$trusted="FALSE"; }

  $sql = "UPDATE `t_userprofiles` SET `UserName`= '$_POST[username]', `UserEmail`='$_POST[useremail]', `UserWebsite`='$_POST[userwebsite]', `UserMode`='$mode', `UserTrusted`='$trusted', `UserEmailHide`='$_POST[useremailhide]' WHERE `UserID`='$_POST[userid]'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
    echo"<DIV style=\"width: 550px; font-size: 14pt; font-weight: bold; text-align:center; margin: auto\">Your update to $_POST[username], has been submitted successfully...</DIV>";

} else if ($_POST["submit"] == "Delete User") {
if ($_SESSION["level"] !=="admin" && $_SESSION["uid"] !== $_POST["userid"]) {$_POST["userid"]=$_SESSION["uid"];}
  $sql = "DELETE FROM `t_userprofiles` WHERE `UserID`='$_POST[userid]'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
    echo"<DIV style=\"width: 550px; font-size: 14pt; font-weight: bold; text-align:center; margin: auto\">You've successfully deleted $_POST[username]...</DIV>";
}

if (!$userid) {$userid=$_POST["userid"];}

//Show Edit Form
 $sql = "SELECT * FROM `t_userprofiles` WHERE `UserID` = '$userid' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $row = mysql_fetch_array($sql_result);
    $userid = $row["UserID"];
    $username = $row["UserName"];
    $useremail = $row["UserEmail"];
    $userwebsite = $row["UserWebsite"];
    $userpass = $row["UserPass"];
    $usermode = $row["UserMode"];
    $trusted = $row["UserTrusted"];
    $useremailhide = $row["UserEmailHide"];
?>

<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE="border: solid 1px #000000; width: 500px">
    <TR><TD COLSPAN=2 STYLE="font-size:14pt; font-weight: bold; border-bottom: solid 1px #000000">Edit Profile for <?php echo"$username"; ?>:</TD></TR>
<FORM NAME="edituser" METHOD="POST" ACTION="?function=edituser">
<?php
    echo"<INPUT NAME=\"userid\" TYPE=\"HIDDEN\" VALUE=\"$userid\">\n";
    echo"<TR><TD STYLE=\"width: 130px\"><B>Name:</B></TD><TD><INPUT NAME=\"username\" TYPE=\"TEXT\" VALUE=\"$username\" SIZE=30 MAXLENGTH=100></TD></TR>\n";
    echo"<TR><TD><B>E-Mail:</B></TD><TD><INPUT NAME=\"useremail\" TYPE=\"TEXT\" VALUE=\"$useremail\" SIZE=30 MAXLENGTH=100></TD></TR>\n";
    echo"<TR><TD><B>Website:</B></TD><TD><INPUT NAME=\"userwebsite\" TYPE=\"TEXT\" VALUE=\"$userwebsite\" SIZE=30 MAXLENGTH=100></TD></TR>\n";
    echo"<TR><TD><B>Password:</B></TD><TD><FONT STYLE=\"font-size:10pt; font-weight: bold\"><A HREF=\"?function=changepassword&userid=$userid\">Change Password</A></FONT></TD></TR>\n";

    echo"<TR><TD><B>Permissions:</B></TD><TD>";
if ($_SESSION["level"]=="user" or $_SESSION[level]=="editor") {
    if ($usermode=="U") {echo"User <INPUT NAME=\"user\" TYPE=\"HIDDEN\" VALUE=\"TRUE\">\n"; //To prevent being reset to null on submit.
     } else if ($usermode=="E") {
      echo"Editor <INPUT NAME=\"editor\" TYPE=\"HIDDEN\" VALUE=\"TRUE\">\n";
     } else {
      echo"Unknown <INPUT NAME=\"usermode\" TYPE=\"HIDDEN\" VALUE=\"$usermode\">\n";
     }

if ($trusted=="TRUE") {
     echo"Trusted <INPUT NAME=\"trusted\" TYPE=\"HIDDEN\" VALUE=\"TRUE\">\n";
}

} else if ($_SESSION["level"]=="admin") {

if ($usermode=="A") {$a="TRUE"; $e="TRUE";
} else if ($usermode=="E") {$e="TRUE"; $a="FALSE";
} else if ($usermode=="U") {$e="FALSE"; $a="FALSE";
}

    echo"Editor: <INPUT NAME=\"editor\" TYPE=\"CHECKBOX\" VALUE=\"TRUE\" "; if ($e=="TRUE") {echo"CHECKED";} if ($a=="TRUE") {echo" DISABLED=\"DISABLED\"";} echo">\n ";
    echo"Admin: <INPUT NAME=\"admin\" TYPE=\"CHECKBOX\" VALUE=\"TRUE\" "; if ($a=="TRUE") {echo"CHECKED";} echo">\n ";
    echo"Trusted: <INPUT NAME=\"trusted\" TYPE=\"CHECKBOX\" VALUE=\"TRUE\" "; if ($trusted=="TRUE") {echo"CHECKED";} echo">\n";
}
    echo"</TD></TR>\n";

    echo"<TR><TD><B>E-Mail Public:<B></TD><TD>";
    if ($useremailhide==="1") {
    echo"Hidden: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"1\" CHECKED> Visible: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"0\">";
    } else if ($useremailhide==="0") {
    echo"Hidden: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"1\"> Visible: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"0\" CHECKED>";
    } else {
    echo"Hidden: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"1\"> Visible: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"0\">";
    }
    echo"</TD></TR>\n";
?>
<TR><TD COLSPAN="2" ALIGN="CENTER"><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Update">&nbsp;&nbsp;<INPUT NAME="reset" TYPE="RESET" VALUE="Reset Form">&nbsp;&nbsp;<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Delete User" ONCLICK="return confirm('Are you sure?');"></TD></TR>
</FORM>
<?php if ($_SESSION["level"]=="user" or $_SESSION["level"]=="editor") {} else { ?>
<TR><TD COLSPAN="2"><A HREF="?function=">&#171;&#171; Return to User Manager</A></TD></TR>
<?php } ?>
</TABLE>

<?php
} else if ($function=="adduser") {

if ($_POST["submit"]=="Create User") {
 //Verify Users Password and md5 encode it for storage...
 if ($_POST[userpass]==$_POST[userpassconfirm]) {
 $_POST[userpass]=md5($_POST[userpass]);
 } else {
 $errors="true";
 echo"<B>Your two passwords did not match, go back and try again...</B>";
 }

 //Add User to MySQL Table
if ($errors !="true") {
    $_POST["username"] = htmlspecialchars($_POST["username"]);

$admin = $_POST["admin"];
$editor = $_POST["editor"];
$trusted = $_POST["trusted"];
$disabled = $_POST["disabled"];
//echo"$i - $admin - $editor - $trusted<br>\n";

if ($admin=="TRUE") { $mode="A"; 
} else if ($editor=="TRUE") { $mode="E"; 
} else if ($disabled=="TRUE") {$mode="D";
} else { $mode="U"; }

if ($trusted !=="TRUE") {$trusted="FALSE"; }


   	$sql = "INSERT INTO `t_userprofiles` (`UserName`, `UserEmail`, `UserWebsite`, `UserPass`, `UserMode`, `UserTrusted`, `UserEmailHide`) VALUES ('$_POST[username]', '$_POST[useremail]', '$_POST[userwebsite]', '$_POST[userpass]', '$mode', '$trusted', '$_POST[useremailhide]');";
    $result = mysql_query($sql) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
    //include"mail_sendaccountdetails.php"; 
    echo"<DIV style=\"width: 550px; font-size: 14pt; font-weight: bold; text-align:center; margin: auto\">The user $_POST[username] has been Successfully Added...</DIV>";
}

}
?>

<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE="border: solid 1px #000000; width: 450px">
    <TR><TD COLSPAN=2 STYLE="font-size:14pt; font-weight: bold; border-bottom: solid 1px #000000">Add New User:</TD></TR>
<FORM NAME="adduser" METHOD="POST" ACTION="?function=adduser">
    <TR><TD><B>E-Mail:</B></TD><TD><INPUT NAME="useremail" TYPE="TEXT" VALUE="<?php echo"$_POST[email]"; ?>" SIZE=30 MAXLENGTH=100></TD></TR>
    <TR><TD STYLE="width: 150px"><B>Name:</B></TD><TD><INPUT NAME="username" TYPE="TEXT" VALUE="" SIZE=30 MAXLENGTH=100></TD></TR>
    <TR><TD><B>Website:</B></TD><TD><INPUT NAME="userwebsite" TYPE="TEXT" VALUE="" SIZE=30 MAXLENGTH=100></TD></TR>
    <TR><TD><B>Password:</B></TD><TD><INPUT NAME="userpass" TYPE="PASSWORD" VALUE="" SIZE=30 MAXLENGTH=200></TD></TR>
    <TR><TD ALIGN=RIGHT><FONT STYLE="font-size: 10pt"><B>Confirm:</B></FONT>&nbsp;&nbsp;</TD><TD><INPUT NAME="userpassconfirm" TYPE="PASSWORD" VALUE="" SIZE=30 MAXLENGTH=200></TD></TR>
    <TR><TD><B>Permissions:</B></TD><TD>Editor: <INPUT NAME="editor" TYPE="CHECKBOX" VALUE="TRUE"> Admin: <INPUT NAME="admin" TYPE="CHECKBOX" VALUE="TRUE"> Trusted: <INPUT NAME="trusted" TYPE="CHECKBOX" VALUE="TRUE"></TD></TR>
    <TR><TD><B>E-Mail Public:<B></TD><TD>Hidden: <INPUT NAME="useremailhide" TYPE="RADIO" VALUE="1" CHECKED> Visible: <INPUT NAME="useremailhide" TYPE="RADIO" VALUE="0"></TD></TR>
<TR><TD COLSPAN="2" ALIGN="CENTER"><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Create User">&nbsp;&nbsp;<INPUT NAME="reset" TYPE="RESET" VALUE="Reset Form"></TD></TR>
</FORM>
<TR><TD COLSPAN="2"><A HREF="?function=">&#171;&#171; Return to User Manager</A></TD></TR>
</TABLE>

<?php
} else if ($function=="changepassword") {
if (!$userid) {$userid = $_GET["userid"]; }

//Set Password Change if this is a POST.
if ($_POST["submit"]=="Change Password") {
   echo"<DIV style=\"width: 500px; font-size: 14pt; font-weight: bold; text-align:center; margin: auto\">";
 $sql = "SELECT `UserPass` FROM `t_userprofiles` WHERE `UserID` = '$_POST[userid]' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $row = mysql_fetch_array($sql_result);
    $userpass = $row["UserPass"];
    $oldpass = md5($_POST[oldpass]);

if ($_SESSION["level"]=="admin") {$oldpass=$userpass; } //Bypass Old Password check for Admins only
    if ($userpass==$oldpass) {

    if ($_POST[newpass]==$_POST[newpass2]) {
     $userpass = md5($_POST["newpass"]);
     $sql = "UPDATE `t_userprofiles` SET `UserPass`='$userpass' WHERE `UserID`='$_POST[userid]'";
     //echo"$sql\n<br>";
       $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
       echo"The password has been successfully reset.<br>";

    } else {
     echo"The two passwords did not match, please go back and try again.</FONT>";
    }


    } else {
     echo"Your Old password did not match the password on file, please try again.</FONT>";

    }
    echo"</DIV>\n";

}

if (!$userid) { $userid = $_POST["userid"]; }
//Get Name of User for Form
 $sql = "SELECT `UserName` FROM `t_userprofiles` WHERE `UserID` = '$userid' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $row = mysql_fetch_array($sql_result);
    $username = $row["UserName"];
?>
<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE="border: solid 1px #000000; width: 500px">
    <TR><TD COLSPAN=2 STYLE="font-size:14pt; font-weight: bold; border-bottom: solid 1px #000000">Change password for <?php echo"$username"; ?>:</TD></TR>
<FORM NAME="adduser" METHOD="POST" ACTION="?function=changepassword">
    <INPUT NAME="userid" TYPE="HIDDEN" VALUE="<?php echo"$userid"; ?>">
<?php if ($_SESSION["level"] !=="admin") { ?>
    <TR><TD><B>Old Password:</B></TD><TD><INPUT NAME="oldpass" TYPE="PASSWORD" VALUE="" SIZE=30 MAXLENGTH=200></TD></TR>
<?php } ?>
    <TR><TD><B>New Password:</B></TD><TD><INPUT NAME="newpass" TYPE="PASSWORD" VALUE="" SIZE=30 MAXLENGTH=200></TD></TR>
    <TR><TD><B>Retype New Password:</B>&nbsp;&nbsp;&nbsp;</TD><TD><INPUT NAME="newpass2" TYPE="PASSWORD" VALUE="" SIZE=30 MAXLENGTH=200></TD></TR>
<TR><TD COLSPAN="2" ALIGN="CENTER"><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Change Password">&nbsp;&nbsp;<INPUT NAME="reset" TYPE="RESET" VALUE="Reset Form"></TD></TR>
</FORM>
<TR><TD COLSPAN="2"><A HREF="?function=">&#171;&#171; Return to User Manager</A></TD></TR>
</TABLE>
<?php
} else {}
?>

<?php
include"$page_footer";
?>
</BODY>
</HTML>