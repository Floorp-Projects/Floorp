<?php
/**
 * Addon history page.  Displays all the previous releases for a particular
 * addon or theme
 * 
 * @package amo
 * @subpackage docs
 *
 * @todo break this into a simpler design, probably a smaller table with an abbreviated desc.
 * @todo do we still want to allow users access to old versions?
 */
 
// Arrays to store clean inputs.
$clean = array();  // General array for verified inputs.
$sql = array();  // Trusted for SQL.

// Get our addon ID.
$clean['ID'] = intval($_GET['id']);
$sql['ID'] =& $clean['ID'];

$addon = new AddOn($sql['ID']);
$addon->getHistory();

// Assign template variables.
$tpl->assign(
    array(  'addon'     => $addon,
            'title'     => $addon->Name.' Version History',
            'content'   => 'history.tpl',
            'sidebar'   => 'inc/addon-sidebar.tpl')
);
?>
