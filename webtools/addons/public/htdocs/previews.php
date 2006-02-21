<?php
/**
 * Addon previews page.  Displays all the previews for a particular addon or extension.
 * 
 * @package amo
 * @subpackage docs
 */

// Get the int value of our addon ID.
$clean['ID'] = intval($_GET['id']);
$sql['ID'] =& $clean['ID'];

startProcessing('previews.tpl',$clean['ID'],$compileId);
require_once('includes.php');
 
$addon = new AddOn($sql['ID']);
$addon->getPreviews();

// Assign template variables.
$tpl->assign(
    array(  'addon'     => $addon,
            'title'     => $addon->Name.' Previews &amp; Screenshots',
            'content'   => 'previews.tpl',
            'sidebar'   => 'inc/addon-sidebar.tpl')
);
