<?php
// Close database connection.
$db->disconnect();

// If our $wrapper var is not set, fallback to default.
if (!isset($wrapper)) {
    $wrapper = 'inc/wrappers/default.tpl';
}

// Display output.
$smarty->display($wrapper);
?>
