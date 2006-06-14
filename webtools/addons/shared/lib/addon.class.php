<?php
/**
 * Addon super class.  The class to end all classes.
 * @package amo
 * @subpackage lib
 * @todo properly separate accessors and mutators.
 */
class AddOn extends AMO_Object {
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
    var $installFunc;
    var $isThunderbirdAddon;

    // AddOn author metadata.
    var $UserID;
    var $UserName;
    var $UserEmail;
    var $UserWebsite;
    var $UserEmailHide;

    // Current version information.
    var $vID;
    var $Version;
    var $Size;
    var $URI;
    var $Notes;
    var $VersionDateAdded;
    var $AppName;

    var $AppVersions;

    var $OsVersions;

    // Preview information.
    var $PreviewID;
    var $PreviewURI;
    var $PreviewHeight;
    var $PreviewWidth;
    var $Caption;
    var $Previews = array(); // Store the information for previews

    // Comments.
    var $Comments;

    // Categories.
    var $AddonCats;
    
    // History of releases
    var $History;

    /**
    * Class constructor.
    * 
    * @param int $ID AddOn ID
    */
    function AddOn($ID=null) {
        // Our DB and Smarty objects are global to save cycles.
        global $db, $tpl;

        // Pass by reference in order to save memory.
        $this->db =& $db;
        $this->tpl =& $tpl;

        // If $ID is set, attempt to retrieve data.
        if (!empty($ID)) {
            $this->ID = $ID;
            $this->getAddOn();
        }
    }

    /**
     * Get all commonly used AddOn information.
     */
    function getAddOn() {
        $this->getAddonCats();
        $this->getComments(0,3);
        $this->getCurrentVersion();
        $this->getMainPreview();
        $this->getUserInfo();
        $this->getAuthors();
        $this->getHistory();
        $this->getAppVersions();
        $this->getOsVersions();
        $this->installFunc = $this->Type == 'T' ? 'installTheme' : 'install';
    }
    
    /**
     * Get the "highlight" for the current AddOn.
     */
    function getMainPreview() {
        // Gather previews information.
        $this->db->query(" 
            SELECT
                PreviewID,
                PreviewURI,
                Caption
            FROM
                previews
            WHERE
                ID = '{$this->ID}' AND
                preview = 'YES'
            LIMIT 1
        ", SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            $this->setVars($this->db->record);

            if (file_exists(ROOT_PATH.'/htdocs'.$this->PreviewURI)) {
                $size = getimagesize(ROOT_PATH.'/htdocs'.$this->PreviewURI);
                $this->setVar('PreviewWidth',$size[0]);
                $this->setVar('PreviewHeight',$size[1]);
            }
        }

    }

