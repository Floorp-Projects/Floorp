<?php
/**
 * Home page for extensions, switchable on application.
 *
 * @package amo
 * @subpackage docs
 * @todo make this dynamic based on an SQL field (recommended)
 */

startProcessing('recommended.tpl', 'recommended', $compileId, 'rustico');
require_once('includes.php');

setApp();

$amo = new AMO_Object();

// Assign template variables.
$tpl->assign(
    array(  'recommended'       => $amo->getRecommendedAddons($sql['app'],'E',100),
            'content'           => 'recommended.tpl',
            'currentTab'        => 'recommended')
);
?>
