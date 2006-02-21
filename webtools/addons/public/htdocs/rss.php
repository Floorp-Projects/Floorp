<?php
/**
 * Syndication for commonly referenced lists.
 *
 * @package amo
 * @subpackage docs
 */



/**
 * CHECK CACHE
 *
 * Check to see if we already have a matching cacheId.
 * If it exists, we can pull from it and exit; and avoid recompiling.
 */
// Determine a cacheId based on params.
$cacheId = md5($_SERVER['QUERY_STRING']);
 
$tpl = new AMO_Smarty();

// Set our cache timeout to 2 hours, which is reasonable.
$tpl->caching = 1;
$tpl->cache_timeout = 7200;

// Determine our cacheId based on the RSS feed's arguments.

if ($tpl->is_cached('rss.tpl',$cacheId)) {
    header('Content-Type: text/xml; charset=utf-8');
    $tpl->display('rss.tpl',$cacheId);
    exit;
}



/**
 * Pull our input params.
 */
$rssapp = (!empty($_GET['app']) && ctype_alpha($_GET['app'])) ? $_GET['app'] : null;
switch (strtolower($rssapp)) {
    case 'mozilla':
        $clean['app'] = 'mozilla';
        break;
    case 'thunderbird':
        $clean['app'] = 'thunderbird';
        break;
    case 'firefox':
    default:
        $clean['app'] = 'firefox';
        break;
}

$rsstype = !empty($_GET['type']) && ctype_alpha($_GET['type']) ? $_GET['type'] : null;
switch (strtolower($rsstype)) {
    case 'themes':
    case 't':
        $clean['type'] = 't';
        break;
    case 'extensions':
    case 'e':
    default:
        $clean['type'] = 'e';
        break;
}

$rsslist = !empty($_GET['list']) && ctype_alpha($_GET['list']) ? $_GET['list'] : null;
switch (strtolower($rsslist)) {
    case 'popular':
        $rssOrderBy = 'm.downloadcount desc, m.totaldownloads desc, m.rating desc, m.dateupdated desc, m.name asc';
        break;
    case 'updated':
        $rssOrderBy = 'm.dateupdated desc, m.name asc';
        break;
    case 'rated':
        $rssOrderBy = 'm.rating desc, m.downloadcount desc, m.name asc';
        break;
    case 'newest':
    default:
        /**
         * @TODO change this to dateapproved once the db has this in it.
         */
        $rssOrderBy = 'm.dateupdateddesc, m.name asc';
        break;
}

unset($rssapp);
unset($rsstype);
unset($rsslist);



// If we get here, we're going to have to pull DB contents.
require_once('includes.php');

// Build query for RSS data.
$sql['app'] = $clean['app']; // Already ok for sql, type was checked (alpha).
$sql['type'] = $clean['type']; // Already ok for sql, type was checked (alpha).

$_rssSql = "
    SELECT
        m.id,
        m.name as title,
        m.type,
        m.description,
        v.version,
        v.vid,
        v.dateupdated,
        a.appname
    FROM
        main m
    INNER JOIN version v ON v.id = m.id
    INNER JOIN applications a ON a.appid = v.appid
    WHERE
        v.approved = 'yes' AND
        a.appname = '{$sql['app']}' AND
        m.type = '{$sql['type']}' AND
        v.vid = (SELECT max(vid) FROM version WHERE id=m.id AND approved='YES')
    ORDER BY
        {$rssOrderBy}
    LIMIT 0,10
";

// Get data, then set the results.
$db->query($_rssSql,SQL_ALL,SQL_ASSOC);
$_rssData = $db->record;
$tpl->assign('data',$_rssData);

// Set our content-type and spit it out.
header('Content-Type: text/xml; charset=utf-8');
$tpl->display('rss.tpl',$cacheId);
exit;
?>
