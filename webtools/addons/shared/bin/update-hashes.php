<?php
/**
 * The purpose of this script is to retro-actively update existing add-ons
 * with valid hashes.
 *
 * We may not necessarily use this, but it was written just in case we need
 * to run the update in the future.
 *
 * This script should not ever be accessed over HTTP, and instead run via cron.
 * Only sysadmins should be responsible for operating this script.
 *
 * @package amo
 * @subpackage bin
 */


// Before doing anything, test to see if we are calling this from the command
// line.  If this is being called from the web, HTTP environment variables will
// be automatically set by Apache.  If these are found, exit immediately.
if (isset($_SERVER['HTTP_HOST'])) {
    exit;
}

// If we get here, we're on the command line, which means we can continue.
require_once('../../public/inc/config.php');

// For the addon object and db stuff
require_once('../../public/inc/includes.php');

$path = '/data/amo/files/ftp/';  // Path to the directory that contains approved XPIs.  Adjust to point to proper path.
$versions = array();
$hashes = array();

$db = new AMO_SQL();

$db->query("SELECT name,version,type,vid,uri FROM main m INNER JOIN version v ON v.id = m.id WHERE v.approved='yes'", SQL_ALL, SQL_ASSOC);
$versions=$db->record;

foreach($versions as $version) {

    $file = '/data/amo/files/ftp/'.($version['type']=='E'?'extensions/':'themes/').str_replace(' ','_',strtolower($version['name'])).'/'.basename($version['uri']);

    // If the file exists, get its sum and update its record.
    if (file_exists($file)) {
        $buf = sha1_file($file);
        $db->query("UPDATE version SET hash='sha1:{$buf}' WHERE vid={$version['vid']}");
        echo "Updated {$version['name']} {$version['version']} hash to sha1:{$buf}\n";
    }
}

exit;
?>
