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
if (isset($_GET['date'])&&$_GET['date']!='null'&&ctype_alpha($_GET['date'])) {
    $clean['date'] = $_GET['date'];
}

// Application.
if (isset($_GET['app'])&&$_GET['app']!='null'&&ctype_alpha($_GET['app'])) {
    $clean['app'] = $_GET['app'];
}

// Query.
if (isset($_GET['q'])&&preg_match("/^[a-zA-Z0-9'\.-\s]*$/",$_GET['q'])) {
    $clean['q'] = $_GET['q'];
}

// Sort.
if (isset($_GET['sort'])&&ctype_alpha($_GET['sort'])) {
    $clean['sort'] = $_GET['sort'];
}

// Starting point.
$clean['left'] = (isset($_GET['left'])) ? intval($_GET['left']) : 0;

// Per page.
$_GET['perpage'] = (isset($_GET['perpage'])) ? intval($_GET['perpage']) : null;
switch ($_GET['perpage']) {
    case '10':
    default:
        $clean['perpage'] = 10;
        break;
    case '25':
        $clean['perpage'] = 25;
        break;
    case '50':
        $clean['perpage'] = 50;
        break;
}

// Ending point.
$clean['right'] = $clean['left'] + $clean['perpage'];


// Prepared verified inputs for their destinations.
foreach ($clean as $key=>$val) {
    $sql[$key] = mysql_real_escape_string($val);
}

// Instantiate AMO_Object so we can get our categories and platforms.
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
    'newest' => 'Newest'
);

$apps = array(
    'Firefox'     => 'Firefox',
    'Thunderbird' => 'Thunderbird',
    'Mozilla'     => 'Mozilla'
);

$perpage = array(
    10 => '10',
    25 => '25',
    50 => '50'
);


// Now we need to build our query.  Our query starts with four parts:

// Select and joins.
$select = "
    SELECT DISTINCT
        main.ID
    FROM
        main
";

// Where clause.
$where = "
    WHERE
";

// Order by.
$orderby = "
    ORDER BY
";

if (!empty($sql['platform'])||!empty($sql['app'])) {
    $select .= " INNER JOIN version ON version.ID = main.ID ";
}

if (!empty($sql['cat'])) {
    $select .= " INNER JOIN categoryxref ON categoryxref.ID = main.ID ";
    $where .= " categoryxref.CategoryID = '{$sql['cat']}' AND ";
}

if (!empty($sql['platform'])) {
    $where .= " version.OSID = '{$sql['platform']}' AND ";
}

if (!empty($sql['app'])) {
    $select .= " INNER JOIN applications ON version.AppID = applications.AppID ";
    $where .= " applications.AppName = '{$sql['app']}' AND ";
}

if (!empty($sql['q'])) {
    $where .= " main.Name LIKE '%{$sql['q']}%' AND ";
}

if (!empty($sql['date'])) {
    switch ($sql['date']) {
        case 'day':
            $compareTimestamp = strtotime('-1 day');
            break;
        case 'week':
            $compareTimestamp = strtotime('-1 week');
            break;
        case 'month':
            $compareTimestamp = strtotime('-1 month');
            break;
        case 'year':
            $compareTimestamp = strtotime('-1 year');
            break;
    }

    $compareDate = date('Y-m-d',$compareTimestamp);

    $where .= " main.DateUpdated > '{$compareDate}' AND ";

    unset($compareTimestamp);
    unset($compareDate);
}

if (!empty($sql['sort'])) {
    switch ($sql['sort']) {
        case 'name':
        default:
            $orderby .= " main.Name ASC";
            break;
        case 'rating':
            $orderby .= " main.Rating DESC";
            break;
        case 'downloads':
            $orderby .= " main.TotalDownloads DESC";
            break;
        case 'newest':
            $orderby .= " main.DateUpdated DESC";
            break;
    }
} else {
    $orderby .= " main.Name ASC ";
}

$where .= ' 1 ';

$query = $select.$where.$orderby;

$results = array();
$rawResults = array();

$db->query($query, SQL_ALL);

if (is_array($db->record)) {
    foreach ($db->record as $row) {
        $rawResults[] = $row[0]; 
    }
}

for ($i=$clean['left'];$i<$clean['right'];$i++) {
    if (isset($rawResults[$i])) {
        $results[] = new Addon($rawResults[$i]);
    }
}

$resultCount = count($rawResults);
if ($resultCount<$clean['right']) {
    $clean['right'] = $resultCount;
}

unset($select);
unset($where);
unset($orderby);
unset($query);

// Pass variables to template object.
$tpl->assign(
    array(
        'left'          => $clean['left']+1,
        'right'         => $clean['right'],
        'perpage'       => $clean['perpage'],
        'resultcount'   => $resultCount,
        'results'       => $results,
        'clean'         => $clean,
        'cats'          => $amo->Cats,
        'platforms'     => $amo->Platforms,
        'apps'          => $apps,
        'dates'         => $dates,
        'sort'          => $sort,
        'perpage'       => $perpage,
        'content'       => 'search.tpl',
        'title'         => 'Search'
    )
);

// Set a non-default wrapper.
$wrapper = 'inc/wrappers/nonav.tpl';
?>
