<?php
require"../core/config.php";
require"core/sessionconfig.php";

//If already logged in, we don't need to show the prompt... redirect the user in.
if ($_SESSION["logoncheck"]=="YES") {
$return_path="developers/main.php?sid=$sid";
header("Location: http://$sitehostname/$return_path");
exit;
}
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html401/strict.dtd">
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

<h2>About the Developer Control Panel</h2>
<P class="first">The Mozilla Update Developer Control Panel allows Extension and Theme authors full access to manage the
listings of their items on Mozilla Update. This includes the ability to add/remove new versions or whole new items,
add/remove screenshots, update the compatibility information and even to report questionable comments left in the item's feedback
to the Mozilla Update Staff.</P>


</div>
<div id="side" class="right">
<h2>Developers Login</h2>
<?php if ($_GET[login]=="failed") { ?>
<strong>You were not successfully logged in. Check your e-mail address and password and try again.</strong>
<?php } else if ($_GET[logout]=="true") { ?>
<strong>You've been successfully logged out.</strong>
<?php } else {} ?>
<FORM NAME="login" METHOD="POST" ACTION="login.php">
<TABLE CELLPADDING=1 CELLSPACING=1 style="margin: auto">
<TR><TD STYLE="margin-top: 4px"></TD></TR>
<TR>
<TD><strong>E-Mail:</strong></TD><TD><INPUT NAME="email" TYPE="TEXT" SIZE=30 MAXLENGTH=200></TD>
</TR>
<TR>
<TD><strong>Password:</strong></TD><TD><INPUT NAME="password" TYPE="PASSWORD" SIZE=30 MAXLENGTH=100></TD>
</TR>
<TR>
<TD ALIGN=CENTER COLSPAN=2><INPUT NAME="submit" TYPE="SUBMIT" VALUE="Login"> <input type="reset" value="Reset"></TD>
</TR>
</TABLE>
</FORM>
<a href="passwordreset.php">Forgot your password?</a>

<h2>Create an Account</h2>
<P>You need an account to access the features of the Developer Control Panel and add your extension or themes to Mozilla Update.</P>

<a href="createaccount.php">Join Mozilla Update!</a>

</div>

</div>

<?php
include"$page_footer";
?>
</BODY>
</HTML>
