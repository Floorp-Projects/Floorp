<?php
/**
 * Finish script.
 *
 * @package amo
 * @subpackage inc
 */

if (!isset($pageType))
  $pageType = "default";

// Set our wrapper if it has not been set.
//$wrapper = (!isset($wrapper)) ? 'inc/wrappers/default.tpl' : $wrapper;
$tpl->display('inc/wrappers/' . $pageType . '-header.tpl');
// Display output.
$tpl->display($content);

$tpl->display('inc/wrappers/' . $pageType . '-footer.tpl');
// Disconnect from our database.
//$db->disconnect();
?>
