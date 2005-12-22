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
if (empty($_GET['aid']) || empty($_GET['cid']) || empty($_GET['r'])) {
    triggerError('Missing required parameter(s).  Script cannot continue.');
}

// Get our addon ID.
if (isset($_GET['aid'])) {
    $clean['aid'] = intval($_GET['aid']);
    $sql['aid'] =& $clean['aid'];

    // Get addon
    $addon = new Addon($sql['aid']);
}

// Get our comment ID.
if (isset($_GET['cid'])) {
    $clean['cid'] = intval($_GET['cid']);
    $sql['cid'] =& $clean['cid'];

    $db->query("SELECT * FROM feedback WHERE CommentID = '{$sql['cid']}'", SQL_INIT, SQL_ASSOC);
    $comment = $db->record;
    $tpl->assign('comment',$comment);
}

// Get whether helpful or not...
if (isset($_GET['r'])) {
    switch ($_GET['r']) {
        case 'yes':
            $clean['r'] = 'yes';
            $clean['helpful'] = 'helpful';
            break;
        case 'no':
            $clean['r'] = 'no';
            $clean['helpful'] = 'not helpful';
            break;
    }
}

// If our form was submitted, try to process the results.
$success = $db->query("
    UPDATE
        feedback
    SET
        `helpful-{$clean['r']}` = `helpful-{$clean['r']}` + 1
    WHERE
        CommentID = {$sql['cid']}
", SQL_NONE);

if ($success) {
    $tpl->assign('success',true);
} else {
    triggerError('Query failed. Could not enter comment rating.');
}

$tpl->assign(
    array(  'title'     => 'Rate a Comment for ' . $addon->Name,
            'content'   => 'ratecomment.tpl',
            'addon'     => $addon,
            'clean'     => $clean,
            'sidebar'   => 'inc/addon-sidebar.tpl')
);
?>
