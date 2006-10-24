<?php
/**
 * Support page.
 * @package amo
 * @subpackage docs
 */
startProcessing('support.tpl',$memcacheId,$compileId,'rustico');
$tpl->assign(
    array(  'title' => 'Support',
            'content' => 'support.tpl')
);
?>
