<?php
/**
 * Addon super class.  The class to end all classes.
 * @package amo
 * @subpackage lib
 */
class AddOn extends AMO_Object
{
    // AddOn metadata.
    var $ID;
    var $GUID;
    var $Name;
    var $Type;
    var $DateAdded;
    var $DateUpdated;
    var $Homepage;
    var $Description;
    var $Rating;
    var $downloadcount;
    var $TotalDownloads;
    var $devcomments;
    var $db;
    var $tpl;

    // AddOn author metadata.
    var $UserID;
    var $UserName;
    var $UserEmail;
    var $UserWebsite;
    var $UserEmailHide;

    /**
     * Class constructor.
     */
     function AddOn($ID=null) 
     {
        // Our DB and Smarty objects are global to save cycles.
        global $db, $tpl;

        // Pass by reference in order to save memory.
        $this->db =& $db;
        $this->tpl =& $tpl;

        // If $ID is set, attempt to retrieve data.
        if (!empty($ID)) {
            $this->getAddOn($ID);
        }
     }

     function getAddOn($ID)
     {
        $this->db->query("
            SELECT 
                main.*,
                userprofiles.UserID,
                userprofiles.UserName,
                userprofiles.UserEmail,
                userprofiles.UserWebsite,
                userprofiles.UserEmailHide
            FROM 
                main 
            INNER JOIN authorxref ON authorxref.ID = main.ID
            INNER JOIN userprofiles ON userprofiles.UserID = authorxref.UserID
            WHERE 
                main.ID = '{$ID}'
        ", SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            foreach ($this->db->record as $key=>$val) {
                $this->$key = $val;
            }
        }
     }
}
