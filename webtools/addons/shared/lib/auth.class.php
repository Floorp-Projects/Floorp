<?php
/**
 * Authentication class.  Will handle all session functions.  Note that database
 * layout is hardcoded into this class.  If this class is used anywhere but AMO, it
 * will need that pulled out and put into class variables (for example, column names,
 * etc.)
 *
 * Class is roughly based on some examples from php.net
 *
 * @package amo
 * @subpackage lib
 *
 */

require_once 'amo.class.php';

class AMO_Auth extends AMO_Object{

    /**
     * How long the sessions should be.
     * @access private
     * @var int;
     */
    var $_expires  = 0;

    /**
     * UserID of the person logging in
     * @access private
     * @var int;
     */
    var $_user_id  = null;

    /**
     * Name of the user table
     * @access private
     * @var string;
     */
    var $_user_table    = 'userprofiles';

    /**
     * Name of the sessions table to store data in.
     * @access private
     * @var string;
     */
    var $_session_table = 'session_data';

    /**
     * Constructor for the AMO Authentication object
     * @access public
     */
    function AMO_Auth()
    {
        parent::AMO_Object();
        $this->_expires  = get_cfg_var('session.gc_maxlifetime');
    }

    /**
     * Dummy function, we don't need it but session_set_save_handler() requires it
     * @access private
     * @param string path to save files (NOT USED)
     * @param string name of file (NOT USED)
     * @return bool true
     */
    function _openSession($path, $name)
    {
        return true;
    }

    /**
     * This function will actually create the row in the database for the function.
     * session_start() needs to be called before this function.
     * @access private
     * @return bool true
     */
    function createSession()
    {
        if (is_null($this->_user_id)) {
            // We're storing the userid in this object (it get's put in there when
            // the person authenticates.  If the field is empty, there isn't really
            // any point to starting a session, so we just return.
            return false;
        }

        // technically, none of these should need escaping, but hey...
        $_id      = mysql_real_escape_string(session_id());
        $_user_id = mysql_real_escape_string($this->_user_id);
        $_expires = mysql_real_escape_string(time() + $this->_expires);

        $_sql = "INSERT INTO `{$this->_session_table}` 
                 ( `sess_id`,
                   `sess_user_id`,
                   `sess_expires`,
                   `sess_data`
                 ) VALUES (
                   '{$_id}',
                   '{$_user_id}',
                   '{$_expires}',
                   ''
                 )";

       $this->db->query($_sql);
       return true;
    }

    /**
     * Dummy function, we don't need it but session_set_save_handler() requires it
     * @access private
     * @return bool true
     */
    function _closeSession()
    {
        return true;
    }

    /**
     * Pulls data from the session (database in our case)
     * @access private
     * @param string session id
     * @return string with data from session, or empty on empty session or failure
     */
    function _readSession($id)
    {
        $_id = mysql_real_escape_string($id);

        $_sql = "SELECT 
                    `sess_data`
                 FROM
                    `{$this->_session_table}`
                 WHERE
                    `sess_id`={$_id}
                 AND
                    `sess_expires` > CURRENT_TIMESTAMP()";

        $this->db->query($_sql, SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)){
            return $this->db->record;
        } else {
            return '';
        }
    }

    /**
     * Push data into the session (into the database)
     * @access private
     * @param string session id
     * @param string data to store
     * @return bool true on success, false on failure
     */
    function _writeSession($id, $data)
    {
        if (is_null($this->_user_id)) {
            // We're storing the userid in this object (it get's put in there when
            // the person authenticates.  If the field is empty, there isn't really
            // any point to starting a session, so we just return.
            return false;
        }
        // An extra check, otherwise session_start() would start valid sessions
        if ($this->validSession()){
            $_id           = mysql_real_escape_string($id);
            $_user_id      = mysql_real_escape_string($this->_user_id);
            $_data         = mysql_real_escape_string($data);
            $_expires      = mysql_real_escape_string(time() + $this->_expires);

            $_sql = "REPLACE INTO 
                        `{$this->_session_table}` 
                        (   `sess_id`,
                            `sess_user_id`,
                            `sess_expires`,
                            `sess_data`
                        ) VALUES (
                            '{$_id}',
                            '{$_user_id},
                            '{$_expires}',
                            '{$_data}'
                        )";

            $this->db->query($_sql, SQL_INIT, SQL_ASSOC);
            return true;
        }
        return false;
    }

    /**
     * Checks if the current session is valid or not.  session_start() needs to be
     * called before this.
     * @access public
     * @return bool true if valid, false if not
     */
    function validSession()
    {
        $_session_id = mysql_real_escape_string(session_id());

        $_sql = "SELECT 
                    `sess_user_id`
                 FROM 
                    `{$this->_session_table}` 
                 WHERE 
                    `sess_id` = '{$_session_id}'
                 LIMIT 1";

        $this->db->query($_sql, SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)){
            $user = $this->db->record;
            $this->_user_id = $user['sess_user_id'];
            return true;
        } else {
            return false;
        }
    }

    /**
     *  Checks if the user should be able to start a session with us (looks them up
     *  in the user table)
     *  @access public
     *  @param string $username
     *  @param string $password
     *  @return bool true on success, false on failure
     */
    function authenticate($username,$password)
    {
        if (empty($username)||empty($password)) {
            return false;
        }
        $_username = trim(mysql_real_escape_string($username));
        $_password = trim(mysql_real_escape_string($password));
        $_sql =    "SELECT 
                        `UserID`
                    FROM 
                        `{$this->_user_table}` 
                    WHERE 
                        `UserEmail`='{$_username}' 
                    AND 
                        `UserPass`=MD5('{$_password}')
                    AND 
                        `UserMode` != 'D'
                    LIMIT 1";

        $this->db->query($_sql, SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            $_record = $this->db->record;
            $this->_user_id = $_record['UserID'];
            return true;
        } else {
            return false;
        }
    }

    /**
     * Destroys the current session
     * @access private
     * @param string session id
     * @return bool true
     */
    function _destroySession($id)
    {
        $_id = mysql_real_escape_string($id);

        $_sql = "DELETE FROM 
                    `{$this->_session_table}`
                 WHERE 
                    `sess_id` ='{$_id}'";

        $this->db->query($_sql);

        $this->_user_id = null;
        $_COOKIE = array(); 
        $_SESSION = array();

        setcookie(session_name(), '', time()-42000, '/');

        return true;
    }

    /**
     * Clean out stale sessions
     * @access private
     * @return bool true
     */
    function _gcSession()
    {
        $_sql = "DELETE FROM 
                    `{$this->_session_table}`
                 WHERE 
                    `sess_expires` < CURRENT_TIMESTAMP()";

        $this->db->query($_sql);
        return true;
    }

    /**
     * This is simply a conveinence function because pretty much everything is based
     * off the ID.
     */
    function getId()
    {
        return $this->_user_id;
    }
}
?>
