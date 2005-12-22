<?php
/**
 * Author information.
 * @package amo
 * @subpackage docs
 */

// Arrays to store clean inputs.
$clean = array();  // General array for verified inputs.
$sql = array();  // Trusted for SQL.

// Get our addon ID.
$clean['UserID'] = intval($_GET['id']);
$sql['UserID'] =& $clean['UserID'];

$user = new User($sql['UserID']);

// Assign template variables.
$tpl->assign(
    array(  'user'     => $user,
            'title'     => $user->UserName,
            'content'   => 'author.tpl',
            'sidebar'   => 'inc/author-sidebar.tpl')
);
?>
