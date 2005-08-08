<?php
/**
 * AMO master class.  This class contains global application logic.
 */
class AMO_Object
{
    var $cats;
    var $apps;
    var $platforms;

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

    function getCats()
    {
        // Gather categories.
        $this->db->query("
            SELECT
                CategoryID,
                CatName
            FROM
                categories
            GROUP BY
                CatName
            ORDER BY
                CatName
        ", SQL_INIT, SQL_ASSOC);

        do {
            $this->Cats[$this->db->record['CategoryID']] = $this->db->record['CatName'];
        } while ($this->db->next(SQL_ASSOC));
    }
    
    function getPlatforms()
    {
        // Gather platforms..
        $this->db->query("
            SELECT
                OSID,
                OSName
            FROM
                os
            ORDER BY
                OSName
        ", SQL_INIT, SQL_ASSOC);

        do { 
            $this->Platforms[$this->db->record['OSID']] = $this->db->record['OSName'];
        } while ($this->db->next(SQL_ASSOC));

    }

    function getApps()
    {
        // Gather aapplications.
        $this->db->query("
            SELECT DISTINCT
                AppID,
                AppName
            FROM
                applications
            WHERE
                public_ver = 'YES'
            GROUP BY
                AppName
        ", SQL_INIT, SQL_ASSOC);

        do {
            $this->Apps[$this->db->record['AppID']] = $this->db->record['AppName'];
        } while ($this->db->next(SQL_ASSOC));
    }

    function getComment($id)
    {
         // Get one comment based on CommentID.
        $this->db->query("
            SELECT
                feedback.*
            FROM
                feedback
            WHERE
                id = '{$id}'
        ", SQL_INIT, SQL_ASSOC);

        return $this->db->record[0];
    }
}
?>
