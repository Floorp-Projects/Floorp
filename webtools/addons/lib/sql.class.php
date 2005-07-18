<?php
/**
 * PEAR::DB wrappers for SQL queries.
 * @package amo
 * @subpackage lib
 * @author Monte Ohrt <monte [AT] ohrt [DOT] com>
 */

// Define query types.
define('SQL_NONE', 1);
define('SQL_ALL', 2);
define('SQL_INIT', 3);

// Define the query formats.
define('SQL_ASSOC', 1);
define('SQL_INDEX', 2);

class SQL {
    
    var $db = null;
    var $result = null;
    var $error = null;
    var $record = null;
    
    /**
     * Class constructor.
     */
    function SQL() { }
    
    /**
     * Connect to the database.
     *
     * @param string $dsn the data source name
     * @return bool
     */
    function connect($dsn) {
        $this->db =& DB::connect($dsn);

        if(PEAR::isError($this->db)) {
            $this->error =& $this->db->getMessage();
            return false;
        }        
        return true;
    }
    
    /**
     * Disconnect from the database.
     */
    function disconnect() {
        $this->db->disconnect();   
    }
    
    /**
     * Query the database.
     *
     * @param string $query the SQL query
     * @param string $type the type of query
     * @param string $format the query format
     */
    function query($query, $type = SQL_NONE, $format = SQL_INDEX) {

        $this->record = array();
        $_data = array();
        
        // Determine fetch mode (index or associative).
        $_fetchmode = ($format == SQL_ASSOC) ? DB_FETCHMODE_ASSOC : null;
        
        $this->result = $this->db->query($query);
        if (DB::isError($this->result)) {
            $this->error = $this->result->getMessage();
            return false;
        }
        switch ($type) {
            case SQL_ALL:
                // Get all the records.
                while($_row = $this->result->fetchRow($_fetchmode)) {
                    $_data[] = $_row;   
                }
                $this->result->free();            
                $this->record = $_data;
                break;
            case SQL_INIT:
                // Get the first record.
                $this->record = $this->result->fetchRow($_fetchmode);
                break;
            case SQL_NONE:
            default:
                // Records will be looped over with next().
                break;   
        }
        return true;
    }
    
    /**
     * Select next row in result.
     * @param string $format the query format
     */
    function next($format = SQL_INDEX) {
        // Fetch mode (index or associative).
        $_fetchmode = ($format == SQL_ASSOC) ? DB_FETCHMODE_ASSOC : null;
        if ($this->record = $this->result->fetchRow($_fetchmode)) {
            return true;
        } else {
            $this->result->free();
            return false;
        }
            
    }
}
?>
