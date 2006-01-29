<?php
/**
 * Page to recover passwords for existing accounts
 *
 * @package amo
 * @subpackage docs
 *
 */

startProcessing('recoverpassword.tpl', null, null, 'nonav');
require_once 'includes.php';

$bad_input = false;
$success   = false;
if (array_key_exists('email', $_POST) && !empty($_POST['email'])) {
    $user = user::getUserByEmail($_POST['email']);
    if ($user === false) {
        // bad email address
        $bad_input = true;
    } else {
        $user->generateConfirmationCode();
        $user->sendPasswordRecoveryEmail();
        $success = true;
    }
}

$email_value = array_key_exists('email', $_POST) ? $_POST['email'] : '';

// Assign template variables.
$tpl->assign(
    array(  'title'       => 'Firefox Add-ons Password Recovery',
            'currentTab'  => null,
            'email'       => $email_value,
            'bad_input'   => $bad_input,
            'success'     => $success
            )
);
?>
