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

define('ROOT_PATH','/home/morgamic/public_html/v2');
define('WEB_PATH','/~morgamic/v2');

// The repository directory is the place to store all addons binaries.
// This directory should be writable by the webserver (apache:apache).
define('REPO_DIR',ROOT_PATH.'/files');

define('LIB',ROOT_PATH.'/lib');

define('TEMPLATE_DIR',ROOT_PATH.'/tpl');
define('COMPILE_DIR',LIB.'/smarty/templates_c');
define('CACHE_DIR',LIB.'/smarty/cache');
define('CONFIG_DIR',LIB.'/smarty/configs');

define('DB_USER','username');
define('DB_PASS','password');
define('DB_HOST','localhost');
define('DB_NAME','database');
define('DB_PORT', '3306');

define('USE_DB_SESSIONS', TRUE);
?>
