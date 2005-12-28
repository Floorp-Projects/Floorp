<?php
// Include required libraries and classes.
require_once('DB.php');  // PEAR::DB
require_once('Auth.php');  // PEAR::Auth
require_once(LIB.'/amo.class.php');
require_once(LIB.'/addon.class.php');
require_once(LIB.'/error.php');
require_once(LIB.'/rdf.class.php');
require_once(LIB.'/rss.class.php');
require_once(LIB.'/session.class.php');
require_once(LIB.'/sql.class.php');
require_once(LIB.'/user.class.php');
require_once(LIB.'/version.class.php');

// Database configuration.
class AMO_SQL extends SQL 
{
    function AMO_SQL()
    {
        require_once("DB.php");
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

        // Test connection; display "gone fishing" on failure.
        if (DB::isError($this->db)) {
            triggerError($this->error,'site-down.tpl');
        }
    }
}

$db = new AMO_SQL();

if (USE_DB_SESSIONS)
{
    $amo_session_handler = new AMO_Session($db);
    session_set_save_handler(array(&$amo_session_handler, '_open'),
    array(&$amo_session_handler, '_close'),
    array(&$amo_session_handler, '_read'),
    array(&$amo_session_handler, '_write'),
    array(&$amo_session_handler, '_destroy'),
    array(&$amo_session_handler, '_gc'));
}
?>
