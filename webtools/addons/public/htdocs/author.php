<?php
/**
 * Author information.
 * @package amo
 * @subpackage docs
 */

// Get our addon ID.
$clean['UserID'] = intval($_GET['id']);
$sql['UserID'] =& $clean['UserID'];

$cacheId = $clean['UserID'];

startProcessing('author.tpl',$cacheId,$compileId);
require_once('includes.php');

$user = new User($sql['UserID']);

// Assign template variables.
$tpl->assign(
    array(  'user'     => $user,
            'title'     => $user->UserName,
            'content'   => 'author.tpl',
            'sidebar'   => 'inc/author-sidebar.tpl')
);
?>
