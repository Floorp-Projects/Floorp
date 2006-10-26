<?php
/**
 * Plugin listing that shows available plugins for Firefox.
 * 
 * @package amo
 * @subpackage docs
 */

$currentTab = 'plugins';
startProcessing('plugins.tpl',$memcacheId,$compileId,'rustico');

// Assign template variables.
$tpl->assign(
    array(  'currentTab'=> $currentTab,
            'title'     => 'Plugins',
            'content'   => 'plugins.tpl',
            'sidebar'   => 'inc/plugin-sidebar.tpl',
            'currentTab'=> 'plugins')
);
?>
