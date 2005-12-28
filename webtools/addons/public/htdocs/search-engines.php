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

startProcessing('search-engines.tpl','nonav');
require_once('includes.php');

// Assign template variables.
$tpl->assign(
    array(  'title'             => 'Search Engines',
            'currentTab'        => 'search-engines',
            'content'           => 'search-engines.tpl')
);

$wrapper = 'inc/wrappers/nonav.tpl';
?>
