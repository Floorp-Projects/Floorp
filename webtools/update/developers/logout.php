<?php
require"core/sessionconfig.php";

session_unset();
session_destroy();

$return_path="developers/index.php?logout=true";

header("Location: http://$_SERVER[SERVER_NAME]/$return_path");
?>