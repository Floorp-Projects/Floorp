<?php
/**
 * This script tells clients whether or not a given add-on is blocklisted.
 *
 * It should always be well-formed and won't be seen by users,
 * at least initially.
 *
 * At some point we should consider generating an XSLT in case we want to
 * publish this list.
 *
 * @todo stylesheet for client viewing of blocklists
 * @package amo 
 * @subpackage pub
 */
startProcessing('blocklist.tpl', $memcacheId, $compileId, 'xml');



/**
 *  VARIABLES
 *
 *  Initialize, set up and clean variables.
 */

// Required variables that we need to run the script.
$required_vars = array('reqVersion',    // Used as a marker for the current URI scheme, in case it changes later.
                       'appGuid',       // GUID of the client requesting the blocklist.
                       'appVersion');   // Version of the client requesting the blocklist (not used).

// Debug flag.
$debug = (isset($_GET['debug']) && $_GET['debug'] == 'true') ? true : false;

// Array to hold errors for debugging.
$errors = array();

// Iterate through required variables, and escape/assign them as necessary.
foreach ($required_vars as $var) {
    if (empty($_GET[$var])) {
        $errors[] = 'Required variable '.$var.' not set.'; // set debug error
    }
}



// If we have all of our data, clean it up for our queries.
if (empty($errors)) {

    // We will need our DB in order to perform our query.
    require_once('includes.php');

    // Iterate through required variables, and escape/assign them as necessary.
    foreach ($required_vars as $var) {
        $sql[$var] = mysql_real_escape_string($_GET[$var]);
    }



    /*
     *  QUERIES  
     *  
     *  All of our variables are cleaned.
     *  Now attempt to retrieve blocklist information for this application.
     */ 
    $query = "
        SELECT 
            blitems.id as itemId,
            blitems.guid as itemGuid,
            blitems.min as itemMin,
            blitems.max as itemMax,
            blapps.id as appId,
            blapps.item_id as appItemId,
            blapps.guid as appGuid,
            blapps.min as appMin,
            blapps.max as appMax
        FROM 
            blitems
        LEFT JOIN blapps on blitems.id = blapps.item_id
        WHERE
            blapps.guid = '{$sql['appGuid']}'
            OR blapps.guid IS NULL
        ORDER BY
            itemGuid, appGuid, itemMin, appMin
    ";

    $db->query($query, SQL_ALL, SQL_ASSOC);

    if (DB::isError($db->record)) {
        $errors[] = 'MySQL query for blocklist failed.'; 
    } elseif (empty($db->record)) {
        $errors[] = 'No matching blocklist for given application GUID.'; 
    } else {
        $blocklist = array();

        foreach ($db->record as $row) {

            // If we have item itemMin/itemMax values or an appId possible ranges, we create
            // hashes for each itemId and its related range.
            // 
            // Since itemGuids can have different itemIds, they are the first hash.  Each
            // itemId is effectively an item's versionRange.  For each one of these we create
            // a corresponding array containing the range values, which could be NULL.
            if (!empty($row['itemMin']) && !empty($row['itemMax']) || !empty($row['appItemId'])) {
                $blocklist['items'][$row['itemGuid']][$row['itemId']] = array(
                    'itemMin' => $row['itemMin'],
                    'itemMax' => $row['itemMax']
                );

            // Otherwise, our items array only contains a top-level containing the itemGuid.
            //
            // Doing so tells our template to terminate the item with /> because there is
            // nothing left to display.
            } else {
                $blocklist['items'][$row['itemGuid']] = null;
            }

            // If we retrieved non-null blapp data, store it in the apps array.
            //
            // These are referenced later by their foreign key relationship to items (appItemId).
            if ($row['appItemId']) {
                $blocklist['apps'][$row['itemGuid']][$row['appItemId']][$row['appGuid']][] = array(
                    'appMin' => $row['appMin'],
                    'appMax' => $row['appMax']
                );
            }
        }
        
        // Send our array to the template.
        $tpl->assign('blocklist',$blocklist);
    }
} 



/*
 *  DEBUG
 *
 *  If we get here, something went wrong.  For testing purposes, we can
 *  optionally display errers based on $_GET['debug'].
 *
 *  By default, no errors are ever displayed because humans do not read this
 *  script.
 *
 *  Until there is some sort of API for how clients handle errors, 
 *  things should remain this way.
 */
if ($debug == true) {
    echo '<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">';
    echo '<html lang="en">';

    echo '<head>';
    echo '<title>blocklist.php Debug Information</title>';
    echo '<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">';
    echo '</head>';

    echo '<body>';

    echo '<h1>Parameters</h1>';
    echo '<pre>';
    print_r($_GET);
    echo '</pre>';

    if (!empty($query)) {
        echo '<h1>Query</h1>';
        echo '<pre>';
        echo $query;
        echo '</pre>';
    }

    if (!empty($blocklist)) {
        echo '<h1>Result</h1>';
        echo '<pre>';
        print_r($blocklist);
        echo '</pre>';
    }

    if (!empty($errors) && is_array($errors)) {
        echo '<h1>Errors Found</h1>';
        echo '<pre>';
        print_r($errors);
        echo '</pre>';
    } else {
        echo '<h1>No Errors Found</h1>';
    }

    echo '</body>';

    echo '</html>';
    exit;
}
?>
