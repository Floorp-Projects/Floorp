<?php
/**
 * Comments listing for an addon.
 *
 * @package amo
 * @subpackage docs
 */

// Arrays to store clean inputs.
$clean = array();  // General array for verified inputs.
$sql = array();  // Trusted for SQL.

// Get our addon ID.
$clean['ID'] = intval($_GET['id']);
$sql['ID'] =& $clean['ID'];

$addon = new AddOn($sql['ID']);
$addon->getComments('50');

// Assign template variables.
$tpl->assign(
    array(  'addon'     => $addon,
            'title'     => $addon->Name.' Comments',
            'content'   => 'comments.tpl',
            'sidebar'   => 'inc/addon-sidebar.tpl')
);
?>
