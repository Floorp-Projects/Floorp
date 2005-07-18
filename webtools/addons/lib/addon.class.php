<?php
/**
 * Addon super class.  The class to end all classes.
 * @package amo
 * @subpackage lib
 */
class AddOn extends AMO_Object
{
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

    /**
     * Class constructor.
     */
     function AddOn() 
     {
        // Our DB and Smarty objects are global to save cycles.
        global $db, $tpl;

        // Pass by reference in order to save memory.
        $this->db =& $db;
        $this->tpl =& $tpl;
     }

     function getAddOn($ID)
     {
        $this->db->query("SELECT * FROM main WHERE ID = '{$ID}'", SQL_ALL, SQL_ASSOC);
        return $this->db->record;
     }
}
