<?php
require_once('../config.inc.php');
require_once($config['base_path'].'/includes/iolib.inc.php');
require_once($config['base_path'].'/includes/db.inc.php');
require_once($config['base_path'].'/includes/security.inc.php');

// Turn off Error Reporting
error_reporting(0);

// Headers
// Start Session
session_name('reportSessID');
session_start();
header("Cache-control: private"); //IE 6 Fix
printheaders();

if($securitylib->isLoggedIn() === true){
    $db = NewDBConnection($config['db_dsn']);
    $db->SetFetchMode(ADODB_FETCH_ASSOC);

    $query = $db->Execute("SELECT screenshot_data, screenshot.screenshot_format
                           FROM screenshot
                           WHERE screenshot_report_id = ".$db->quote($_GET['report_id']));
    if(!$query){
        exit;
    }

    // Output the MIME header
    $imageExtension = array_search($query->fields['screenshot_format'], $config['screenshot_imageTypes']);
    
    // This should never happen, but we test for it regardless
    if($imageExtension === false){
        // XXX -> we should redirect to an error image or someting to that effect as
        //        in most cases, nobody would even see this error.
        print "Invalid Image";
    }

    // Headers
    header("Content-Type: ".$query->fields['screenshot_format']);
    header("Content-disposition: inline; filename=".$_GET['report_id'].".".$imageExtension);

    // Output the image
    echo $query->fields['screenshot_data'];
} else {
    // XXX -> we should redirect to an error image or someting to that effect as
    //        in most cases, nobody would even see this error.
    print "You are not authorized to view this";
}
?>