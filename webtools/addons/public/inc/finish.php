<?php
/**
 * Finish script.
 *
 * @package amo
 * @subpackage inc
 */

if (empty($pageType)) {
  $pageType = "default";
}

// Display our page header.
$tpl->display('inc/wrappers/' . $pageType . '-header.tpl');

// Display page content..
$tpl->display($content);

// Display our page footer.
$tpl->display('inc/wrappers/' . $pageType . '-footer.tpl');
?>
