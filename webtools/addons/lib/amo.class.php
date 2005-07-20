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
     * Set var.
     *
     * @param string $key name of object property to set
     * @param mixed $val value to assign
     *
     * @return bool
     */
    function setVar($key,$val)
    {
        $this->$key = $val;
        return true;
    }

    /**
     * Set an array of variables based on a $db record.
     *
     * @param array $data associative array of data.
     *
     * @return bool
     */
    function setVars($data)
    {
        if (is_array($data)) {
            foreach ($data as $key=>$val) {
                $this->setVar($key,$val);
            }
            return true;
        } else {
            return false;
        }
    }

}
?>
