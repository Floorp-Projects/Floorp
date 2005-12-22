<?php
/**
 * Addon summary page.  Displays a top-down view of all Addon properties.
 * 
 * @package amo
 * @subpackage docs
 */

startProcessing('addon.tpl');

// Arrays to store clean inputs.
$clean = array();  // General array for verified inputs.
$sql = array();  // Trusted for SQL.

// Get our addon ID.
$clean['ID'] = intval($_GET['id']);
$sql['ID'] =& $clean['ID'];

$addon = new AddOn($sql['ID']);

// Assign template variables.
$tpl->assign(
    array(  'addon'     => $addon,
            'title'     => $addon->Name,
            'content'   => 'addon.tpl',
            'sidebar'   => 'inc/addon-sidebar.tpl')
);
?>
