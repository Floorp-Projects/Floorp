<?php
/**
 * Comments listing for an addon.
 *
 * @package amo
 * @subpackage docs
 */

// Get our addon ID.
$clean['ID'] = intval($_GET['id']);
$sql['ID'] =& $clean['ID'];

startProcessing('comments.tpl',$clean['ID'],$compileId);
require_once('includes.php');

$addon = new AddOn($sql['ID']);
$addon->getComments();

// Assign template variables.
$tpl->assign(
    array(  'addon'     => $addon,
            'title'     => $addon->Name.' Comments',
            'content'   => 'comments.tpl',
            'sidebar'   => 'inc/addon-sidebar.tpl')
);
?>
