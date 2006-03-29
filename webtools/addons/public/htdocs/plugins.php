<?php
/**
 * Plugin listing that shows available plugins for Firefox.
 * 
 * @package amo
 * @subpackage docs
 */

$currentTab = 'plugins';
startProcessing('plugins.tpl',$memcacheId,$compileId,'nonav');

// Assign template variables.
$tpl->assign(
    array(  'currentTab'=> $currentTab,
            'title'     => 'Plugins',
            'content'   => 'plugins.tpl')
);
?>
