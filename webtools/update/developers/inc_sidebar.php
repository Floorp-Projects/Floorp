<div id="side">
<ul id="nav">
<li><A HREF="main.php">Overview</A></li>
<?php
if ($_SESSION["level"] == "user") {
?>
<li><A HREF="usermanager.php">Your Profile</A></li>

<?php
} else if ($_SESSION["level"] == "editor") {
?>
<li><A HREF="usermanager.php?function=edituser&userid=<?php echo"$_SESSION[uid]"; ?>">Your Profile</A></li>
<li><A HREF="approval.php">Approval Queue</A></li>
<li><A HREF="listmanager.php?type=T">Themes list</A></li>
<li><A HREF="listmanager.php?type=E">Extensions list</A></li>
<li><A HREF="usermanager.php">Users Manager</A></li>
<?php
} else {
?>
<li><A HREF="usermanager.php?function=edituser&userid=<?php echo"$_SESSION[uid]"; ?>">Your Profile</A></li>
<li><A HREF="approval.php">Approval Queue</A></li>
<li><A HREF="listmanager.php?type=T">Themes list</A></li>
<li><A HREF="listmanager.php?type=E">Extensions list</A></li>
<li><A HREF="usermanager.php">Users Manager</A></li>
<li><a href="appmanager.php">Application Manager</a></li>
<li><a href="categorymanager.php">Category Manager</A></li>
<li><a href="faqmanager.php">FAQ Manager</A></li>
<?php } ?>
<li><a href="logout.php">Logout</A></li>
</ul>
</DIV>
<hr class="hide">
<div id="mainContent">
