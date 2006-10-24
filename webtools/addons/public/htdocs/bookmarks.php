<?php
/**
 * Home page for extensions, switchable on application.
 *
 * @package amo
 * @subpackage docs
 * @todo make this dynamic based on an SQL field (recommended)
 */

startProcessing('bookmarks.tpl', 'bookmarks', $compileId, 'rustico');
require_once('includes.php');

setApp();

$amo = new AMO_Object();

$primary = $amo->getAddons(array(3615));
$primary = $primary[0];

$other = $amo->getAddons(array(2410, 1833));

// Assign template variables.
$tpl->assign(
    array(  'primary'           => $primary,
            'other'             => $other,
            'content'           => 'bookmarks.tpl',
            'currentTab'        => 'bookmarks')
);
?>
