<?php
/**
 * Verify a newly created account
 *
 * @package amo
 * @subpackage docs
 *
 */

startProcessing('verifyaccount.tpl', null, null, 'nonav');
require_once 'includes.php';

if (! (array_key_exists('email', $_GET) && array_key_exists('confirmationcode', $_GET)) ) {
    triggerError('There was an error processing your request.');
}

$user = user::getUserByEmail($_GET['email']);

// Most likely not a valid email
if ($user===false) {
    triggerError('There was an error processing your request.');
}

$confirmed = $user->confirm($_GET['confirmationcode']);


// Assign template variables.
$tpl->assign(
    array(  'title'      => 'Verify your Mozilla Addons Account',
            'currentTab' => null,
            'email'      => $_GET['email'],
            'confirmed'  => $confirmed
            )
);
?>
