<?php
/**
 * Authentication and Session handling class.
 * This class circumvents basic PHP Sessions and uses MySQL to store all session data.
 * The reason this was done was to preserve sessions across different LVS nodes by utilizing the application latyer.
 *
 * @package amo
 * @subpackage lib
 */
class Session
{
    var $id;
    var $data;
    var $db;

    /**
     * Constructor.
     *
     * @param int $id
     */
    function Session($id=null)
    {
        global $db;
        
        $this->db =& $db;

        if (!is_null($id)) {
            $this->resume($id);
        }
    }

    /**
     * Write (or overwrite) a value to the session data array.
     *
     * @param string $key
     * @param mixed $val
     * @param bool $overwrite
     *
     * @return bool
     */
    function write($key,$val,$overwrite=false)
    {
        if (!isset($this->data[$key]) || ($this->data[$key] == $val && $overwrite)) {
            $this->data[$key] = $val;
            return true;
        } else {
            return false;
        }
    }

    /**
     * Retrieve a value from the session data array.
     *
     * @param string $key
     *
     * @return mixed|bool
     */
    function read($key)
    {
        if (isset($this->data[$key])) {
            return $this->data[$key];
        } else {
            return false;
        }
    }

    /**
     * Start a session.
     *
     * @return bool
     */
    function create()
    {
        // Insert a session entry in the database.
        if (true) {
            return true;
        } else {
            return false;
        }
        
        // Set a cookie containing the session ID.
    }

    /**
     * Resume a session.  This gathers session data and restores it.
     *
     * @param $id
     *
     * @return bool
     */
    function resume($id)
    {
        // Retrieve session data from database based on id.
        if (true) {
            return true;
        } else {
            return false;
        }
    }
}
?>
