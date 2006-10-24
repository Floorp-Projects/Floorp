<?php
/**
 * Overview provides an inside look at what is going on for an application.
 *
 * @package amo
 * @subpackage docs
 */

$currentTab = 'home';

setApp();
$pageType = ($clean['app'] === 'Firefox' ? 'rustico' : 'nonav');
startProcessing('index.tpl', 'home', $compileId, $pageType);
require_once('includes.php');

$amo = new AMO_Object();

header("Cache-Control: max-age=120, must-revalidate");
// Assign template variables.
$tpl->assign(
    array(  'popularExtensions' => $amo->getPopularAddons($sql['app'],'E',5),
            'feature'           => $amo->getFeature($sql['app']),
            'title'             => $clean['app'].' Addons',
            'currentTab'        => $currentTab,
            'app'               => $sql['app'])
);
?>
