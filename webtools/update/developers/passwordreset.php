<?php
require"../core/config.php";
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<TITLE>Mozilla Update :: Password Reset</TITLE>

<?php
include"$page_header";
?>
<hr class="hide">
<div id="mBody">

<?php
if ($_POST["submit"]=="Reset My Password") {
echo"<h1>Resetting Your Password, Please Wait...</h1>\n";
//Gather and Filter Data from the Submission Form
$email = $_POST["email"];

//Generate Confirmation Code
$confirmationcode = md5(mt_rand());

$newpassword = substr(md5(mt_rand()),0,14);
$password_plain = $newpassword;
$password = md5($newpassword);

$sql = "SELECT `UserEmail` FROM `t_userprofiles` WHERE `UserEmail`='$email' LIMIT 1";
 $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
   if (mysql_num_rows($sql_result)=="1") {
    $sql = "UPDATE `t_userprofiles` SET `UserPass`='$password' WHERE `UserEmail`='$email' LIMIT 1";
     $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
      if ($sql_result) {
        include"mail_newpassword.php";
        echo"Your password has been reset successfully. An e-mail has been sent to you containing your new password..<br>\n";
        echo"<br><br><a href=\"./index.php\">&#171;&#171; Login to Mozilla Update's Developer Control Panel &#187;&#187;</a>";
      }

   } else { 
   echo"An error was encountered when trying to reset your password, verify the e-mail you entered is correct and try again.<br>\n";
   }

include"$page_footer";
echo"</body>\n</html>\n";
exit;
}
?>


<h3>Reset Your Password</h3>
<P>Forgot the password to your Mozilla Update account? No problem. Just put your e-mail address in the form below, and a new password will
be generated and e-mailed to the e-mail address you have on file.</P>

<form name="resetpassword" method="post" action="?post=resetpassword">
<table border=0 cellpadding=0 cellspacing=0 style="width: 50%; margin: auto;">
<tr>
<td style="height: 30px";>E-Mail Address:</td>
<td><input name="email" type="text" size=30></td>
</tr>
<tr>
<td colspan=2 align=center><input name="submit" type="submit" value="Reset My Password"><input name="reset" type="reset" value="Clear Form"></td>
</tr>
</table>

</form>
<?php
include"$page_footer";
?>
</BODY>
</HTML>
