<?php
/**
 * AMO global configuration document.
 * Unless otherwise noted, trailing slashes should not be used.
 * @package amo
 * @subpackage inc
 */

// Set runtime error options.
// See http://php.oregonstate.edu/manual/en/ref.errorfunc.php#errorfunc.constants
define('DISPLAY_ERRORS',1);
define('ERROR_REPORTING',2047);

// This is the base of the URL.  It includes https|http.
define('HTTP_HOST','https://YOURHOST.DOMAIN.TLD');

// This is the root directory for public.
define('ROOT_PATH','/file/path/to/public');

// This is the web path from the hostname.
// In production, it's typically just ''.
// However, if the URL to your installation is something like:
//     https://update-staging.mozilla.org/~morgamic/v2/public/htdocs/
// Then WEB_PATH would be:
//     /~morgamic/v2/public/htdocs 
define('WEB_PATH','/web/relative/path/to/htdocs');

// Shared library directory contains all class definitions and central code.
define('LIB','/file/path/to/shared/lib');

// Smarty configuration.
define('SMARTY_BASEDIR','/usr/local/lib/php/Smarty');
define('TEMPLATE_DIR',ROOT_PATH.'/tpl');
define('COMPILE_DIR',ROOT_PATH.'/templates_c');
define('CACHE_DIR',ROOT_PATH.'/cache');
define('CONFIG_DIR',ROOT_PATH.'/configs');

// DB configuration.
// This database has read/write capabilities.
define('DB_USER','');
define('DB_PASS','');
define('DB_HOST','');
define('DB_NAME','');
define('DB_PORT', '3306');

// Shadow DB configuration.
// This database has read-only access.
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

// Shadow array defines which pages are going to use the shadow database.
// Filenames found in this array must work with read-only access to the database.
//
// Array contains [script name] values.  For example, we could set the main page to
// use the shadow database:
//     'index.php'
//
// In the case where the PHP script is in a subdirectory, use the relative path from webroot:
//     'somedirectory/index.php'
$shadow_config = array(
);

// Array contains [script name] => [timeout] entries.  For example, we could set 
// a timeout of 1800 for index.php:
//     'index.php' => '1800'
//
// In the case where the PHP script is in a subdirectory, use the relative path from webroot:
//     'somedirectory/index.php' => '1800'
$cache_config = array(
);
?>
