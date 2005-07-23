<?php
/**
 * Search page.  All searches are filtered through this page.
 *
 * @package amo
 * @subpackage docs
 */

// Array to store clean inputs.
$clean = array();
$sql = array();
$html = array();

// Category.
if (isset($_GET['cat'])&&ctype_alpha($_GET['cat'])) {
    $clean['cat'] = $_GET['cat'];
}

// Type.
if (isset($_GET['type'])) {
    switch ($_GET['type']) {
        case 'T':
            $clean['type'] = 'T';
            break;
        case 'E':
            $clean['type'] = 'E';
            break;
    }
}

// Platform.
if (isset($_GET['platform'])&&ctype_alpha($_GET['platform'])) {
    $clean['platform'] = $_GET['platform'];
}

// Date.
if (isset($_GET['date'])&&ctype_alnum($_GET['date'])) {
    $clean['date'] = strtotime($_GET['date']);
}

// Application.
if (isset($_GET['app'])&&ctype_alpha($_GET['app'])) {
    $clean['app'] = $_GET['app'];
}

// Sort.
if (isset($_GET['sort'])&&ctype_alpha($_GET['sort'])) {
    $clean['sort'] = $_GET['sort'];
}

// Query.
if (isset($_GET['q'])&&preg_match("/^[a-zA-Z0-9'\.-]*$/",$_GET['q'])) {
    $clean['q'] = $_GET['q'];
}

// Prepared verified inputs for their destinations.
foreach ($clean as $key=>$val) {
    $html[$key] = strip_tags(htmlentities($val));
    $sql[$key] = mysql_real_escape_string($val);
}

// Format inputs for 

$tpl->assign(
    array(
        'content'    =>'search.tpl',
        'title'     =>'Search',
    )
);

$wrapper = 'inc/wrappers/nonav.tpl';
?>
