<?php
/**
 * Survey config.
 * 
 * @package survey
 * @subpackage inc
 */
// Set runtime error options.
// See http://php.oregonstate.edu/manual/en/ref.errorfunc.php#errorfunc.constants
define('DISPLAY_ERRORS',1);
define('ERROR_REPORTING',2047);

// Root path of the application directory.
define('ROOT_PATH','');

// Header and footer includes that are required for htdocs.
define('HEADER',ROOT_PATH.'/inc/header.php');
define('FOOTER',ROOT_PATH.'/inc/footer.php');

// This is the relative web path from the hostname.
// For example if we had http://foo.com/bar, WEB_PATH would be '/bar'.
define('WEB_PATH','');

// Database configuration.
define('DB_USER','');
define('DB_PASS','');
define('DB_HOST','');
define('DB_NAME','');
define('DB_PORT', '3306');
?>
