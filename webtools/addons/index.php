<?php
/**
 * Overview provides an inside look at what is going on for an application.
 *
 * @package amo
 * @subpackage docs
 *
 * @todo Do something to spice up this page.
 * @todo Get main template spruced up.
 */

// Arrays to store clean inputs.
$clean = array();  // General array for verified inputs.
$sql = array();  // Trusted for SQL.

// If app is not set or empty, set it to null for our switch.
$_GET['app'] = (!empty($_GET['app'])) ? $_GET['app'] : null;

// Determine our application.
switch( $_GET['app'] ) {
    case 'Mozilla':
        $clean['app'] = 'Mozilla';
        break;
    case 'Thunderbird':
        $clean['app'] = 'Thunderbird';
        break;
    case 'Firefox':
    default:
        $clean['app'] = 'Firefox';
        break;
}

// $sql['app'] can equal $clean['app'] since it was assigned in a switch().
$sql['app'] =& $clean['app'];

// Get most popular extensions based on application.
$db->query("
    SELECT DISTINCT
        TM.ID id, 
        TM.Name name, 
        TM.Rating,
        LEFT(TM.Description,96) as Description,
        TM.downloadcount dc
    FROM
        main TM
    INNER JOIN version TV ON TM.ID = TV.ID
    INNER JOIN applications TA ON TV.AppID = TA.AppID
    INNER JOIN os TOS ON TV.OSID = TOS.OSID
    WHERE
        AppName = '{$sql['app']}' AND 
        downloadcount > '0' AND
        approved = 'YES' AND
        Type = 'E'
    ORDER BY
        downloadcount DESC 
    LIMIT 
        5 
", SQL_ALL, SQL_ASSOC);

$popularExtensions = $db->record;

// Get most popular themes based on application.
$db->query("
    SELECT DISTINCT
        TM.ID id, 
        TM.Name name, 
        TM.Rating,
        LEFT(TM.Description,96) as Description,
        TM.downloadcount dc
    FROM
        main TM
    INNER JOIN version TV ON TM.ID = TV.ID
    INNER JOIN applications TA ON TV.AppID = TA.AppID
    INNER JOIN os TOS ON TV.OSID = TOS.OSID
    WHERE
        AppName = '{$sql['app']}' AND 
        downloadcount > '0' AND
        approved = 'YES' AND
        Type = 'T'
    ORDER BY
        downloadcount DESC 
    LIMIT 
        5 
", SQL_ALL, SQL_ASSOC);

$popularThemes = $db->record;

// Get newest addons based on application.
$db->query("
    SELECT 
        TM.ID,
        TM.Type,
        TM.Name,
        TM.Rating,
        LEFT(TM.Description,96) as Description,
        MAX(TV.Version) Version,
        MAX(TV.DateAdded) DateAdded
    FROM  
        `main` TM
    INNER JOIN version TV ON TM.ID = TV.ID
    INNER JOIN applications TA ON TV.AppID = TA.AppID
    INNER JOIN os TOS ON TV.OSID = TOS.OSID
    WHERE
        AppName = '{$sql['app']}' AND 
        `approved` = 'YES' 
    GROUP BY
        TM.ID
    ORDER BY
        DateAdded DESC
    LIMIT
        5
", SQL_ALL, SQL_ASSOC);

$newest = $db->record;

$tabs = array(
    array(
        'app' => 'Firefox'
    ),
    array(
        'app' => 'Thunderbird'
    ),
    array(
        'app' => 'Mozilla'
    )
);

// Assign template variables.
$tpl->assign(
    array(  'popularExtensions' => $popularExtensions,
            'popularThemes'     => $popularThemes,
            'newest'            => $newest,
            'app'               => $clean['app'],
            'title'             => $clean['app'].' Addons',
            'tabs'              => $tabs,
            'content'           => 'index.tpl')
);

$wrapper = 'inc/wrappers/nonav.tpl';
?>
