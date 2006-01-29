<?php
/**
 * Create a new account
 *
 * @package amo
 * @subpackage docs
 *
 */

startProcessing('createaccount.tpl', null, null, 'nonav');
require_once 'includes.php';

// If there are problems, these will be set to true and used in the template.  By
// using null/booleans, error messages are kept in the template.
$error_email_empty = null;
$error_email_malformed = null;
$error_emailconfirm_empty = null;
$error_emailconfirm_nomatch = null;
$error_email_duplicate = null;
$error_name_empty = null;
$error_password_empty = null;
$error_passwordconfirm_empty = null;
$error_passwordconfirm_nomatch = null;

$_bad_input = false; // think positive :)
$account_created = false;

if (array_key_exists('submit', $_POST) && isset($_POST['submit'])) {
    /* Verify Input */
        // Check email - a little long and confusing.  Basically, throw an error if
        // the following is not met (in order):
        // $email is set, $emailconfirm is set, $email=$emailconfirm, and $email is a valid address
        if (!array_key_exists('email', $_POST) || empty($_POST['email'])) {
            $error_email_empty = true;
            $_bad_input = true;
        } else {
            if (!array_key_exists('emailconfirm', $_POST) || empty($_POST['emailconfirm'])) {
                $error_emailconfirm_empty = true;
                $_bad_input = true;
            } else {
                // technically this would catch if emailconfirm was empty to, but
                // waiting until here could make php throw a warning.
                if ($_POST['email'] != $_POST['emailconfirm']) {
                    $error_emailconfirm_nomatch = true;
                    $_bad_input = true;
                }
            }
            // Regex from Gavin Sharp -- thanks Gavin.
            if (!preg_match('/^(([A-Za-z0-9]+_+)|([A-Za-z0-9]+\-+)|([A-Za-z0-9]+\.+)|([A-Za-z0-9]+\++))*[A-Za-z0-9]+@((\w+\-+)|(\w+\.))*\w{1,63}\.[a-zA-Z]{2,6}$/',$_POST['email'])) {
                $error_email_malformed = true;
                $_bad_input = true;
            }
        }
        // name is required
        if (!array_key_exists('name', $_POST) || empty($_POST['name'])) {
            $error_name_empty = true;
            $_bad_input = true;
        }
        // password is required and match
        if (!array_key_exists('password', $_POST) || empty($_POST['password'])) {
            $error_password_empty = true;
            $_bad_input = true;
        } else {
            if (!array_key_exists('passwordconfirm', $_POST) || empty($_POST['passwordconfirm'])) {
                $error_passwordconfirm_empty = true;
                $_bad_input = true;
            } else {
                if ($_POST['password'] != $_POST['passwordconfirm']) {
                    $error_passwordconfirm_nomatch = true;
                    $_bad_input = true;
                }
            }
        }
        // This is a little out of order because we're trying to save a query.  If we
        // haven't had any bad input yet, do one last check to make sure the email
        // address isn't already in use.
        if ($_bad_input === false) {
            $_user_test = user::getUserByEmail($_POST['email']);

            if (is_object($_user_test)) {
                $_bad_input = true;
                $error_email_duplicate = true;
            }
        }

        // We're happy with the input, make a new account
        if ($_bad_input === false) {
            $_user_info = array();
            $_user_info['email']    = $_POST['email'];
            $_user_info['name']     = $_POST['name'];
            $_user_info['website']  = $_POST['website'];
            $_user_info['password'] = $_POST['password'];
            $user_id = user::addUser($_user_info);
            if ($user_id === false) {
                triggerError('There was an error processing your request.');
            }
            $user = new User($user_id[0]);
            // we're emailing them their plain text password
            $user->sendConfirmation($_user_info['password']);
            $account_created = true;
        }

}

// Pull values from POST to put back in the form
$email_value        = array_key_exists('email', $_POST) ? $_POST['email'] : '';
$emailconfirm_value = array_key_exists('emailconfirm', $_POST) ? $_POST['emailconfirm'] : '';
$name_value         = array_key_exists('name', $_POST) ? $_POST['name'] : '';
$website_value      = array_key_exists('website', $_POST) ? $_POST['website'] : '';

// Assign template variables.
$tpl->assign(
    array(  'title'                         => 'Create a Mozilla Addons Account',
            'currentTab'                    => null,
            'account_created'               => $account_created,
            'bad_input'                     => $_bad_input,
            'error_email_empty'             => $error_email_empty,
            'error_email_malformed'         => $error_email_malformed,
            'error_emailconfirm_empty'      => $error_emailconfirm_empty,
            'error_emailconfirm_nomatch'    => $error_emailconfirm_nomatch,
            'error_email_duplicate'         => $error_email_duplicate,
            'error_name_empty'              => $error_name_empty,
            'error_password_empty'          => $error_password_empty,
            'error_passwordconfirm_empty'   => $error_passwordconfirm_empty,
            'error_passwordconfirm_nomatch' => $error_passwordconfirm_nomatch,
            'email_value'                   => $email_value,
            'emailconfirm_value'            => $emailconfirm_value,
            'name_value'                    => $name_value,
            'website_value'                 => $website_value
            )
);
?>
