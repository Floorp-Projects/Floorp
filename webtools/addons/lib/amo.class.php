<?php
/**
 * AMO master class.  This class contains global application logic.
 */
class AMO_Object
{
    var $wrapper;

    function AMO_Object()
    {
        // Our DB and Smarty objects are global to save cycles.
        global $db, $tpl;

        // Pass by reference in order to save memory.
        $this->db =& $db;
        $this->tpl =& $tpl;
    }

    /**
     * Close database connection and display output.
     */
    function finish()
    {
        $this->db->disconnect();
        $this->tpl->display($this->wrapper);
    }
}
?>
