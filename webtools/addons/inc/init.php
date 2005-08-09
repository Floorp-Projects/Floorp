<?php
/**
 * Global initialization script.
 *
 * @package amo
 * @subpackage docs
 * @todo find a more elegant way to push in global template data (like $cats)
 */
// Include config file.
require_once('config.php');

// Set runtime options.
ini_set('display_errors',DISPLAY_ERRORS);
ini_set('error_reporting',ERROR_REPORTING);
ini_set('magic_quotes_gpc',0);

// Include required libraries and classes.
require_once('DB.php');  // PEAR::DB
require_once('Auth.php');  // PEAR::Auth
require_once(LIB.'/smarty/libs/Smarty.class.php');  // Smarty
require_once(LIB.'/amo.class.php');
require_once(LIB.'/addon.class.php');
require_once(LIB.'/auth.class.php');
require_once(LIB.'/error.php');
require_once(LIB.'/rdf.class.php');
require_once(LIB.'/rss.class.php');
require_once(LIB.'/session.class.php');
require_once(LIB.'/sql.class.php');
require_once(LIB.'/user.class.php');
require_once(LIB.'/version.class.php');

// Smarty configuration.
class AMO_Smarty extends Smarty
{
    function AMO_Smarty()
    {
        $this->template_dir = TEMPLATE_DIR;
        $this->compile_dir = COMPILE_DIR;
        $this->cache_dir = CACHE_DIR;
        $this->config_dir = CONFIG_DIR;

        // Pass config variables to Smarty object.
        $this->assign('config',
            array(  'webpath'   => WEB_PATH,
                    'rootpath'  => ROOT_PATH,
                    'repo'      => REPO_DIR)
        );
    }
}

// Database configuration.
class AMO_SQL extends SQL 
{
    function AMO_SQL()
    {
        $dsn = array (
            'phptype'  => 'mysql',
            'dbsyntax' => 'mysql',
            'username' => DB_USER,
            'password' => DB_PASS,
            'hostspec' => DB_HOST,
            'database' => DB_NAME
        );
        $this->connect($dsn);

        // Test connection; display "gone fishing" on failure.
        if (DB::isError($this->db)) {
            triggerError($this->error,'site-down.tpl');
        }
    }
}

// Global template object.
$tpl = new AMO_Smarty();

// Global DB object.
$db = new AMO_SQL();
?>
