<?php
require"../core/config.php";
require"core/sessionconfig.php";

$password = md5($_POST[password]);
$sql = "SELECT DISTINCT `UserID`, `UserEmail`,`UserName`,`UserMode`,`UserTrusted` FROM `t_userprofiles` WHERE `UserEmail` = '$_POST[email]' && `UserPass` = '$password' LIMIT 1";
$sql_result = mysql_query($sql, $connection) or trigger_error("<FONT COLOR=\"#FF0000\"><B>MySQL Error ".mysql_errno().": ".mysql_error()."</B></FONT>", E_USER_NOTICE);
  $num = mysql_num_rows($sql_result);

if ($num == 1) {

$row = mysql_fetch_array($sql_result);

$userid=$row["UserID"];
$useremail=$row["UserEmail"];
$username=$row["UserName"];
$usermode=$row["UserMode"];
$usertrusted=$row["UserTrusted"];
$logoncheck="YES";

//Update LastLogin Time to current.
$sql = "UPDATE `t_userprofiles` SET `UserLastLogin`=NOW(NULL) WHERE `UserID`='$userid' LIMIT 1";
  $sql_result = mysql_query($sql, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);

//User Role to Session Level variable...
$rolearray = array("A"=>"admin", "E"=>"editor","U"=>"user");
$usermode = $rolearray[$usermode];

//Register Session Variables
$_SESSION["uid"] = "$userid";
$_SESSION["email"] = "$useremail";
$_SESSION["name"] = "$username";
$_SESSION["level"] = "$usermode";
$_SESSION["trusted"] = "$usertrusted";
$_SESSION["logoncheck"] = "$logoncheck";

$return_path="developers/main.php";

header("Location: http://$_SERVER[HTTP_HOST]/$return_path");
exit;


} else {
$return_path ="developers/index.php?login=failed";
header("Location: http://$_SERVER[HTTP_HOST]/$return_path");
exit;
}
?>