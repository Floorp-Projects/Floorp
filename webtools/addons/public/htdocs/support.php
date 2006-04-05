<?php
/**
 * Support page.
 * @package amo
 * @subpackage docs
 */
startProcessing('support.tpl',$memcacheId,$compileId,'nonav');
$tpl->assign(
    array(  'title' => 'Support',
            'content' => 'support.tpl')
);
?>
