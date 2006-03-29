<?php
/**
 * Plugin listing that shows available plugins for Firefox.
 * 
 * @package amo
 * @subpackage docs
 */

startProcessing('plugins.tpl',$memcacheId,$compileId,'nonav');

// Assign template variables.
$tpl->assign(
    array(  'title'     => 'Plugins',
            'content'   => 'plugins.tpl')
);
?>
