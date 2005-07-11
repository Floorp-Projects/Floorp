<?php

// Include config file.
require_once('config.php');

// Set runtime options.
ini_set('display_errors',DISPLAY_ERRORS);
ini_set('error_reporting',ERROR_REPORTING);
ini_set('magic_quotes_gpc',0);

// Include required libraries and classes.
require_once('DB.php');
require_once(LIB.'/smarty/libs/Smarty.class.php');
require_once(LIB.'/addon.class.php');
require_once(LIB.'/auth.class.php');
require_once(LIB.'/rdf.class.php');
require_once(LIB.'/rss.class.php');
require_once(LIB.'/session.class.php');
require_once(LIB.'/sql.class.php');
require_once(LIB.'/user.class.php');
require_once(LIB.'/version.class.php');

// Instantiate Smarty class.
$smarty = new Smarty();

// Set up smarty.
$smarty->template_dir = TEMPLATE_DIR;
$smarty->compile_dir = COMPILE_DIR;
$smarty->cache_dir = CACHE_DIR;
$smarty->config_dir = CONFIG_DIR;

// Instantiate SQL class.
$db = new SQL();

// Define PEAR options.
$dsn = array (
    'phptype'  => 'mysql',
    'dbsyntax' => 'mysql',
    'username' => DB_USER,
    'password' => DB_PASS,
    'hostspec' => DB_HOST,
    'database' => DB_NAME
);

// Try to connect.  If we do not connect, show standard 'gone fishing' error page.
if (!$db->connect($dsn)) {
    $wrapper = 'inc/wrappers/nonav.tpl';
    $smarty->assign(
        array(
            'content'=>'site-down.tpl',
            'error'=>$db->error
        )
    );
    $smarty->display($wrapper);
    exit;
}
?>
