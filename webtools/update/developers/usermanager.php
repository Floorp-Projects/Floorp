<?php
require"core/sessionconfig.php";
require"../core/config.php";

class PermissionsManager {
  var $modes;
  var $trusted;
  var $mode;
  var $func;
  function PermissionsManager($function) {
    $this->func=$function;
    $this->modes=array('A'=>'Admin','E'=>'Editor','U'=>'User','D'=>'Disabled');
    $mode=$_POST['usermode'];
    if( (!$this->decodeMode($mode)) // unknown mode degrades to U
        || ( $mode == 'A' && $_SESSION['level'] != 'admin' ) 
            // only admins can create admins  
    ) { 
      $mode='U';
    }
    $this->mode=$mode;
    $this->trusted=$_POST['trusted']=='TRUE'?'TRUE':'FALSE';
  }
  
  function decodeMode($mode) {
    return $this->modes[$mode];
  }
  
  function printUI() {
    $level=$_SESSION['level'];
    if( $this->func == 'edituser' && ($level=='user' || $level=='editor')) {
      // read-only (hidden) fields: "usermode" and "trusted"
      $umDes=$this->decodeMode($this->mode);
      echo "<input name=\"usermode\" type=\"hidden\" value=\"$this->mode\">"
        . ( $umDes?$umDes:"Unknown ($this->mode)" );
      
      if ($this->trusted=="TRUE") {
        echo "Trusted <INPUT NAME=\"trusted\" TYPE=\"HIDDEN\" VALUE=\"TRUE\">\n";
      }
    } else if( $level=='admin' 
      || ($editorAdding=($this->func == 'adduser' && $level=='editor')) ) { 
      // editable fields: "usermode" (radio) and "trusted" (checkbox)
      $modes=$this->modes;
      if($editorAdding) array_shift($modes); // only admins can create admins 
      foreach($modes as $umId=>$umDes) { // decode usermodes
        echo '<input name="usermode" type="radio" value="'.$umId
          . ($umId===$this->mode?'" checked="checked"/>':'"/>' )
          ."$umDes\n";
      }
      
      echo '<input name="trusted" type="checkbox" value="TRUE"';
      if($this->trusted) echo ' checked="checked"';
      echo ">Trusted\n";
    
    }
  }
  
}

$function = $_GET["function"];
$perms=new PermissionsManager($function);

//Access Level: only admins can edit somebody else's profile
if ($_SESSION["level"] !=="admin") {
  //Kill access to add user.
  $function="edituser";
  $userid=$_SESSION["uid"];
}

