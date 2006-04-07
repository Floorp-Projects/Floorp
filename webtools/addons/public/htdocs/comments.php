<?php
/**
 * Comments listing for an addon.
 *
 * @package amo
 * @subpackage docs
 * @TODO Disallow comments for addon authors (authors should not be allowed to comment on their own addon).
 * @TODO Throttle comment entry.
 */

// Get our addon id.
$clean['id'] = intval($_GET['id']);
$sql['id'] =& $clean['id'];

// Sort.
if (isset($_GET['sort'])&&ctype_alpha($_GET['sort'])) {
    $clean['sort'] = $_GET['sort'];
}

// Starting point.
$page['left'] = (isset($_GET['left'])) ? intval($_GET['left']) : 0;

// Ending point.
$page['right'] = $page['left'] + 10;

// Order by.
$page['orderby'] = (!empty($_GET['orderby'])&&ctype_alpha($_GET['orderby'])) ? $_GET['orderby'] : "";

startProcessing('comments.tpl',$clean['id'],$compileId);
require_once('includes.php');

$addon = new AddOn($sql['id']);
$addon->getComments($page['left'],10,$page['orderby']);

// Get our result count.
$db->query("SELECT FOUND_ROWS()", SQL_INIT);
$resultCount = !empty($db->record) ? $db->record[0] : $page['right'];
if ($resultCount<$page['right']) {
    $page['right'] = $resultCount;
}

// Do we even have a next or previous page?
$page['previous'] = ($page['left'] >= 10) ? $page['left']-10 : null;
$page['next'] = ($page['left']+10 < $resultCount) ? $page['left']+10 : null;
$page['resultCount'] = $resultCount;
$page['leftDisplay'] = $page['left']+1;

// Build the URL based on passed arguments.
foreach ($clean as $key=>$val) {
    if (!empty($val)) {
        $buf[] = $key.'='.$val;
    }
}
$page['url'] = implode('&amp;',$buf);
unset($buf);

// Assign template variables.
$tpl->assign(
    array(  'addon'     => $addon,
            'title'     => $addon->Name.' Comments',
            'content'   => 'comments.tpl',
            'sidebar'   => 'inc/addon-sidebar.tpl',
            'page'      => $page)
);
?>
