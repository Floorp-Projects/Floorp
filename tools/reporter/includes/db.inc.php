<?php
require_once($config['base_path'].'/includes/contrib/adodb/adodb.inc.php');

function NewDBConnection(){
    global $config;

    $db = NewADOConnection($config['db_dsn']);
    if ($config['debug']){
        $db->debug = true;
    }
    if (!$db) {
        trigger_error("Sorry, we are unable to process your request right now.  The database is either busy or unavailable.", E_USER_ERROR);
    }
    return $db;
}
?>