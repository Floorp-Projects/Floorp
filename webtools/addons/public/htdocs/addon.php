<?php
/**
 * Addon summary page.  Displays a top-down view of all Addon properties.
 * 
 * @package amo
 * @subpackage docs
 */

// Get the int value of our addon ID.
$clean['ID'] = intval($_GET['id']);
$sql['ID'] =& $clean['ID'];

startProcessing('addon.tpl',$clean['ID'],$compileId);
require_once('includes.php');

// Create our AddOn object using the ID.
$addon = new AddOn($sql['ID']);

/* This is kind of a cheesy hack to determine how to display
   download links on the addon page.  If only one link is shown,
   there will just be an "Install Now" link, otherwise there will
   be links for each version. */
if (sizeof($addon->OsVersions) == 1) {
    $multiDownloadLinks = false;
} else {
    $multiDownloadLinks = true;
}

// Assign template variables.
$tpl->assign(
    array(  'addon'     => $addon,
            'multiDownloadLinks' => $multiDownloadLinks,
            'title'     => $addon->Name,
            'content'   => 'addon.tpl',
            'sidebar'   => 'inc/addon-sidebar.tpl')
);
?>
