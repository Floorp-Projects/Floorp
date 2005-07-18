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

        // Default wrapper for AMO template.
        $this->wrapper = 'inc/wrappers/default.tpl';

        if (DB::isError($this->db)) {
            $this->tpl->assign(
                array(
                    'content'=>'site-down.tpl',
                    'error'=>$this->db->error
                )
            );
            $this->tpl->display($this->wrapper);
            exit;
        }
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
