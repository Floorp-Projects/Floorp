<?php
require"core/sessionconfig.php";

session_unset();
session_destroy();

$return_path="developers/index.php?logout=true";

header("Location: http://$_SERVER[HTTP_HOST]/$return_path");
?>