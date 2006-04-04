<?php
require_once($config['base_path'].'/includes/contrib/adodb/adodb.inc.php');

function NewDBConnection(){
    global $config;

    $db = NewADOConnection($config['db_dsn']);
    if ($config['debug']){
        $db->debug = true;
    }
    if (!$db) {
        trigger_error("Database server unavailable.", E_USER_ERROR);
    }
    return $db;
}
?>