<?php
/**
 * Overview provides an inside look at what is going on for an application.
 *
 * @package amo
 * @subpackage docs
 *
 * @todo Do something to spice up this page.
 * @todo Get main template spruced up.
 */

$currentTab = 'home';

startProcessing('index.tpl', null, $compileId, 'nonav');
require_once('includes.php');

// If app is not set or empty, set it to null for our switch.
$_GET['app'] = (!empty($_GET['app'])) ? $_GET['app'] : null;

// Determine our application.
switch( $_GET['app'] ) {
    case 'mozilla':
        $clean['app'] = 'Mozilla';
        break;
    case 'thunderbird':
        $clean['app'] = 'Thunderbird';
        break;
    case 'firefox':
    default:
        $clean['app'] = 'Firefox';
        break;
}

// $sql['app'] can equal $clean['app'] since it was assigned in a switch().
// We have to ucfirst() it because the DB has caps.
$sql['app'] = $clean['app'];

$amo = new AMO_Object();

// Assign template variables.
$tpl->assign(
    array(  'popularExtensions' => $amo->getPopularAddons($sql['app'],'E',5),
            'feature'           => $amo->getFeature($sql['app']),
            'title'             => $clean['app'].' Addons',
            'currentTab'        => $currentTab)
);
?>
