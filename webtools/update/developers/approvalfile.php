<?php
require"../core/config.php";
$filename = basename($_SERVER["PATH_INFO"]);
$file = "$repositorypath/approval/$filename";
header('Content-Description: File Transfer');
header('Content-Type: application/octet-stream');
header('Content-Length: ' . filesize($file));
header('Content-Disposition: attachment; filename=' . basename($file));
readfile($file);
?>