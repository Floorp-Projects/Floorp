<?php
require"../core/config.php";
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<TITLE>Mozilla Update :: Create An Account</TITLE>

<?php
include"$page_header";
$function = $_GET["function"];
if (!$function or $function=="step1") {
?>
<hr class="hide">
<div id="mBody">
<div id="mainContent" class="right">

<h3>Create an Account</h3>
Joining Mozilla Update is easy just fill out the form below and click the join button.

<form name="createaccount" method="post" action="createaccount.php?function=step2">
<table border=0 cellpadding=0 cellspacing=0>
<tr><td colspan=2>Your e-mail address is used as your username to login. You'll also receive confirmation e-mail to this address. In order for your account to be activated succesfully, you must specify a valid e-mail address.</td></tr>
<tr>
<td>E-Mail Address:</td>
<td><input name="email" type="text" size=30></td>
</tr>
<tr>
<td>Confirm E-Mail:</td>
<td><input name="emailconfirm" type="text" size=30></td>
</tr>
<tr><td colspan=2>How do you want to be known to visitors of Mozilla Update? This is your "author name" it will be shown with your extension/theme listings on the site.</td></tr>
<tr>
<td>Your Name</td>
<td><input name="name" type="text" size=30></td>
</tr>
<tr><td colspan=2>If you have a website, enter the URL here. (including the http:// ) Your website will be shown to site visitors on your author profile page. This field is optional, if you don't have a website or don't want it linked to from Mozilla Update, leave this box blank. </td></tr>
<tr>
<td>Your Website</td>
<td><input name="website" type="text" size=30></td>
</tr>
<tr><td colspan=2>Your desired password. This along with your e-mail will allow you to login, so make it something memorable but not easy to guess. Type it in both fields below, the two fields must match.</td></tr>
<tr>
<td>Password:</td>
<td><input name="password" type="password" size=30></td>
</tr>
<tr>
<td>Confirm Password:</td>
<td><input name="passwordconfirm" type="password" size=30></td>
</tr>
<tr><td colspan=2>Review what you entered above, if everything's correct, click the "Join Mozilla Update" button. If you want to start over, click "Clear Form".</td></tr>
<tr>
<td colspan=2><input name="submit" type="submit" value="Join Mozilla Update"><input name="reset" type="reset" value="Clear Form"></td>
</tr>
</table>

</form>

</div>
<div id="side" class="right">
<h3>Already Have an Account?</h3>
<P>If you already have signed-up for an account, you don't need to sign-up again. Just use your e-mail address and password and <a href="index.php">login</a>.</P>
<P>If you don't remember the password for your acconut, you can <a href="passwordreset.php">recover a forgotten password</a>.</P>
</div>

<?php
} else if ($function=="step2") {
echo"<h1>Processing New Account Request, Please Wait...</h1>\n";
//Gather and Filter Data from the Submission Form
if ($_POST["email"]==$_POST["emailconfirm"]) {$email = $_POST["email"];} else { $errors="true"; $emailvalid="no";}
if ($_POST["password"]==$_POST["passwordconfirm"]) {$password = $_POST["password"];} else { $errors="true"; $passwordvalid="no"; }
if ($_POST["name"]) { $name = $_POST["name"]; } else { $errors="true"; $namevalid="no"; }
$website = $_POST["website"];

//Check e-mail address and see if its already in use.
if ($emailvalid !="no") {
$sql = "SELECT `UserEmail` from `t_userprofiles` WHERE `UserEmail`='$email' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  if (mysql_num_rows($sql_result)>"0") {$errors="true"; $emailvalid="no"; }
}

if ($errors == "true") {
echo"Errors have been found in your submission, please go back to the previous page and fix the errors and try again.<br>\n";
if ($emailvalid=="no") {echo"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Your e-mail addresses didn't match, or your e-mail is already in use.<BR>\n"; }
if ($passwordvalid=="no") {echo"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;The passwords you entered did not match.<BR>\n"; }
if ($namevalid=="no") {echo"&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;The name field cannot be left blank.<BR>\n"; }

include"$page_footer";
echo"</BODY>\n</HTML>\n";
exit;
}

//We've got good data here, valid password & e-mail. 

//Generate Confirmation Code
$confirmationcode = md5(mt_rand());
$password_plain = $password;
$password = md5($password);

$sql = "INSERT INTO `t_userprofiles` (`UserName`,`UserEmail`,`UserWebsite`,`UserPass`,`UserMode`,`ConfirmationCode`) VALUES ('$name','$email','$website','$password','D','$confirmationcode');";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  if ($sql_result) {
    include"mail_newaccount.php";
    echo"Your account has been created successfully. An e-mail has been sent to you with instructions on how to activate your account so you can begin using it.<br>\n";
    echo"<br><br><a href=\"./index.php\">&#171;&#171; Login to Mozilla Update's Developer Control Panel &#187;&#187;</a>";
  }

} else if ($function=="confirmaccount") {
?>
<h1>Activate Your Mozilla Update Account</h1>
<?php
//Get the two URI variables from the query_string..
$email = $_GET["email"];
$confirmationcode = $_GET["confirmationcode"];

//Check DB to see if those two values match a record.. if it does, activate the account, if not throw error.
$sql = "SELECT `UserID` from `t_userprofiles` WHERE `UserEmail`='$email' and `ConfirmationCode`='$confirmationcode' LIMIT 1";
  $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   if (mysql_num_rows($sql_result)=="1") {
     $row = mysql_fetch_array($sql_result);
      $userid = $row["UserID"];
      $sql = "UPDATE `t_userprofiles` SET `UserMode`='U', `ConfirmationCode`=NULL WHERE `UserID`='$userid' LIMIT 1";
        $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        if ($sql_result) {
        echo"Thanks! Your account has been activated successfully, you may now login and being using Mozilla Update's Developer Control Panel.";
        echo"<br><br><a href=\"./index.php\">&#171;&#171; Login to Mozilla Update's Developer Control Panel &#187;&#187;</a>";
        }
   } else {
     echo"Sorry, the e-mail and confirmation code do not match, please make sure you've copied the entire URL, if you copy/pasted it from your e-mail client, and try again.";
     echo"<br><br><a href=\"./index.php\">&#171;&#171; Back to Mozilla Update Developer Control Panel Home &#187;&#187;</a>";

   }

?>


<?php
} else {}
?>

<?php
include"$page_footer";
?>
</BODY>
</HTML>
