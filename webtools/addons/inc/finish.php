<?php
/**
 * Finish script.
 */

// Disconnect from our database.
$db->disconnect();

// Set our wrapper if it has not been set.
$wrapper = (!isset($wrapper)) ? 'inc/wrappers/nonav.tpl' : $wrapper;

// Display output.
$tpl->display($wrapper);
