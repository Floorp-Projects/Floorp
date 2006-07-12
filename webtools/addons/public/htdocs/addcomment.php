<?php
/**
 * Add a comment to any Addon.
 *
 * @package amo
 * @subpackage docs
 *
 * Variables:
 *   $_GET['aid'] = Addon ID (integer)
 */

startProcessing('addcomment.tpl', null, null);
require_once 'includes.php';

session_start();

if ((!array_key_exists('aid', $_GET)) || !is_numeric($_GET['aid'])) {
    triggerError('There was an error processing your request.');
}

//This is a secure page, so we'll check the session
if (!$_auth->validSession()) {
    //id is already verified to be numeric from above
    header('Location: '.WEB_PATH."/login.php?dest=comment&aid={$_GET['aid']}");
    exit;
}

// If there are errors, this will be populated
$_errors = array();

// This will be used in queries and the template
$addon = new AddOn($_GET['aid']);

// If the comment is added successfully, this will toggle (used in the template)
$added_comment = false;

// They're posting a comment
if (isset($_POST['c_submit'])) {

    if (! (array_key_exists('c_rating', $_POST)
        && array_key_exists('c_title', $_POST)
        && array_key_exists('c_comments', $_POST))) {
        //This should never happen, but hey...
        triggerError('There was an error processing your request.');
    }

    // Check all our input to make sure something is there, and it is appropriate.
    // If it isn't, make $_bad_input=true which means we'll print the form back out
    // with an error message.  (By using booleans here, we keep the error messages in
    // the .tpl)
        $_bad_input = false;
        if (!is_numeric($_POST['c_rating'])) {
            $_errors['c_rating'] = true;
            $_bad_input = true;
        }
        if (empty($_POST['c_title'])) {
            $_errors['c_title'] = true;
            $_bad_input = true;
        }
        if (empty($_POST['c_comments'])) {
            $_errors['c_comments'] = true;
            $_bad_input = true;
        }

    // If bad_input is true, we'll skip the rest of the processing and dump them
    // back out to the from with an error.
    if ($_bad_input === false) {

        // I got a little carried away with the escaping, but it's not gonna hurt anything.
        $_c_id         = mysql_real_escape_string($addon->ID);
        $_c_user_id    = mysql_real_escape_string($_auth->getId());
        $_c_rating     = mysql_real_escape_string($_POST['c_rating']);
        $_c_title      = mysql_real_escape_string(strip_tags($_POST['c_title']));
        $_c_comments   = mysql_real_escape_string(strip_tags($_POST['c_comments']));
        $_c_commentip  = mysql_real_escape_string($_SERVER['REMOTE_ADDR']);

        $_sql = "INSERT INTO `feedback`
                 ( 
                    `ID`,
                    `UserId`,
                    `CommentVote`,
                    `CommentTitle`,
                    `CommentNote`,
                    `CommentDate`,
                    `commentip`
                 ) VALUES (
                     {$_c_id},
                     {$_c_user_id},
                     {$_c_rating},
                    '{$_c_title}',
                    '{$_c_comments}',
                    NOW(),
                    '{$_c_commentip}'
                 )";

        $db->query($_sql);

        if (!DB::isError($db->record)) {
            // Calculate the lookup value in main for comment avg if our INSERT was successful.
            $_ratingSql = "UPDATE `main` SET `Rating` = ROUND((SELECT AVG(`CommentVote`) FROM `feedback` WHERE `ID` = {$_c_id}),2) WHERE `ID` = {$_c_id}";
            $db->query($_ratingSql);
        }

        // For the template
        $added_comment = true;
    }
}

// Put values back into the form - if something went wrong this will populate the
// form again
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
            'c_added_comment'   => $added_comment,
            'c_errors'          => $_errors,
            'c_rating_value'    => $c_rating_value,
            'c_title_value'     => $c_title_value,
            'c_comments_value'  => $c_comments_value,
            'sidebar'           => 'inc/addon-sidebar.tpl'
         )
);
?>
