<?php
require"../core/config.php";
require"core/sessionconfig.php";

//If already logged in, we don't need to show the prompt... redirect the user in.
if ($_SESSION["logoncheck"]=="YES") {
$return_path="developers/main.php?sid=$sid";
header("Location: http://$_SERVER[HTTP_HOST]/$return_path");
exit;
}
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html401/loose.dtd">
<HTML>
<HEAD>
<LINK REL="STYLESHEET" TYPE="text/css" HREF="/admin/core/mozupdates.css">
<TITLE>Mozilla Update :: Developer Control Panel</TITLE>

<?php
include"$page_header";
?>
<hr class="hide">
<div id="mBody">
<div id="mainContent" class="right">

	      <h3>About the Developer Control Panel</h3>

</div>
<div id="side" class="right">
<h3>Developers Login</h3>
<?php if ($_GET[login]=="failed") { ?>
<strong>You were not successfully logged in. Check your e-mail address and password and try again.</strong>
<?php } else if ($_GET[logout]=="true") { ?>
<strong>You've been successfully logged out.</strong>
<?php } else {} ?>
<TABLE CELLPADDING=1 CELLSPACING=1 ALIGN="CENTER">
<TR><TD STYLE="margin-top: 4px"></TD></TR>
<FORM NAME="login" METHOD="POST" ACTION="login.php">
<TR>
<TD><strong>E-Mail:</strong></TD><TD><INPUT NAME="email" TYPE="TEXT" SIZE=30 MAXLENGTH=200></TD>
</TR>
<TR>
<TD><strong>Password:</strong></TD><TD><INPUT NAME="password" TYPE="PASSWORD" SIZE=30 MAXLENGTH=100></TD>
</TR>
<TR>
<TD ALIGN=CENTER COLSPAN=2><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Login"> <input type="reset" value="Reset"></TD>
</TR>
</FORM>
</TABLE>
<a href="passwordreset.php">Forgot your password?</a>

<h3>Create an Account</h3>
<P>You need an account to access the features of the Developer Control Panel and add your extension or themes to Mozilla Update.</P>

<a href="createaccount.php">Join Mozilla Update!</a>



</div>

<?php
include"$page_footer";
?>
</BODY>
</HTML>
