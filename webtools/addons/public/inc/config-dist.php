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

// Database information.
define('DB_USER','');
define('DB_PASS','');
define('DB_HOST','');
define('DB_NAME','');
define('DB_PORT', '');
?>
