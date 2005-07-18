<?php
/**
 * AMO home.
 * @package amo
 * @subpackage docs
 * @todo clean up these stupid queries
 */
$db->query("
    SELECT DISTINCT
        TM.ID id, 
        TM.Name name, 
        TM.downloadcount dc
    FROM
        main TM
    INNER JOIN version TV ON TM.ID = TV.ID
    INNER JOIN applications TA ON TV.AppID = TA.AppID
    INNER JOIN os TOS ON TV.OSID = TOS.OSID
    WHERE
        downloadcount > '0' AND
        approved = 'YES' AND
        Type = 'E'
    ORDER BY
        downloadcount DESC 
    LIMIT 
        5
", SQL_ALL, SQL_ASSOC);

$popularExtensions = $db->record;

$db->query("
    SELECT DISTINCT
        TM.ID id, 
        TM.Name name, 
        TM.downloadcount dc
    FROM
        main TM
    INNER JOIN version TV ON TM.ID = TV.ID
    INNER JOIN applications TA ON TV.AppID = TA.AppID
    INNER JOIN os TOS ON TV.OSID = TOS.OSID
    WHERE
        downloadcount > '0' AND
        approved = 'YES' AND
        Type = 'T'
    ORDER BY
        downloadcount DESC 
    LIMIT 
        5
", SQL_ALL, SQL_ASSOC);

$popularThemes = $db->record;

$db->query("
    SELECT 
        TM.ID,
        TM.Type,
        TM.Name,
        MAX(TV.Version) Version,
        MAX(TV.DateAdded) DateAdded
    FROM  
        `main` TM
    INNER JOIN version TV ON TM.ID = TV.ID
    INNER JOIN applications TA ON TV.AppID = TA.AppID
    INNER JOIN os TOS ON TV.OSID = TOS.OSID
    WHERE
        `approved` = 'YES' 
    GROUP BY
        TM.ID
    ORDER BY
        DateAdded DESC
    LIMIT
        8
", SQL_ALL, SQL_ASSOC);

$newest = $db->record;

// Assign template variables.
$tpl->assign(
    array(  'popularExtensions' => $popularExtensions,
            'popularThemes'     => $popularThemes,
            'newest'            => $newest,
            'content'           => 'index.tpl')
);
?>
