<?php
/**
 * User class.  Contains user methods and metadata.
 * @package amo
 * @subpackage docs
 */
class User extends AMO_Object
{
    // User metadata.
    var $UserID;
    var $UserEmail;
    var $UserWebsite;
    var $UserMode;
    var $UserTrusted;
    var $UserEmailHide;
    var $UserLastLogin;

    // Addons metadata.
    var $AddOns;

    /**
     * Constructor.
     *
     * @param int $UserID UserID
     */
    function User($UserID=null)
    {
        // Our DB and Smarty objects are global to save cycles.
        global $db, $tpl;

        // Pass by reference in order to save memory.
        $this->db =& $db;
        $this->tpl =& $tpl;

        // If $ID is set, attempt to retrieve data.
        if (!empty($UserID)) {
            $this->setVar('UserID',$UserID);
            $this->getUser();
            $this->GetAddons();
        }
    }

    /**
     * Get user information from singular record.
     */
    function getUser()
    {
        $this->db->query("
            SELECT
                UserName,
                UserEmail,
                UserWebsite,
                UserMode,
                UserTrusted,
                UserEmailHide,
                UserLastLogin
            FROM
                userprofiles
            WHERE
                UserID = {$this->UserID}
        ", SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            $this->setVars($this->db->record);
        }
    }

    /**
     * Get addons for this author.
     */
    function getAddons()
    {
        // Gather addons metadata, user info.
        $this->db->query("
            SELECT 
                main.ID,
                main.Name,
                main.Description
            FROM 
                main 
            INNER JOIN authorxref ON authorxref.ID = main.ID
            INNER JOIN userprofiles ON userprofiles.UserID = authorxref.UserID
            WHERE 
                userprofiles.UserID = '{$this->UserID}'
        ", SQL_ALL, SQL_ASSOC);

        if (!empty($this->db->record)) {
            $this->setVar('AddOns',$this->db->record);
        }
    }
}
?>
