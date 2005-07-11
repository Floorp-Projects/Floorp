<?php
/**
 * AMO global configuration document.
 * @package amo
 * @subpackage inc
 */

// Set runtime error options.
// See http://php.oregonstate.edu/manual/en/ref.errorfunc.php#errorfunc.constants
define('DISPLAY_ERRORS',E_ALL);
define('ERROR_REPORTING',1);

define('ROOT_PATH','/home/morgamic/public_html/v2');
define('WEB_PATH','/~morgamic/v2');

define('LIB',ROOT_PATH.'/lib');

define('TEMPLATE_DIR',ROOT_PATH.'/tpl');
define('COMPILE_DIR',LIB.'/smarty/templates_c');
define('CACHE_DIR',LIB.'/smarty/cache');
define('CONFIG_DIR',LIB.'/smarty/configs');

define('DB_USER','username');
define('DB_PASS','password');
define('DB_HOST','localhost');
define('DB_NAME','database');
?>
