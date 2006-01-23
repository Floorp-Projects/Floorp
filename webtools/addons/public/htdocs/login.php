<?php
/**
 * Page that validates whether a user is logged in.  If they aren't, it will prompt
 * them for their username/pass
 *
 * @package amo
 * @subpackage docs
 *
 */

startProcessing('login.tpl', null, null, 'nonav');
require_once 'includes.php';

// When the template is drawn, if this isn't null, it will print out a "failure to
// authenticate, try again" message.
$login_error = null;

$valid_destinations = array ('comment' => WEB_PATH.'/addcomment.php');

if (!empty($_POST['username']) && !empty($_POST['password'])) {
    if ($_auth->authenticate($_POST['username'], $_POST['password'])) {
        session_start();
        $_auth->createSession();

        if (array_key_exists('dest', $_GET) && array_key_exists($_GET['dest'], $valid_destinations)) {
            $_next_page = $valid_destinations[$_GET['dest']];
        } else {
            triggerError('There was an error processing your request.');
        }

        /* Right now $_GET['id'] is needed for all pages, but potentially you could
         * login and not need it, so this should handle all cases. */
        if (array_key_exists('id', $_GET) && is_numeric($_GET['id'])) {
            $_addon = "?id={$_GET['id']}";
        } else {
            $_addon = '';
        }

        header("Location: {$_next_page}{$_addon}");
        exit;

    } else {
        $login_error = true;
    }
}

// Assign template variables.
$tpl->assign(
    array(  'title'       => 'Firefox Add-ons Login',
            'currentTab'  => null,
            'login_error' => $login_error
            )
);
?>
