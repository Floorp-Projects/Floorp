<?php
// Include required libraries and classes.
require_once('DB.php');  // PEAR::DB
require_once('Auth.php');  // PEAR::Auth
require_once(LIB.'/amo.class.php');
require_once(LIB.'/addon.class.php');
require_once(LIB.'/auth.class.php');
require_once(LIB.'/error.php');
require_once(LIB.'/rdf.class.php');
require_once(LIB.'/rss.class.php');
require_once(LIB.'/sql.class.php');
require_once(LIB.'/user.class.php');
require_once(LIB.'/version.class.php');

// Database configuration.
class AMO_SQL extends SQL 
{
    function AMO_SQL()
    {
        global $shadow_config;

        require_once("DB.php");

        /**
         * If our current script is in the shadow array, we should 
         * connect to the shadow db instead of the default.
         */
        if (in_array(SCRIPT_NAME, $shadow_config)) {
            $shadow_dsn = array (
                'phptype'  => 'mysql',
                'dbsyntax' => 'mysql',
                'username' => SHADOW_USER,
                'password' => SHADOW_PASS,
                'hostspec' => SHADOW_HOST,
                'database' => SHADOW_NAME,
                'port'        => SHADOW_PORT
            );

            $this->connect($shadow_dsn);
        } else {
            $dsn = array (
                'phptype'  => 'mysql',
                'dbsyntax' => 'mysql',
                'username' => DB_USER,
                'password' => DB_PASS,
                'hostspec' => DB_HOST,
                'database' => DB_NAME,
                'port'        => DB_PORT
            );

            $this->connect($dsn);
        }

        // Test connection; display "gone fishing" on failure.
        if (DB::isError($this->db)) {
            triggerError($this->error,'site-down.tpl');
        }
    }
}

$db = new AMO_SQL();

// Setup the session for the secure pages
$_auth = new AMO_Auth();
session_set_save_handler(
    array(&$_auth, "_openSession"), 
    array(&$_auth, "_closeSession"), 
    array(&$_auth, "_readSession"), 
    array(&$_auth, "_writeSession"), 
    array(&$_auth, "_destroySession"), 
    array(&$_auth, "_gcSession")
    );

?>
