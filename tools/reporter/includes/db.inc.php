<?php
require_once($config['base_path'].'/includes/contrib/adodb/adodb.inc.php');

class db extends ADOConnection
{
    function NewADOConnection(){
        $db = NewADOConnection($config['db_dsn']);
$db->debug = true;
        if (!$db) die("Connection failed");
        return $db;
    }
}
?>