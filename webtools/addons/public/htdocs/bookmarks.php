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

$primary = $amo->getRecommendedAddons($sql['app'],'E',1);
$primary = $primary[0];

// Assign template variables.
$tpl->assign(
    array(  'primary'           => $primary,
            'other'             => $amo->getRecommendedAddons($sql['app'],'E',3),
            'content'           => 'bookmarks.tpl',
            'currentTab'        => 'bookmarks')
);
?>
