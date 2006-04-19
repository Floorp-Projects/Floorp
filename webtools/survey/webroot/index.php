<?php
/**
 * Uninstall survey.
 * @package survey
 * @subpackage docs
 */
$sql['product'] = !empty($_POST['product'])?mysql_real_escape_string($_POST['product']):(!empty($_GET['product'])?mysql_real_escape_string($_GET['product']):null);
$sql['product_id'] = $app->getAppIdByName($sql['product']);

// If no matching product is found, redirect them to the support page.
if (empty($sql['product_id'])) {
   header('Location: http://www.mozilla.com/support/');
   exit;
}

$intends = $app->getIntends($sql['product_id']);
$issues = $app->getIssues($sql['product_id']);

/**
 * If the user has submitted, process and complete the transaction.
 */
if (!empty($_POST['submit'])) {

// The easy stuff.
$sql['useragent'] = !empty($_POST['useragent'])?mysql_real_escape_string($_POST['useragent']):null;
$sql['http_user_agent'] = mysql_real_escape_string($_SERVER['HTTP_USER_AGENT']);
$sql['intend_id'] = !empty($_POST['intend_id'])?mysql_real_escape_string($_POST['intend_id']):null;
$sql['intend_text'] = !empty($_POST['intend_text'])?mysql_real_escape_string($_POST['intend_text']):null;
$sql['comments'] = !empty($_POST['comments'])?mysql_real_escape_string($_POST['comments']):null;

// For each issue, we need to log the other text.
$sql['issue_id'] = array();
if (!empty($_POST['issue_id']) && is_array($_POST['issue_id'])) {
    foreach ($_POST['issue_id'] as $issue_id) {
        $sql['issue_id'][mysql_real_escape_string($issue_id)] = !empty($_POST[$issue_id.'_text'])?mysql_real_escape_string($_POST[$issue_id.'_text']):'';
    }
}

// Result record.
$query = "
    INSERT INTO
        result(intend_id, intend_text, product, useragent, http_user_agent, comments, date_submitted)
    VALUES(
        '{$sql['intend_id']}', '{$sql['intend_text']}', '{$sql['product']}', '{$sql['useragent']}', '{$sql['http_user_agent']}', '{$sql['comments']}', NOW()
    );\n
";
$db->query($query, SQL_NONE);

if (!empty($sql['issue_id']) && count($sql['issue_id']) > 0) {
    foreach ($sql['issue_id'] as $id => $text) {
        $db->query("INSERT INTO issue_result_map() VALUES(LAST_INSERT_ID(), '{$id}', '{$text}')", SQL_NONE);
    }
}

// Redirect to thank you page, and we're done.
require_once('./thanks.php');
exit;



/**
 * If we haven't submitted, show the form by default.
 */
} else {
    switch ($sql['product']) {
        case 'Mozilla Thunderbird':
            require_once('../tpl/thunderbird.php');
            break;

        case 'Mozilla Firefox':
        default:
            require_once('../tpl/firefox.php');
            break;
    }
}
?>
