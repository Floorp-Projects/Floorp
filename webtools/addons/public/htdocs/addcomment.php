<?php
/**
 * Add a comment to any Addon.
 *
 * @package amo
 * @subpackage docs
 *
 * Variables:
 *   $_GET['id'] = Addon ID (integer)
 */

startProcessing('addcomment.tpl', null, null, 'nonav');
require_once 'includes.php';

session_start();

if ((!array_key_exists('id', $_GET)) || !is_numeric($_GET['id'])) {
    triggerError('There was an error processing your request.');
}

//This is a secure page, so we'll check the session
if (!$_auth->validSession()) {
    //id is already verified to be numeric from above
    header('Location: '.WEB_PATH."/login.php?dest=comment&id={$_GET['id']}");
    exit;
}

// If there are errors, this will be populated
$_errors     = array();

// They're posting a comment
if (isset($_POST['c_submit'])) {

    if (! (array_key_exists('c_rating', $_POST)
        && array_key_exists('c_title', $_POST)
        && array_key_exists('c_comments', $_POST))) {
        //This should never happen, but hey...
        triggerError('There was an error processing your request.');
    }


    $_c_rating   = mysql_real_escape_string($_POST['c_rating']);
    $_c_title    = mysql_real_escape_string($_POST['c_title']);
    $_c_comments = mysql_real_escape_string($_POST['c_comments']);

    // This is used in the template.  If 'true' is returned, an error will be
    // printed in the template (using booleans instead of strings here keeps the
    // error messages in the template).
    $_errors['c_rating']   = !is_numeric($_c_rating);
    $_errors['c_title']    = empty($_c_title);
    $_errors['c_comments'] = empty($_c_comments);

    foreach ($_errors as $error) {
        if ($error !== false) {
            $_bad_input = true;
        } else {
            $_bad_input = false;
        }
    }

    // If bad_input is true, we'll skip the rest of the processing and dump them
    // back out to the from with an error.
    if ($_bad_input === false) {

        // @todo this
        // Put it in the database
        // Drop significant stuff in the session
        // header() them to somewhere else
    }
}

$addon = new AddOn($_GET['id']);

// Put values back into the form - something went wrong (or they haven't hit 'submit' yet).
$c_rating_value   = array_key_exists('c_rating', $_POST)   ? $_POST['c_rating']   : '';
$c_title_value    = array_key_exists('c_title', $_POST)    ? $_POST['c_title']    : '';
$c_comments_value = array_key_exists('c_comments', $_POST) ? $_POST['c_comments'] : '';

// Assign template variables.
$tpl->assign(
    array(  'title'             => 'Add Comment',
            'currentTab'        => null,
            'rate_select_value' => array('','5','4','3','2','1','0'),
            'rate_select_name'  => array('Rating:','5 stars', '4 stars', '3 stars', '2 stars', '1 star', '0 stars'),
            'addon'             => $addon,
            'c_errors'          => $_errors,
            'c_rating_value'    => $c_rating_value,
            'c_title_value'     => $c_title_value,
            'c_comments_value'  => $c_comments_value
         )
);
?>
