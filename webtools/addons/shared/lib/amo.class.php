<?php
/**
 * AMO master class.  This class contains global application logic.
 * @todo properly separate accessors and mutators.
 * @todo don't store data in this superclass -- strip vars except for tpl/db.
 */
class AMO_Object
{
    var $db;
    var $tpl;

    /**
     * AMO_Object constructor.
     */
    function AMO_Object() {
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

    /**
     * Get all category names.
     *
     * @return array
     */
    function getCats($type=null)
    {
        if (!empty($type)) {
            $typesql = " WHERE cattype='{$type}' ";
        } else {
            $typesql = '';
        }

        // Gather categories.
        $this->db->query("
            SELECT DISTINCT
                CategoryID,
                CatName
            FROM
                categories
            {$typesql}
            GROUP BY
                CatName
            ORDER BY
                CatName
        ", SQL_INIT, SQL_ASSOC);

        do {
            $retval[$this->db->record['CategoryID']] = $this->db->record['CatName'];
        } while ($this->db->next(SQL_ASSOC));

        return $retval;
    }
    
    /**
     * Get all operating system names (platforms).  Used to populate forms.
     *
     * @return array
     */
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
            $retval[$this->db->record['OSID']] = $this->db->record['OSName'];
        } while ($this->db->next(SQL_ASSOC));

        return $retval;
    }

    /**
     * Get all application names.  Used to populate forms.
     */
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
            $retval[$this->db->record['AppID']] = $this->db->record['AppName'];
        } while ($this->db->next(SQL_ASSOC));

        return $retval;
    }

    /**
     * Get newest addons.
     *
     * @param string $app
     * @param string $type
     * @param int $limit
     * @return array
     */
    function getNewestAddons($app='firefox',$type='E',$limit=10) {

        // Get most popular extensions based on application.
        $this->db->query("
            SELECT DISTINCT
                m.ID ID, 
                m.Name name, 
                m.downloadcount dc,
                v.DateUpdated as dateupdated
            FROM
                main m
            INNER JOIN version v ON m.ID = v.ID
            INNER JOIN applications TA ON v.AppID = TA.AppID
            INNER JOIN os o ON v.OSID = o.OSID
            WHERE
                AppName = '{$app}' AND 
                downloadcount > '0' AND
                approved = 'YES' AND
                Type = '{$type}'
            GROUP BY
                m.ID
            ORDER BY
                v.dateupdated DESC , downloadcount DESC, rating DESC
            LIMIT 
                {$limit}
        ", SQL_ALL, SQL_ASSOC);

        return $this->db->record;
    }

    /**
     * Get most popular addons.
     *
     * @param string $app
     * @param string $type
     * @param int $limit
     * @return array
     */
     function getPopularAddons($app='firefox',$type='E', $limit=10) {

        // Return most popular addons.
        $this->db->query("
            SELECT DISTINCT
                m.ID ID, 
                m.Name name, 
                m.downloadcount dc,
                v.DateUpdated as dateupdated
            FROM
                main m
            INNER JOIN version v ON m.ID = v.ID
            INNER JOIN applications TA ON v.AppID = TA.AppID
            INNER JOIN os o ON v.OSID = o.OSID
            WHERE
                AppName = '{$app}' AND 
                downloadcount > '0' AND
                approved = 'YES' AND
                Type = '{$type}'
            GROUP BY
                m.ID
            ORDER BY
                downloadcount DESC, rating DESC, v.dateupdated DESC 
            LIMIT 
                {$limit}
        ", SQL_ALL, SQL_ASSOC);

        return $this->db->record;
     }

    /**
     * Get most popular addons.
     *
     * @param string $app
     * @param string $type
     * @param int $limit
     * @return array
     */
     function getRecommendedAddons($app='firefox',$type='E', $limit=10) {

        // Return most popular addons.
        $this->db->query("
            SELECT DISTINCT
                m.id, 
                m.name, 
                m.downloadcount,
                v.dateupdated,
                v.uri,
                r.body,
                r.title,
                v.size,
                v.version,
                p.previewuri
            FROM
                main m
            INNER JOIN version v ON m.ID = v.ID
            INNER JOIN applications TA ON v.AppID = TA.AppID
            INNER JOIN os o ON v.OSID = o.OSID
            INNER JOIN reviews r ON m.ID = r.ID
            INNER JOIN previews p ON p.ID = m.ID
            WHERE
                AppName = '{$app}' AND 
                downloadcount > '0' AND
                approved = 'YES' AND
                Type = '{$type}' AND
                r.featured = 'YES' AND
                p.preview = 'YES'
            GROUP BY
                m.ID
            ORDER BY
                m.Name
            LIMIT 
                {$limit}
        ", SQL_ALL, SQL_ASSOC);

        return $this->db->record;
     }

}
?>
