<?php
/**
 * Addon summary page.  Displays a top-down view of all Addon properties.
 * 
 * @package amo
 * @subpackage docs
 */

startProcessing('addon.tpl');
require_once('includes.php');

// Arrays to store clean inputs.
$clean = array();  // General array for verified inputs.
$sql = array();  // Trusted for SQL.

// Get the int value of our addon ID.
$clean['ID'] = intval($_GET['id']);

// Since it is guaranteed to be just an int, we can reference it.
$sql['ID'] =& $clean['ID'];

// Create our AddOn object using the ID.
$addon = new AddOn($sql['ID']);

// Assign template variables.
$tpl->assign(
    array(  'addon'     => $addon,
            'title'     => $addon->Name,
            'content'   => 'addon.tpl',
            'sidebar'   => 'inc/addon-sidebar.tpl')
);
?>
