<?php
//session_name('sid');
session_name();
session_start();

if ($_SESSION["logoncheck"] !=="YES" && strpos($_SERVER["SCRIPT_NAME"],"login.php")===false && strpos($_SERVER["SCRIPT_NAME"],"index.php")===false) {
$return_path="developers/index.php";
header("Location: http://$_SERVER[HTTP_HOST]/$return_path");
exit;
}
?>