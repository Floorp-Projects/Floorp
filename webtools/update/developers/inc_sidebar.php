<?php
if (($_SESSION["level"] == "admin" or $_SESSION["level"] == "moderator") and $skipqueue != "true") {
    $sql ="SELECT TM.ID FROM `main` TM
        INNER JOIN `version` TV ON TM.ID = TV.ID
        WHERE `approved` = '?' GROUP BY `URI` ORDER BY TV.DateUpdated ASC";
    $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
        $queuenum = mysql_num_rows($sql_result);
}
?>
<div id="mBody">

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
<li><A HREF="usermanager.php?function=edituser&amp;userid=<?php echo"$_SESSION[uid]"; ?>">Your Profile</A></li>
<li><A HREF="approval.php">Approval Queue <?php if ($skipqueue != "true") { echo"($queuenum)"; } ?></A></li>
<li><A HREF="listmanager.php?type=T">Themes list</A></li>
<li><A HREF="listmanager.php?type=E">Extensions list</A></li>
<li><A HREF="usermanager.php">Users Manager</A></li>
<li><a href="commentsmanger.php?function=flaggedcomments">Comments Manager</a></li>
<?php
} else {
?>
<li><A HREF="usermanager.php?function=edituser&amp;userid=<?php echo"$_SESSION[uid]"; ?>">Your Profile</A></li>
<li><A HREF="approval.php">Approval Queue <?php if ($skipqueue != "true") { echo"($queuenum)"; } ?></A></li>
<li><A HREF="listmanager.php?type=T">Themes list</A></li>
<li><A HREF="listmanager.php?type=E">Extensions list</A></li>
<li><A HREF="usermanager.php">Users Manager</A></li>
<li><a href="appmanager.php">Application Manager</a></li>
<li><a href="categorymanager.php">Category Manager</A></li>
<li><a href="faqmanager.php">FAQ Manager</A></li>
<li><a href="commentsmanager.php?function=flaggedcomments">Comments Manager</a></li>
<?php } ?>
<li><a href="logout.php">Logout</A></li>
</ul>

	</div>
	<div id="mainContent">
