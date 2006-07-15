<?php
/**
 * AMO global configuration document.
 * Unless otherwise noted, trailing slashes should not be used.
 * @package amo
 * @subpackage inc
 */



/**
 * Global settings.
 */

/**
 * Set runtime error options.
 * See http://php.oregonstate.edu/manual/en/ref.errorfunc.php#errorfunc.constants
 */
define('DISPLAY_ERRORS',1);
define('ERROR_REPORTING',2047);

/**
 * This is the base of the URL.  It includes https|http.
 */
define('HTTP_HOST','https://addons-test.mozilla.org');

/**
 * This is the root directory for public.
 */
define('ROOT_PATH','/data/www/addons-test.mozilla.org/v2-memcache/public');

/**
 *
 * This is the web path from the hostname.
 * In production, it's typically just ''.
 * However, if the URL to your installation is something like:
 *     https://update-staging.mozilla.org/~morgamic/v2/public/htdocs/
 * Then WEB_PATH would be:
 *     /~morgamic/v2/public/htdocs 
 */
define('WEB_PATH','/v2-memcache/public/htdocs');

/**
 * Shared library directory contains all class definitions and central code.
 */
define('LIB','/data/www/addons-test.mozilla.org/v2-memcache/shared/lib');



/**
 * Smarty configuration.  The _DIR paths need to be writeable by the web user.
 */
define('SMARTY_BASEDIR','/usr/local/lib/php/Smarty');
define('TEMPLATE_DIR',ROOT_PATH.'/tpl');
define('COMPILE_DIR',ROOT_PATH.'/templates_c');
define('CACHE_DIR',ROOT_PATH.'/cache');
define('CONFIG_DIR',ROOT_PATH.'/configs');



/**
 * Database configuration.
 */

/**
 * This database has read/write capabilities.
 */
define('DB_USER','');
define('DB_PASS','');
define('DB_HOST','');
define('DB_NAME','');
define('DB_PORT', '3306');

/**
 * The shadow_db has read-only access.
 */
define('SHADOW_USER','');
define('SHADOW_PASS','');
define('SHADOW_HOST','');
define('SHADOW_NAME','');
define('SHADOW_PORT', '3306');

/**
 * Config arrays.
 *
 * These arrays are for shadow database and cache configuration.  The goal here is to
 * adjust application behavior based on membership in these arrays.
 *
 * Not only should we be able to control these behaviors, but we wanted to do so from
 * a central config file that would be external to the CVS checkout.
 *
 * This gives us flexibility in production to make on-the-fly adjustments under duress,
 * and lets sysadmins tweak things without creating CVS conflicts in production.
 */

/**
 * Shadow array defines which pages are going to use the shadow database.
 * Filenames found in this array must work with read-only access to the database.
 *
 * Array contains [script name] values.  For example, we could set the main page to
 * use the shadow database:
 *     'index.php'
 *
 * In the case where the PHP script is in a subdirectory, use the relative path from webroot:
 *     'somedirectory/index.php'
 */
$shadow_config = array(
    'addon.php',
    'author.php',
    'comments.php',
    'extensions.php',
    'faq.php',
    'feeds.php',
    'finalists.php',
    'history.php',
    'index.php',
    'policy.php',
    'recommended.php',
    'rss.php',
    'search-engines.php',
    'search.php',
    'themes.php',
    'update.php',
    'winners.php'
);

/**
 * Array contains [script name] => [timeout] entries.  For example, we could set 
 * a timeout of 1800 for index.php:
 *     'index.php' => '1800'
 *
 * In the case where the PHP script is in a subdirectory, use the relative path from webroot:
 *     'somedirectory/index.php' => '1800'
 *
 * If you have having any issues with memcached not being available, you should just comment out these array entries.
 * It would also make sense to do this if you are developing / testing, since caching stuff would just confuse you.
 */
$cache_config = array(
    'addon.php' => 900,
    'author.php' => 900,
    'comments.php' => 900,
    'extensions.php' => 900,
    'faq.php' => 900,
    'feeds.php' => 900,
    'history.php' => 900,
    'index.php' => 20,
    'pfs.php' => 1800,
    'plugins.php' => 1800,
    'policy.php' => 900,
    'recommended.php' => 900,
    'rss.php' => 900,
    'search-engines.php' => 900,
    'search.php' => 900,
    'themes.php' => 900,
    'update.php' => 900
);



/**
 * memcache configuration.
 * 
 * The memcache_config array lists all possible memcached servers to use in case the default server does not have the appropriate key.
 */
$memcache_config = array(
    'localhost' => array(
       'port' => '11211',
       'persistent' => true,
       'weight' => '1',
       'timeout' => '1',
       'retry_interval' => 15
    )
);



/**
 * Content type configuration.
 *
 * Array contains [script name] => [content type string] entries.  These entries flag files that need to have a specific
 *     content-type that is not text/html.
 */
$contentType_config = array(
    'update.php' => 'text/xml; charset=utf-8',
    'rss.php'    => 'text/xml; charset=utf-8',
    'pfs.php'    => 'text/xml; charset=utf-8'
);
?>
