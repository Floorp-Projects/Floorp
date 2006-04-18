<?php
/**
 * Survey master class.  This class contains global application logic.
 * @package amo
 * @subpackage lib
 */
class Survey
{
    var $db;

    /**
     * AMO_Object constructor.
     */
    function Survey() {
        global $db;
        $this->db =& $db;
    }

    /**
     * Get possible intentions.
     * @param int $id
     * @return array of names.
     */
    function getIntends($id)
    {
        $retval = array();
        $this->db->query("SELECT * FROM intend WHERE app_id={$id} ORDER BY pos DESC, description", SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            do {
                $retval[$this->db->record['id']] = $this->db->record['description'];
            } while ($this->db->next(SQL_ASSOC));
        }

        return $retval;
    }
    
    /**
     * Get possible issues.
     * @param int $id
     * @return array
     */
    function getIssues($id)
    {
        $retval = array();
        $this->db->query("SELECT * FROM issue WHERE app_id={$id} ORDER BY pos DESC, description", SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            do { 
                $retval[$this->db->record['id']] = $this->db->record['description'];
            } while ($this->db->next(SQL_ASSOC));
        }

        return $retval;
    }

    /**
     * Get an app_id and the template name based on app_name.
     * The app_name is passed from the client in the GET string.
     *
     * @return array|false
     */
    function getAppIdByName($name) {
        $this->db->query("SELECT id FROM app WHERE app_name='{$name}' LIMIT 1", SQL_ALL, SQL_ASSOC);
        if (!empty($this->db->record)) {
            return $this->db->record[0]['id'];
        } else {
            return false;
        }
    }
}
?>
