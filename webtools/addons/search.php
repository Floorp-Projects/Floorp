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

// Category.
if (isset($_GET['cat'])&&ctype_digit($_GET['cat'])) {
    $clean['cat'] = intval($_GET['cat']);
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
if (isset($_GET['platform'])&&ctype_digit($_GET['platform'])) {
    $clean['platform'] = intval($_GET['platform']);
}

// Date.
if (isset($_GET['date'])&&ctype_alpha($_GET['date'])) {
    $clean['date'] = $_GET['date'];
}

// Application.
if (isset($_GET['app'])&&ctype_alpha($_GET['app'])) {
    $clean['app'] = $_GET['app'];
}

// Sort.
if (isset($_GET['sort'])&&ctype_digit($_GET['sort'])) {
    $clean['sort'] = $_GET['sort'];
}

// Query.
if (isset($_GET['q'])&&preg_match("/^[a-zA-Z0-9'\.-]*$/",$_GET['q'])) {
    $clean['q'] = $_GET['q'];
}

// Sort.
if (isset($_GET['sort'])&&ctype_alpha($_GET['sort'])) {
    $clean['sort'] = $_GET['sort'];
}

// Prepared verified inputs for their destinations.
foreach ($clean as $key=>$val) {
    $sql[$key] = mysql_real_escape_string($val);
}


$amo = new AMO_Object();

$amo->getCats();
$amo->getPlatforms();

$dates = array(
    'day'  => 'Today',
    'week' => 'This Week',
    'month'=> 'This Month',
    'year' => 'This Year'
);

$sort = array(
    'name'   => 'Name',
    'rating' => 'Rating',
    'downloads' => 'Popularity',
);

$apps = array(
    'Firefox'     => 'Firefox',
    'Thunderbird' => 'Thunderbird',
    'Mozilla'     => 'Mozilla'
);

$tpl->assign(
    array(
        'clean'     => $clean,
        'cats'      => $amo->Cats,
        'platforms' => $amo->Platforms,
        'apps'      => $apps,
        'dates'     => $dates,
        'sort'      => $sort,
        'content'   => 'search.tpl',
        'title'     => 'Search',
    )
);

$wrapper = 'inc/wrappers/nonav.tpl';
?>
