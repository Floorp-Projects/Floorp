<?php
/**
 * Finish script.
 *
 * @package amo
 * @subpackage inc
 */

// Set our wrapper if it has not been set.
$wrapper = (!isset($wrapper)) ? 'inc/wrappers/default.tpl' : $wrapper;

// Display output.
$tpl->display($wrapper);

// Disconnect from our database.
//$db->disconnect();
?>
