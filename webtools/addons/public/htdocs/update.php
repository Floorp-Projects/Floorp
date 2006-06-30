<?php
/**
 * VersionCheck.php is a dynamic RDF that compares version information for
 * extensions and determines whether or not an update is needed.  If an update
 * is needed, the correct update file is referenced based on the UMO database
 * and repository.  The script is set to die silently instead of echoing errors
 * clients don't use anyway.  For testing, if you would like to debug, supply
 * the script with ?debug=true
 *
 * @package amo 
 * @subpackage pub
 */

startProcessing('update.tpl', $memcacheId, $compileId, 'xml');

/**
 *  VARIABLES
 *
 *  Initialize, set up and clean variables.
 */

// Map the mysql main.type enum into the right type.
$ext_typemap = array('T' => 'theme',
                     'E' => 'extension',
                     'P' => 'plugin');

// Required variables that we need to run the script.
$required_vars = array('reqVersion',
                       'id',
                       'version',
                       'appID',
                       'appVersion');

// Debug flag.
$debug = (isset($_GET['debug']) && $_GET['debug'] == 'true') ? true : false;

// Array to hold errors for debugging.
$errors = array();

// Set OS.  get_os_id() can only return an int.
$sql['os_id'] = get_os_id();

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
     *  Now attempt to retrieve update information.
     */ 
    $os_query = ($sql['os_id']) ? " OR version.OSID = '{$sql['os_id']}' " : '';  // Set up os_id.

    // Query for possible updates.
    //
    // The query sorts by version.vid, which is an auto_increment primary key for that table.
    //
    // The reason why this was used is that the version.version column was a varchar and
    // it was failing to sort correctly in some cases.
    //
    // There is a possibility that the version.vid sort could be incorrect, but only in edge
    // cases where the version was added retroactively, and I've actually _never_ seen such
    // a case. (It's happened, see bug #334747 -- clouserw)
    $query = "
        SELECT
            main.guid AS extguid,
            main.type AS exttype,
            version.version AS extversion,
            version.uri AS exturi,
            version.minappver AS appminver,
            version.maxappver AS appmaxver,
            applications.guid AS appguid,
            version.hash AS hash
        FROM
            main
        INNER JOIN version ON main.id = version.id
        INNER JOIN applications ON version.appid = applications.appid
        WHERE
            main.guid = '{$sql['id']}' AND
            applications.guid = '{$sql['appID']}' AND
            (version.OSID = 1 {$os_query} ) AND
            version.approved = 'YES' AND
            '{$sql['appVersion']}+' >= version.minappver
        ORDER BY
            version.vid DESC
        LIMIT 1       
    ";

    $db->query($query, SQL_INIT, SQL_ASSOC);

    if (DB::isError($db->record)) {
        $errors[] = 'MySQL query for item information failed.'; 
    } elseif (empty($db->record)) {
        $errors[] = 'No matching update for given item/GUID.'; 
    } else {
        $update = array();
        
        // An update exists.  Retrieve it.
        foreach ($db->record as $key=>$val) {
            $update[$key] = $val;
        }

        $update['exttype'] = $ext_typemap[$update['exttype']];

        $tpl->assign('update',$update);
    }
} 



/*
 *  DEBUG
 *
 *  If we get here, something went wrong.  For testing purposes, we can
 *  optionally display errors based on $_GET['debug'].
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
    echo '<title>VersionCheck.php Debug Information</title>';
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

    if (!empty($update)) {
        echo '<h1>Result</h1>';
        echo '<pre>';
        print_r($update);
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



/*
 *  FUNCTIONS
 */



/**
 * Determine the os_id based on passed OS or guess based on UA string.
 * @return int|bool $id ID of the OS in the UMO database
 */
function get_os_id()
{
    /* OS from UMO database
    2 	Linux
    3 	MacOSX
    4 	BSD
    5 	Windows
    6 	Solaris
    */

   // possible matches
    $os = array(
        'linux'=>2,
        'mac'=>3,
        'bsd'=>4,
        'win'=>5,
        'solaris'=>6
    );

    // Check $_GET for a match.
    if (!empty($_GET['appOS']) && ctype_alpha($_GET['appOS'])) {
        $appOS = strtolower($_GET['appOS']);
        foreach ($os as $string=>$id) {
            if ($appOS == $string) {
                return $id;
            }
        }
    }

    // Check UA string for a match
    foreach ($os as $string=>$id) {
        if (preg_match("/^.*{$string}.*$/i",$_SERVER['HTTP_USER_AGENT'])) {
            return $id;
        }
    }

    // If we get here, there is no defined OS
    // This OS is undetermined, and the query will instead rely on "ALL" (1) in the OR
    return false;
}
?>
