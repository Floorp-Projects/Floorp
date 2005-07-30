<?php
/**
 * Rate comment for Adddon.
 * 
 * @package amo
 * @subpackage docs
 */

// Arrays to store clean inputs.
$clean = array();  // General array for verified inputs.
$sql = array();  // Trusted for SQL.

// If some of the inputs don't exist, throw an error and exit
if (!isset($_GET['aid']) || !isset($_GET['cid']) || !isset($_GET['r'])) {
    commentError();
}

// Get our addon ID.
if (isset($_GET['aid'])) {
    $clean['aid'] = intval($_GET['aid']);
    $sql['aid'] =& $clean['aid'];
}

// Get our comment ID.
if (isset($_GET['cid'])) {
    $clean['cid'] = intval($_GET['cid']);
    $sql['cid'] =& $clean['cid'];
}

// Get whether helpful or not...
if (isset($_GET['r'])) {
    switch ($_GET['r']) {
        case 'yes':
            $clean['r'] = 'yes';
            break;
        case 'no':
            $clean['r'] = 'no';
            break;
        default:
            commentError();
    }
}

// Get addon
$addon = new Addon($sql['aid']);

$success = $db->query("
    UPDATE
        feedback
    SET
        `helpful-{$clean['r']}` = `helpful-{$clean['r']}` + 1
    WHERE
        CommentID = {$sql['cid']}
", SQL_NONE);

$tpl->assign(
    array(  'title'     => 'Rate a Comment for ' . $addon->Name,
            'content'   => 'ratecomment.tpl',
            'rate'      => $success,
            'addon'     => $addon,
            'sidebar'   => 'inc/addon-sidebar.tpl')
);

function commentError() {
    global $tpl;
    $tpl->assign(
    array(  'title'     => 'Rate a Comment',
            'content'   => 'ratecomment.tpl',
            'error'     => true)
    );
    $tpl->display('inc/wrappers/nonav.tpl');
    exit;
}
?>
