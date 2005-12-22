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

startProcessing('index.tpl', 'nonav');

// init the stuff we need to generate content
require_once('includes.php');

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
        TM.ID ID, 
        TM.Name name, 
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


// Assign template variables.
$tpl->assign(
    array(  'popularExtensions'     => $popularExtensions,
            'title'             => $clean['app'].' Addons',
            'currentTab'        => 'home')
);


?>