    /**
     * Get all preview information attached to the current AddOn.
     */
    function getPreviews() {
        // Gather preview information
        $this->db->query("
            SELECT
                PreviewURI,
                caption
            FROM
                previews
            WHERE
                ID = {$this->ID}
            ORDER BY
                PreviewID ASC
        ", SQL_NONE);

        while ($this->db->next(SQL_ASSOC)) {
            $result = $this->db->record;
            $uri = $result['PreviewURI'];
            list($src_width, $src_height, $type, $attr) = getimagesize(ROOT_PATH.'/htdocs'.$uri);
            $this->Previews[] = array(
                'PreviewURI' => $uri,
                'caption' => $result['caption'],
                'width' => $src_width,
                'height' => $src_height
            );
        }
    }
     
    /**
     * Get all previous versions of the current AddOn.
     */
    function getHistory() {
        $this->db->query("
             SELECT DISTINCT
                 TV.vID,
                 TV.Version,
                 TV.MinAppVer,
                 TV.MaxAppVer,
                 TV.Size,
                 TV.URI,
                 TV.Notes,
                 UNIX_TIMESTAMP(TV.DateAdded) AS VerDateAdded,
                 TA.AppName,
                 TOS.OSName
            FROM
                version TV
            INNER JOIN applications TA ON TV.AppID = TA.AppID
            INNER JOIN os TOS ON TV.OSID = TOS.OSID
            WHERE
                TV.ID = {$this->ID} AND
                approved = 'YES'
            GROUP BY
                TV.Version
            ORDER BY
                TV.vID DESC
        ", SQL_ALL, SQL_ASSOC);

        $this->History = $this->db->record;
    }

    /**
     * Get information about the most recent verison of the current AddOn.
     */
    function getCurrentVersion() {
        $this->db->query("
            SELECT 
                version.vID, 
                version.Version, 
                version.Size, 
                version.URI, 
                version.Notes, 
                version.DateAdded as VersionDateAdded,
                applications.AppName
            FROM  
                version
            INNER JOIN applications ON version.AppID = applications.AppID
            WHERE 
                version.ID = '{$this->ID}' AND 
                version.approved = 'YES'
            ORDER BY
                version.DateAdded DESC
            LIMIT 1
        ", SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            $this->setVars($this->db->record);
        }
    }

    /**
     * Retrieve user information.
     *
     * @todo have this function set a User object instead
     * @deprecated (clouserw) - use getAuthors() instead
     */ 
    function getUserInfo() {
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
                main.ID = '{$this->ID}'
        ", SQL_INIT, SQL_ASSOC);

        if (!empty($this->db->record)) {
            $this->setVars($this->db->record);
        }
    }

    /**
     * Retrieve user information.
     *
     * @todo have this function set a User object instead
     */ 
    function getAuthors() {
        // Gather addons metadata, user info.
        $this->db->query("
            SELECT 
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
                main.ID = '{$this->ID}'
        ", SQL_ALL, SQL_ASSOC);

        foreach ($this->db->record as $var => $val) {
                $_final[$val['UserID']] = array (
                    'UserID'        => $val['UserID'],
                    'UserName'      => $val['UserName'],
                    'UserEmail'     => $val['UserEmail'],
                    'UserWebsite'   => $val['UserWebsite'],
                    'UserEmailHide' => $val['UserEmailHide']
                );
        }

        $this->setVar('Authors',$_final);
    }

    /**
     * Get comments attached to this Addon.
     *
     * @param int $offset starting point of result set.
     * @param int $numrows number of rows to show.
     * @todo add left/right limit clauses i.e. LIMIT 10,20 to work with pagination
     */
    function getComments($offset=0, $numrows=10, $orderBy=null) {

        // Set order by.
        switch ($orderBy) {
            case 'ratinghigh':
                $_orderBySql = " ORDER BY CommentVote desc, helpful_yes desc ";
                break;
            case 'ratinglow':
                $_orderBySql = " ORDER BY CommentVote asc ";
                break;
            case 'dateoldest':
                $_orderBySql = " ORDER BY CommentDate asc, helpful_yes desc ";
                break;
            default:
            case 'datenewest':
                $_orderBySql = " ORDER BY CommentDate desc, helpful_yes desc ";
                break;
            case 'leasthelpful':
                $_orderBySql = " ORDER BY helpful_no desc, helpful_yes asc, CommentDate desc ";
                break;
            case 'mosthelpful':
                $_orderBySql = " ORDER BY helpful_yes desc, CommentDate desc ";
                break;
        }
    
        // Gather 10 latest comments.
        $this->db->query("
            SELECT SQL_CALC_FOUND_ROWS
                CommentID,
                CommentName,
                CommentTitle,
                CommentNote,
                CommentDate,
                CommentVote,
                `helpful-yes` as helpful_yes,
                `helpful-no` as helpful_no,
                `helpful-yes` + `helpful-no` as helpful_total,
                UserName
            FROM
                feedback
            LEFT JOIN
                userprofiles
            ON
                userprofiles.UserID = feedback.UserID
            WHERE
                ID = '{$this->ID}' AND
                CommentVote IS NOT NULL
            {$_orderBySql}
            LIMIT {$offset}, {$numrows} 
        ", SQL_ALL, SQL_ASSOC);
        
        $this->setVar('Comments',$this->db->record);
    }

    /**
     * Retrieve all categories attached to the current AddOn.
     */
    function getAddonCats() {
        // Gather addon categories.
        $this->db->query("
            SELECT DISTINCT
                categories.CatName,
                categories.CategoryID
            FROM
                categoryxref
            INNER JOIN categories ON categoryxref.CategoryID = categories.CategoryID 
            INNER JOIN main ON categoryxref.ID = main.ID
            WHERE
                categoryxref.ID = {$this->ID}
            GROUP BY
                categories.CatName
            ORDER BY
                categories.CatName
        ", SQL_ALL, SQL_ASSOC);

        $this->setVar('AddonCats',$this->db->record);
    }

    /**
     * Retrieve all compatible applications and their versions.  This only returns
     * the applications that are marked as "supported" in the db!
     */
    function getAppVersions() {
        $_final = array();
        $this->db->query("
          SELECT 
            `applications`.`AppName`, 
            `version`.`MinAppVer`, 
            `version`.`MaxAppVer`,
            `os`.`OSName`
          FROM 
            `version` 
          INNER JOIN
            `applications`
          ON
            `version`.`AppID` = `applications`.`AppID`
          INNER JOIN
            `os`
          ON
            `version`.`OSID` = `os`.`OSID`
          WHERE
            `version`.`Version`='{$this->Version}'
          AND 
            `version`.`ID`={$this->ID}
          AND 
            `version`.`approved`='yes'
          AND
            `applications`.`supported` = 1
          ORDER BY
            `applications`.`AppName` 
        ", SQL_ALL, SQL_ASSOC);

        foreach ($this->db->record as $var => $val) {

            // If we find Thunderbird in here somewhere, set the isThunderbirdAddon flag to true.
            // This is so we can trigger install instructions under the install box in the template.
            if ($val['AppName'] == 'Thunderbird') {
                $this->isThunderbirdAddon = true;
            }
        
            $_key = "{$val['AppName']} {$val['MinAppVer']} - {$val['MaxAppVer']}";

            // We've already got at least one hit, just add the OS
            if (array_key_exists($_key, $_final)) {
                array_push($_final[$_key]['os'], $val['OSName']);
            } else {
                $_final[$_key] = array (
                    'AppName'   => $val['AppName'],
                    'MinAppVer' => $val['MinAppVer'],
                    'MaxAppVer' => $val['MaxAppVer'],
                    'os'        => array ($val['OSName'])
                );
            }
        }

        $this->setVar('AppVersions',$_final);
    }

    /* A very similar function as getAppVersions() but this
    has an additional GROUP BY which limits the OS's that
    come back */
    function getOsVersions() {
        $_final = array();
        $this->db->query("
          SELECT 
            `applications`.`appname`,
            `version`.`URI`,
            `version`.`Size`,
            `os`.`OSName`
          FROM 
            `version` 
          INNER JOIN
            `applications`
          ON
            `version`.`AppID` = `applications`.`AppID`
          INNER JOIN
            `os`
          ON
            `version`.`OSID` = `os`.`OSID`
          WHERE
            `version`.`Version`='{$this->Version}'
          AND 
            `version`.`ID`={$this->ID}
          AND
            `applications`.`supported` = 1
          AND 
            `version`.`approved`='yes'
          GROUP BY 
            `os`.`OSID`
        ", SQL_ALL, SQL_ASSOC);

        foreach ($this->db->record as $var => $val) {
            $_final[$val['OSName']] = array (
                'AppName'   => $val['appname'],
                'URI'       => $val['URI'],
                'Size'      => $val['Size'],
                'OSName'    => $val['OSName'],
                'Version'   => $this->Version
            );
        }

        $this->setVar('OsVersions',$_final);
    }
}
?>
