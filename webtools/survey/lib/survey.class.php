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
     * @return arrayory names.
     */
    function getIntends()
    {
        // Return array.
        $retval = array();
        
        // Gather intend list.
        $this->db->query("SELECT * FROM intend ORDER BY pos, description", SQL_INIT, SQL_ASSOC);

        do {
            $retval[$this->db->record['id']] = $this->db->record['description'];
        } while ($this->db->next(SQL_ASSOC));

        return $retval;
    }
    
    /**
     * Get possible issues.
     * @return array
     */
    function getIssues()
    {
        // Return array.
        $retval = array();

        // Gather platforms..
        $this->db->query("SELECT * FROM issue ORDER BY pos, description", SQL_INIT, SQL_ASSOC);

        do { 
            $retval[$this->db->record['id']] = $this->db->record['description'];
        } while ($this->db->next(SQL_ASSOC));

        return $retval;
    }
}
?>
