<?php
/**
 * Syndication for commonly referenced lists.
 *
 * @package amo
 * @subpackage docs
 */

/**
 * Pull our input params.
 */

/**
 * CHECK CACHE
 *
 * Check to see if we already have a matching cache_id.
 * If it exists, we can pull from it and exit; and avoid recompiling.
 */
$tpl = new AMO_Smarty();

// Set our cache timeout to 1 hour.
$tpl->caching = true;
$tpl->cache_timeout = 3600;

// Determine our cache_id based on the RSS feed's arguments.

if ($tpl->is_cached('rss.tpl',$cache_id)) {
    $tpl->display('rss.tpl',$cache_id);
    exit;
}
?>
