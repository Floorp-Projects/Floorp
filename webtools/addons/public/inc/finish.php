<?php
/**
 * Finish script.
 *
 * @package amo
 * @subpackage inc
 */

// If we do not have a pageType, we need to set one.
if (empty($pageType)) {
  $pageType = "default";
}

// Display our page header.
//
// $currentTab is set in init.php or the calling script,
//  and corresponds to the selected tab in the header.tpl template.
//
// $compileId is set in init.php and corresponds to the current app.
$tpl->display('inc/wrappers/' . $pageType . '-header.tpl', $GLOBALS['currentTab'], $GLOBALS['compileId']);

// Display page content.
//
// $cacheId is set in init.php or the colling script.  It
//  is unique for any number of combinations of parameters (GET, typically).
//
// $compileId is set in init.php and corresponds to the current app.
$tpl->display($content,$cacheId,$compileId);

// Display our page footer.  We do not pass it a cacheId or compileId
//  because the footer doesn't change based on parameters.
$tpl->display('inc/wrappers/' . $pageType . '-footer.tpl');
?>
