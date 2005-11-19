<?php
require_once($config['base_path'].'/includes/contrib/adodb/adodb.inc.php');

function NewDBConnection(){
    global $config;

    $db = NewADOConnection($config['db_dsn']);
    if ($config['debug']){
        $db->debug = true;
    }
    if (!$db) die("Connection failed");
    return $db;
}
?>