<?php
/**
 * This will delete a session if the user has one.
 *
 * @package amo
 * @subpackage docs
 *
 */

startProcessing('logout.tpl', null, null, 'nonav');
require_once 'includes.php';

session_start();

session_destroy();

// Assign template variables.
$tpl->assign(
    array(  'title'             => 'Firefox Add-ons',
            'currentTab'        => null
            )
);
?>
