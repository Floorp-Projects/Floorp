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

    // Current version information.
    var $vID;
    var $Version;
    var $MinAppVer;
    var $MaxAppVer;
    var $Size;
    var $URI;
    var $Notes;
    var $VersionDateAdded;
    var $AppName;
    var $OSName;

    // Preview information.
    var $PreviewID;
    var $PreviewURI;
    var $Caption;

    // Comments.
    var $Comments;

    // Categories.
    var $Cats;

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
        // Gather addons metadata, user info.
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
            $this->setVars($this->db->record);
        }

        // Gather version information for most current version.
        $this->db->query("
            SELECT 
                version.vID, 
                version.Version, 
                version.MinAppVer, 
                version.MaxAppVer, 
                version.Size, 
                version.URI, 
                version.Notes, 
                version.DateAdded as VersionDateAdded,
                applications.AppName, 
                os.OSName
            FROM  
                version
            INNER JOIN applications ON version.AppID = applications.AppID
            INNER JOIN os ON version.OSID = os.OSID
            WHERE 
                version.ID = '{$ID}' AND 
                version.approved = 'YES'
            ORDER BY
                version.DateAdded DESC
            LIMIT 1
        ", SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            $this->setVars($this->db->record);
        }


        // Gather previews information.
        $this->db->query(" 
            SELECT
                PreviewID,
                PreviewURI,
                Caption
            FROM
                previews
            WHERE
                ID = '{$ID}' AND
                preview = 'YES'
            LIMIT 1
        ", SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            $this->setVars($this->db->record);
        }

        // Gather 10 latest comments.
        $this->db->query("
            SELECT
                CommentName,
                CommentTitle,
                CommentNote,
                CommentDate,
                CommentVote
            FROM
                feedback
            WHERE
                ID = '{$ID}' AND
                CommentNote IS NOT NULL
            ORDER BY
                CommentDate DESC
            LIMIT 5
        ", SQL_ALL, SQL_ASSOC);
        
        $this->setVar('Comments',$this->db->record);

        // Gather addon categories.
        $this->db->query("
            SELECT DISTINCT
                categories.CatName
            FROM
                categoryxref
            INNER JOIN categories ON categoryxref.CategoryID = categories.CategoryID 
            INNER JOIN main ON categoryxref.ID = main.ID
            WHERE
                categoryxref.ID = {$ID}
            ORDER BY
                categories.CatName
        ", SQL_ALL, SQL_ASSOC);

        $this->Cats = $this->db->record;
    }
}
?>
