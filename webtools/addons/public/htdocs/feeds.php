<?php
/**
 * Home page for extensions, switchable on application.
 *
 * @package amo
 * @subpackage docs
 */

startProcessing('feeds.tpl', null, $compileId,'nonav');
require_once('includes.php');

// Assign template variables.
$tpl->assign(
    array(  'title'             => 'Feeds',
            'content'           => 'feeds.tpl')
);
?>
