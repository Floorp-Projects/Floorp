<?php
/**
 * Search page.  All searches are filtered through this page.
 *
 * @package amo
 * @subpackage docs
 */

startProcessing('search.tpl',$memcacheId,$compileId,'nonav');
require_once('includes.php');

// Instantiate AMO_Object so we can get our categories and platforms.
$amo = new AMO_Object();
$appList = $amo->getApps();

$appSelectList = array();
foreach ($appList as $id=>$name) {
    $appSelectList[strtolower($name)] = $name; 
}

// Array to store our page information.
$page = array();

// App.
$clean['app'] = isset($app) ? $app : 'firefox';

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
if (isset($_GET['appfilter'])&&$_GET['appfilter']!='null'&&(ctype_alpha($_GET['appfilter'])||is_numeric($_GET['appfilter']))) {

    $clean['appfilter'] = $_GET['appfilter'];

    if (is_numeric($clean['appfilter'])) {
        foreach ($appList as $id=>$name) {
            if ($clean['appfilter'] == $id) {
                $clean['appfilter'] = $name;
                break;
            }
        }
    }


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
$page['left'] = (isset($_GET['left'])) ? intval($_GET['left']) : 0;

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
$page['right'] = $page['left'] + $clean['perpage'];


// Prepared verified inputs for their destinations.
foreach ($clean as $key=>$val) {
    $sql[$key] = mysql_real_escape_string($val);
}

$dates = array(
    'day'  => 'Today',
    'week' => 'This Week',
    'month'=> 'This Month',
    'year' => 'This Year'
);

$sort = array(
    'newest' => 'Newest',
    'name'   => 'Name',
    'rating' => 'Rating',
    'downloads' => 'Popularity'
);

$perpage = array(
    10 => '10',
    25 => '25',
    50 => '50'
);


// Now we need to build our query.  Our query starts with four parts:

// Select top.
$selectTop = "
    SELECT DISTINCT SQL_CALC_FOUND_ROWS
        main.ID
    FROM
        main
";

// Select joins.
$select = "";

// Where clause.
$where = "
    WHERE
        version.approved = 'YES' AND
";

// Order by.
$orderby = "
    ORDER BY
";

$select .= " INNER JOIN version ON version.ID = main.ID ";

if (!empty($sql['cat'])) {
    $select .= " INNER JOIN categoryxref ON categoryxref.ID = main.ID ";
    $where .= " categoryxref.CategoryID = '{$sql['cat']}' AND ";
}

if (!empty($sql['platform'])) {
    // OSID=1 is 'ALL' in the db
    $where .= " (version.OSID = '{$sql['platform']}' OR version.OSID = '1' )AND ";
}

if (!empty($sql['appfilter'])) {
    $select .= " INNER JOIN applications ON version.AppID = applications.AppID ";

    $where .= " applications.AppName = '{$sql['appfilter']}' AND ";
}

if (!empty($sql['q'])) {
    $where .= " (main.name LIKE '%" . preg_replace('/[\s]+/','%',$sql['q']) . "%'
                OR main.Description LIKE '%" . preg_replace('/[\s]+/','%',$sql['q']) . "%') AND ";
}

if (!empty($sql['type'])) {
    $where .= " main.Type = '{$sql['type']}' AND ";
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
        case 'newest':
        default:
            $orderby .= " main.DateUpdated DESC";
            break;
        case 'name':
            $orderby .= " main.Name ASC";
            break;
        case 'rating':
            $orderby .= " main.Rating DESC, main.Name ASC";
            break;
        case 'downloads':
            $orderby .= " main.downloadcount DESC, main.Rating DESC";
            break;
    }
} else {
    $orderby .= " main.DateUpdated DESC ";
}

$where .= ' 1 ';

$limit = " LIMIT {$page['left']}, {$sql['perpage']} ";

$query = $selectTop.$select.$where.$orderby.$limit;

$results = array();
$rawResults = array();

// Get results.
$db->query($query, SQL_ALL);

if (is_array($db->record)) {
    foreach ($db->record as $row) {
        $rawResults[] = $row[0]; 
    }
}

// Get our result count.
$db->query("SELECT FOUND_ROWS()", SQL_INIT);
if (!empty($db->record)) {
    $resultCount = $db->record[0];
}
if ($resultCount<$page['right']) {
    $page['right'] = $resultCount;
}



// If we have only one result, redirect to the addon page.
if ( $resultCount == 1) {
    header('Location: '.HTTP_HOST.WEB_PATH.'/'.$clean['app'].'/'.$rawResults[0].'/');
    exit;
} else {
    foreach($rawResults as $id) {
        $results[] = new Addon($id);
    }
}

// Do we even have a next or previous page?
$page['previous'] = ($page['left'] >= $clean['perpage']) ? $page['left']-$clean['perpage'] : null;
$page['next'] = ($page['left']+$clean['perpage'] < $resultCount) ? $page['left']+$clean['perpage'] : null;
$page['resultCount'] = $resultCount;
$page['leftDisplay'] = $page['left']+1;

// Build the URL based on passed arguments.
foreach ($clean as $key=>$val) {
    if (!empty($val)) {
        $buf[] = $key.'='.$val;
    }
}
$page['url'] = implode('&amp;',$buf);
unset($buf);

// Pass variables to template object.
$tpl->assign(
    array(
        'page'          => $page,
        'results'       => $results,
        'clean'         => $clean,
        'cats'          => $amo->getCats(),
        'platforms'     => $amo->getPlatforms(),
        'apps'          => $appSelectList,
        'dates'         => $dates,
        'sort'          => $sort,
        'perpage'       => $perpage,
        'content'       => 'search.tpl',
        'title'         => 'Search',
        'extraHeaders'  => '<script src="'.WEB_PATH.'/js/search.js" type="text/javascript"></script>'
    )
);
?>