?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<TITLE>Mozilla Update :: Developer Control Panel :: User Manager</TITLE>
<?php
include"$page_header";
include"inc_sidebar.php";
?>
<?php
//Security Check for EditUser/ChangePassword function.
if ($function=="edituser" or $function=="changepassword") {
  $postuid = escape_string($_GET["userid"]);
  $userid = escape_string($_SESSION["uid"]);
  // All users users may change their own accounts, check when trying to change other accounts
  if ($postuid and $postuid != $userid) {
    $allowed = false;
    if ($_SESSION["level"] == "admin") {
      // Admins may change any account
      $allowed = true;
    } else if ($_SESSION["level"]=="editor") {
      // Editors may only change regular users, not editors or admins
      $sql = "SELECT `UserID` from `userprofiles` WHERE " .
             "`UserMode`='U' and `UserID`='$postuid'" .
             " LIMIT 1";
      $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
      if (mysql_num_rows($sql_result) == 1) {
        $allowed = true;
      }
    }

    if ($allowed == false) {
      echo "<h1>Error Accessing Record</h1>\n";
      echo "You do not appear to have permission to edit this record.<br>\n";
      echo "<a href=\"?function=\">&#171;&#171; Go Back</a>\n";
      include"$page_footer";
      echo "</body>\n<html>\n";
      exit;
    } else {
      $userid = $postuid;
    }
  }
}
?>
<?php
if (!$function) {
?>

<?php
if ($_POST["submit"] && $_GET["action"]=="update") {
?>
<h1>Updating User List...</h1>
<?php

    //Process Post Data, Make Changes to User Table.
    //Begin General Updating
    for ($i=1; $i<=$_POST['maxuserid']; $i++) {
      $admin = escape_string($_POST["admin$i"]);
      $editor = escape_string($_POST["editor$i"]);
      $trusted = escape_string($_POST["trusted$i"]);
      $disabled = escape_string($_POST["disabled$i"]);
      $selected = escape_string($_POST["selected$i"]);
      //echo "$i - $admin - $editor - $trusted - $selected<br>\n";

      if ($admin=="TRUE") { $mode="A"; 
      } else if ($editor=="TRUE") { $mode="E"; 
      } else if ($disabled=="TRUE") {$mode="D";
      } else { $mode="U"; }

      if ($trusted !=="TRUE") {$trusted="FALSE"; }

      if (checkFormKey()) {
        $sql = "UPDATE `userprofiles` SET `UserMode`= '$mode', `UserTrusted`= '$trusted' WHERE `UserID`='$selected'";
        $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
      }

      //Do Special Disable, Delete, Enable Account Operations
      if ($_POST["selected$i"] AND $_POST["submit"] !=="Update") {
	$selecteduser = escape_string($_POST["selected$i"]);

	if ($_POST["submit"]=="Disable Selected") {
          if (checkFormKey()) {
	    $sql = "UPDATE `userprofiles` SET `UserMode`= 'D' WHERE `UserID`='$selecteduser'";
	    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
	    if ($sql_result) {
	      echo "User Account for User Number $selecteduser Disabled<br>\n";
	    }

	    //Disabling an author, check their extension list and disable any item they're the solo author of.
	    $sql = "SELECT TM.ID, TM.Name from `main` TM INNER JOIN `authorxref` TAX ON TM.ID=TAX.ID WHERE TAX.UserID = '$selecteduser'";
	    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
	    while ($row = mysql_fetch_array($sql_result)) {
	      $id = $row["ID"];
	      $name = $row["Name"];
	      $sql2 = "SELECT `ID` from `authorxref` WHERE `ID` = '$id'";
	      $sql_result2 = mysql_query($sql2, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
	      if (mysql_num_rows($sql_result2)<="1") {
	        $sql3 = "UPDATE `version` SET `approved`='DISABLED' WHERE `ID`='$id' and `approved` !='NO' ";
	        $sql_result3 = mysql_query($sql3, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
	        if ($sql_result3) {
	  	  echo "$name disabled from public viewing...<br>\n";
	        }
	      }
            }
	  }

	} else if ($_POST["submit"]=="Delete Selected") {
          if (checkFormKey()) {
	    $sql = "DELETE FROM `userprofiles` WHERE `UserID`='$selecteduser' LIMIT 1";
	    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
	    if ($sql_result) {
	      echo "User Account for User Number $selecteduser Deleted<br>\n";
	    }
          }

	} else if ($_POST["submit"]=="Enable Selected") {
          if (checkFormKey()) {
	    $sql = "UPDATE `userprofiles` SET `UserMode`= 'U' WHERE `UserID`='$selecteduser'";
	    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
	    if ($sql_result) {
	      echo "User Account for User Number $selecteduser Enabled, User Mode set to User<br>\n";
	    }
	    //Disabling an author, check their extension list and disable any item they're the solo author of.
	    $sql = "SELECT TM.ID, TM.Name from `main` TM INNER JOIN `authorxref` TAX ON TM.ID=TAX.ID WHERE TAX.UserID = '$selecteduser'";
	    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
	    while ($row = mysql_fetch_array($sql_result)) {
	      $id = $row["ID"];
	      $name = $row["Name"];
	      $sql2 = "SELECT `ID` from `authorxref` WHERE `ID` = '$id'";
	      $sql_result2 = mysql_query($sql2, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
	      if (mysql_num_rows($sql_result2)<="1") {
	        $sql3 = "UPDATE `version` SET `approved`='?' WHERE `ID`='$id' and `approved` !='NO'";
	        $sql_result3 = mysql_query($sql3, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
	        if ($sql_result3) {
	  	  echo "$name restored to public view pending approval...<br>\n";
	        }
	      }
	    }
          }
	}
      }


    }
  unset($i);

  echo "Your changes to the User List have been successfully completed<BR>\n";

 }
?>
<h1>Manage User list</h1>
<TABLE BORDER=0 CELLPADDING=1 CELLSPACING=1 ALIGN=CENTER STYLE="border: 0px; width: 100%" class="listing">
<TR style="font-weight: bold">
<TH></TH>
<TH>Name</TH>
<TH>E-Mail Address</TH>
<TH>S</TH>
<TH>E</TH>
<TH>A</TH>
<TH>T</TH>
</TR>
<FORM NAME="updateusers" METHOD="POST" ACTION="?function=&action=update">
<?writeFormKey();?>
<?php
 $sql = "SELECT * FROM `userprofiles` ORDER BY `UserMode`, `UserName` ASC";
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
    echo "<TR>";
    echo "<TD CLASS=\"tablehighlight\" ALIGN=CENTER><B>$i</B></TD>\n";
    echo "<TD CLASS=\"tablehighlight\"><B>&nbsp;&nbsp;<A HREF=\"?function=edituser&userid=$userid\">$username</A></B></TD>\n";
    echo "<TD CLASS=\"tablehighlight\"><B>&nbsp;&nbsp;<A HREF=\"mailto:$useremail\">$useremail</A></B></TD>\n";
    echo "<TD CLASS=\"tablehighlight\"><INPUT NAME=\"selected$userid\" TYPE=\"CHECKBOX\" VALUE=\"$userid\" TITLE=\"Selected User\""; if (($a=="TRUE" or $e=="TRUE") AND $_SESSION["level"]=="editor") {echo " DISABLED=\"DISABLED\"";} echo "></TD>";
    echo "<TD CLASS=\"tablehighlight\"><INPUT NAME=\"editor$userid\" TYPE=\"CHECKBOX\" VALUE=\"TRUE\" "; if ($e=="TRUE") {echo "CHECKED=\"CHECKED\""; } if (($a=="TRUE" or $d=="TRUE") or $_SESSION["level"]=="editor") {echo " DISABLED=\"DISABLED\"";} echo " TITLE=\"Editor\"></TD>";
    echo "<TD CLASS=\"tablehighlight\"><INPUT NAME=\"admin$userid\" TYPE=\"CHECKBOX\" VALUE=\"TRUE\" "; if ($a=="TRUE") {echo "CHECKED=\"CHECKED\""; } if ($d=="TRUE" or $_SESSION["level"]=="editor") {echo " DISABLED=\"DISABLED\"";} echo " TITLE=\"Administrator\"></TD>";
    echo "<TD CLASS=\"tablehighlight\"><INPUT NAME=\"trusted$userid\" TYPE=\"CHECKBOX\" VALUE=\"TRUE\" "; if ($t=="TRUE") {echo "CHECKED=\"CHECKED\""; } if ($d=="TRUE" or (($a=="TRUE" or $e=="TRUE") AND $_SESSION["level"]=="editor" )) {echo " DISABLED=\"DISABLED\"";}echo " TITLE=\"Trusted User\"></TD>";
    if ($d=="TRUE") {echo "<INPUT NAME=\"disabled$userid\" TYPE=\"HIDDEN\" VALUE=\"TRUE\">\n"; }
    echo "</TR>\n";

  unset($a,$e,$t);
}

echo "<INPUT NAME=\"maxuserid\" TYPE=\"HIDDEN\" VALUE=\"$maxuserid\">";

?>
<TR><TD COLSPAN=3 ALIGN=CENTER>
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Disable Selected" ONCLICK="return confirm('Disabling this account will hide all their extensions and themes from view and prevent them from logging in. Do you want to procede and disable this account?');">
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Enable Selected">
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Delete Selected" ONCLICK="return confirm('Deleting this account will permanently remove all their extensions and account information from Mozilla Update. This cannot be undone. Do you want to procede and delete this account?');">
<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Update">
</TD>
<TD>
</TR>
</FORM>
</TABLE>
<h2><a href="?function=adduser">Add New User</A></h2>
<div style="width: 580px; border: 0px dotted #AAA; margin-top: 1px; margin-left: 50px; margin-bottom: 5px; font-size: 10pt; font-weight: bold">

<form name="adduser" method="post" action="?function=adduser">
<?writeFormKey();?>
  E-Mail: <input name="email" type="text" size="30" maxlength="150" value="">
<input name="submit" type="submit" value="Add User"></SPAN>
</form>
</div>


<?php
} else if ($function=="edituser") {


if (!$userid) {$userid = escape_string($_GET["userid"]);}

//Process Submitted Values if this is a return with $_POST data...
if ($_POST["submit"]=="Update") {
  if ($_SESSION["level"] !=="admin" && $_SESSION["uid"] !== $_POST["userid"]) {$_POST["userid"]=$_SESSION["uid"];}
    
  $userid = escape_string($_POST["userid"]);
  $username = escape_string(htmlspecialchars($_POST["username"]));
  $useremail = escape_string($_POST["useremail"]);
  $userwebsite = escape_string($_POST["userwebsite"]);
  $useremailhide = escape_string($_POST["useremailhide"]);

  if (checkFormKey()) {
    $sql = "UPDATE `userprofiles` SET `UserName`= '$username', `UserEmail`='$useremail', `UserWebsite`='$userwebsite', `UserMode`='$perms->mode', `UserTrusted`='$perms->trusted', `UserEmailHide`='$useremailhide' WHERE `UserID`='$userid'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
    if ($sql_result) {
      echo "<h1>Updating User Profile...</h1>\n";
      echo "The User Profile for $username, has been successfully updated...<br>\n";
    }
  }
} else if ($_POST["submit"] == "Delete User") {
  if ($_SESSION["level"] !=="admin" && $_SESSION["uid"] !== $_POST["userid"]) {$_POST["userid"]=$_SESSION["uid"];}
  $userid = escape_string($_POST["userid"]);
  if (checkFormKey()) {
    $sql = "DELETE FROM `userprofiles` WHERE `UserID`='$userid'";
    $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
    if ($sql_result) {
      $username = htmlspecialchars($_POST["username"]);
      echo "<h1>Deleting User... Please wait...</h1>\n";
      echo "You've successfully deleted the user profile for $username...<br>\n";
      include"$page_footer";
      echo "</body>\n</html>\n";
      exit;
    }
  }
}

if (!$userid) {$userid=escape_string($_POST["userid"]);}

//Show Edit Form
 $sql = "SELECT * FROM `userprofiles` WHERE `UserID` = '$userid' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $row = mysql_fetch_array($sql_result);
    $userid = $row["UserID"];
    $username = $row["UserName"];
    $useremail = $row["UserEmail"];
    $userwebsite = $row["UserWebsite"];
    $userpass = $row["UserPass"];
    $perms->mode = $row["UserMode"];
    $trusted = $row["UserTrusted"];
    $useremailhide = $row["UserEmailHide"];
    $userlastlogin = date("l, F, d, Y, g:i:sa", strtotime($row["UserLastLogin"]));
?>

<h1>Edit User Profile for <?php echo "$username"; ?></h1>
<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE="border: 0px; width: 95%">
<TR><TD COLSPAN=2>Last login: <?php echo "$userlastlogin"; ?></TD></TR>
<FORM NAME="edituser" METHOD="POST" ACTION="?function=edituser">
<?writeFormKey();?>
<?php
    echo "<INPUT NAME=\"userid\" TYPE=\"HIDDEN\" VALUE=\"$userid\">\n";
    echo "<TR><TD STYLE=\"width: 130px\"><B>Name:</B></TD><TD><INPUT NAME=\"username\" TYPE=\"TEXT\" VALUE=\"$username\" SIZE=30 MAXLENGTH=100></TD></TR>\n";
    echo "<TR><TD><B>E-Mail:</B></TD><TD><INPUT NAME=\"useremail\" TYPE=\"TEXT\" VALUE=\"$useremail\" SIZE=30 MAXLENGTH=100></TD></TR>\n";
    echo "<TR><TD><B>Website:</B></TD><TD><INPUT NAME=\"userwebsite\" TYPE=\"TEXT\" VALUE=\"$userwebsite\" SIZE=30 MAXLENGTH=100></TD></TR>\n";
    echo "<TR><TD><B>Password:</B></TD><TD><FONT STYLE=\"font-size:10pt; font-weight: bold\"><A HREF=\"?function=changepassword&userid=$userid\">Change Password</A></FONT></TD></TR>\n";

    echo "<TR><TD><B>Permissions:</B></TD><TD>";

    $perms->printUI();
    
    echo "</TD></TR>\n";

    echo "<TR><TD><B>E-Mail Public:<B></TD><TD>";
    if ($useremailhide==="1") {
      echo "Hidden: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"1\" CHECKED> Visible: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"0\">";
    } else if ($useremailhide==="0") {
      echo "Hidden: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"1\"> Visible: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"0\" CHECKED>";
    } else {
      echo "Hidden: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"1\"> Visible: <INPUT NAME=\"useremailhide\" TYPE=\"RADIO\" VALUE=\"0\">";
    }
    echo "</TD></TR>\n";
?>
<TR><TD COLSPAN="2"><strong>Warning:</strong> Setting E-Mail Public to Visible will show your e-mail address in plaintext on your profile. Do not enable this if you wish to keep your e-mail address hidden.</TD></TR>
<TR><TD COLSPAN="2" ALIGN="CENTER"><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Update">&nbsp;&nbsp;<INPUT NAME="reset" TYPE="RESET" VALUE="Reset Form">&nbsp;&nbsp;<INPUT NAME="submit" TYPE="SUBMIT" VALUE="Delete User" ONCLICK="return confirm('Are you sure you want to delete the profile for <?php echo "$username"; ?>?');"></TD></TR>
</FORM>
<?php if ($_SESSION["level"]=="user") {} else { ?>
<TR><TD COLSPAN="2"><A HREF="?function=">&#171;&#171; Return to User Manager</A></TD></TR>
<?php } ?>
</TABLE>

<?php
} else if ($function=="adduser") {

if ($_POST["submit"]=="Create User") {
echo "<h1>Adding User...</h1>\n";
 //Verify Users Password and md5 encode it for storage...
 if ($_POST['userpass']==$_POST['userpassconfirm']) {
 $_POST['userpass']=md5($_POST['userpass']);
 } else {
 $errors="true";
 echo "<B>Your two passwords did not match, go back and try again...</B><br>\n";
 }

 //Add User to MySQL Table
if ($errors !="true") {
   
// Set user level, but only allow admins to set the admin flag
if ($admin=="TRUE" && $_SESSION["level"]=="admin") { $mode="A"; 
} else if ($editor=="TRUE") { $mode="E"; 
} else if ($disabled=="TRUE") {$mode="D";
} else { $mode="U"; }

if ($trusted !=="TRUE") {$trusted="FALSE"; }

$username = escape_string(htmlspecialchars($_POST["username"]));
$useremail = escape_string($_POST['useremail']);
$userwebsite = escape_string($_POST['userwebsite']);
$userpass = escape_string($_POST['userpass']);
$useremailhide = escape_string($_POST['useremailhide']);
  if (checkFormKey()) {
    $sql = "INSERT INTO `userprofiles` (`UserName`, `UserEmail`, `UserWebsite`, `UserPass`, `UserMode`, `UserTrusted`, `UserEmailHide`) VALUES ('$username', '$useremail', '$userwebsite', '$userpass', '$mode', '$trusted', '$useremailhide');";
    $sql_result = mysql_query($sql) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
    if ($sql_result) {
      include"mail_newaccount.php"; 
      echo "The user $username has been added successfully...<br>\n";
      echo "An E-Mail has been sent to the e-mail address specified with the login info they need to log in to their new account.<br>\n";
    }
  }
}

}
?>

<h1>Add New User</h1>
<FORM NAME="adduser" METHOD="POST" ACTION="?function=adduser">
<?writeFormKey();?>
<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE="border: 0px; width: 95%">
  <TR><TD><B>E-Mail:</B></TD><TD><INPUT NAME="useremail" TYPE="TEXT" VALUE="<?php echo "$_POST[email]"; ?>" SIZE=30 MAXLENGTH=100></TD></TR>
  <TR><TD ALIGN=RIGHT><B>Show E-Mail:<B></TD><TD>Hidden: <INPUT NAME="useremailhide" TYPE="RADIO" VALUE="1" CHECKED> Visible: <INPUT NAME="useremailhide" TYPE="RADIO" VALUE="0"></TD></TR>
  <TR><TD STYLE="width: 150px"><B>Name:</B></TD><TD><INPUT NAME="username" TYPE="TEXT" VALUE="" SIZE=30 MAXLENGTH=100></TD></TR>
  <TR><TD><B>Website:</B></TD><TD><INPUT NAME="userwebsite" TYPE="TEXT" VALUE="" SIZE=30 MAXLENGTH=100></TD></TR>
  <TR><TD><B>Password:</B></TD><TD><INPUT NAME="userpass" TYPE="PASSWORD" VALUE="" SIZE=30 MAXLENGTH=200></TD></TR>
  <TR><TD ALIGN=RIGHT><FONT STYLE="font-size: 10pt"><B>Confirm:</B></FONT>&nbsp;&nbsp;</TD><TD><INPUT NAME="userpassconfirm" TYPE="PASSWORD" VALUE="" SIZE=30 MAXLENGTH=200></TD></TR>

  <TR>
    <TD><B>Permissions:</B></TD>
    <TD>
      <?php $perms->printUI(); ?>
    </TD>
  </TR>

  <TR><TD COLSPAN="2" ALIGN="CENTER"><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Create User">&nbsp;&nbsp;<INPUT NAME="reset" TYPE="RESET" VALUE="Reset Form"></TD></TR>
  <TR><TD COLSPAN="2"><A HREF="?function=">&#171;&#171; Return to User Manager</A></TD></TR>
</TABLE>
</FORM>

<?php
} else if ($function=="changepassword") {
if (!$userid) {$userid = escape_string($_GET["userid"]); }

//Set Password Change if this is a POST.
if ($_POST["submit"]=="Change Password") {
   echo "<h1>Changing Password, please wait...</h1>\n";
 $userid = escape_string($_POST["userid"]);
 $sql = "SELECT `UserPass`, `UserEmail` FROM `userprofiles` WHERE `UserID` = '$userid' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   $row = mysql_fetch_array($sql_result);
    $userpass = $row["UserPass"];
    $email = $row["UserEmail"];
    $oldpass = md5($_POST['oldpass']);

    if ($userpass==$oldpass) {

    if ($_POST['newpass']==$_POST['newpass2']) {
     $newpassword = $_POST["newpass"];
     $password_plain = $newpassword;
     $userpass = md5($newpassword);

     $sql_result = false;
     if (checkFormKey()) {
       $sql = "UPDATE `userprofiles` SET `UserPass`='$userpass' WHERE `UserID`='$userid'";
       $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
     }
     if ($sql_result) {
       include"mail_newpassword.php";
       echo "The password has been successfully changed, an e-mail has been sent confirming this action.<br>\n";
     }
    } else {
     echo "The two passwords did not match, please go back and try again.<BR>\n";
    }


    } else {
     echo "Your Old password did not match the password on file, please try again.<br>\n";

    }

} else if ($_POST["submit"]=="Generate New Password") {
   echo "<h1>Generating New Password, please wait...</h1>\n";
   $newpassword = substr(md5(mt_rand()),0,14);
   $password_plain = $newpassword;
   $userpass = md5($newpassword);
   $userid = escape_string($_POST["userid"]);

     $sql = "SELECT `UserEmail` FROM `userprofiles` WHERE `UserID` = '$userid' LIMIT 1";
       $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
       $row = mysql_fetch_array($sql_result);
         $email = $row["UserEmail"];

     if (checkFormKey()) {
       $sql = "UPDATE `userprofiles` SET `UserPass`='$userpass' WHERE `UserID`='$userid'";
       $sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
       if ($sql_result) {
       include"mail_newpassword.php";
       echo "The password has been successfully reset. The user has been sent an e-mail notifying them of their new password.<br>\n";
       }
     }
}

if (!$userid) { $userid = escape_string($_POST["userid"]); }
//Get Name of User for Form
 $sql = "SELECT `UserName` FROM `userprofiles` WHERE `UserID` = '$userid' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
    $row = mysql_fetch_array($sql_result);
    $username = $row["UserName"];
?>
<h1>Change password for <?php echo "$username"; ?></h1>
<TABLE BORDER=0 CELLPADDING=2 CELLSPACING=2 ALIGN=CENTER STYLE="border: 0px; width: 95%">
<FORM NAME="adduser" METHOD="POST" ACTION="?function=changepassword&userid=<?php echo "$userid"; ?>">
<?writeFormKey();?>
    <INPUT NAME="userid" TYPE="HIDDEN" VALUE="<?php echo "$userid"; ?>">
<?php if (($_SESSION["level"] =="admin" or $_SESSION["level"]=="editor") and $userid != $_SESSION["uid"]) { ?>
    <TR><TD COLSPAN="2" ALIGN="CENTER"><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Generate New Password"></TD></TR>
<?php } else { ?>
    <TR><TD><B>Old Password:</B></TD><TD><INPUT NAME="oldpass" TYPE="PASSWORD" VALUE="" SIZE=30 MAXLENGTH=200></TD></TR>
    <TR><TD style="width: 190px"><B>New Password:</B></TD><TD><INPUT NAME="newpass" TYPE="PASSWORD" VALUE="" SIZE=30 MAXLENGTH=200></TD></TR>
    <TR><TD><B>Retype New Password:</B>&nbsp;&nbsp;&nbsp;</TD><TD><INPUT NAME="newpass2" TYPE="PASSWORD" VALUE="" SIZE=30 MAXLENGTH=200></TD></TR>
    <TR><TD COLSPAN="2" ALIGN="CENTER"><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Change Password">&nbsp;&nbsp;<INPUT NAME="reset" TYPE="RESET" VALUE="Reset Form"></TD></TR>
<?php } ?>
</FORM>
<TR><TD COLSPAN="2"><A HREF="?function=">&#171;&#171; Return to User Manager</A></TD></TR>
</TABLE>
<?php
} else {}
?>


<!-- close #mBody-->
</div>

<?php
include"$page_footer";
?>
</BODY>
</HTML>