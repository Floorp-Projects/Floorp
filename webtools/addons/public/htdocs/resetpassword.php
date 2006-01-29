<?php
/**
 * Page to reset passwords for existing accounts
 *
 * @package amo
 * @subpackage docs
 *
 */

startProcessing('resetpassword.tpl', null, null, 'nonav');
require_once 'includes.php';

if (! (array_key_exists('email', $_GET) && array_key_exists('code', $_GET)) ) {
    triggerError('There was an error processing your request.');
}

$user = user::getUserByEmail($_GET['email']);

if ($user === false) {
    // bad email address
    triggerError('There was an error processing your request.');
}

$authorized = $user->checkResetPasswordCode($_GET['email'], $_GET['code']);

if ($authorized === false) {
    // bad code
    triggerError('There was an error processing your request.');
}

$bad_input = false;
$success   = false;
if (array_key_exists('password', $_POST) 
    && array_key_exists('passwordconfirm', $_POST) 
    && !empty($_POST['password'])) {

    if ($_POST['password'] != $_POST['passwordconfirm']) {
        $bad_input = true;
    }

    if ($bad_input === false) {
        $user->setPassword($_POST['password']);
        $success = true;
    }
}

// Assign template variables.
$tpl->assign(
    array(  'title'       => 'Firefox Add-ons Password Recovery',
            'currentTab'  => null,
            'bad_input'   => $bad_input,
            'success'     => $success
            )
);
?>
