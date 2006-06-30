<?php
/**
 * Maintenance script for addons.mozilla.org.
 *
 * The purpose of this document is to perform periodic tasks that should not be
 * done everytime a download occurs in install.php.  This should reduce
 * unnecessary DELETE and UPDATE queries and lighten the load on the database
 * backend.
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

$versions = array();
$hashes = array();

$db = new AMO_SQL();

$db->query("SELECT name,type,vid,uri FROM main m INNER JOIN version v ON v.id = m.id WHERE m.id=735", SQL_ALL, SQL_ASSOC);
$versions=$db->record;

foreach($versions as $version) {
    // Here we are making some assumptions on the file path.
    // Change the first part if the server you are running on has the .xpi repository in a different location.
    $buf = sha1_file('/data/amo/files/ftp/'.($version['type']=='E'?'extensions/':'themes/').str_replace(' ','_',strtolower($version['name'])).'/'.basename($version['uri']));
    $db->query("UPDATE version SET hash='sha1:{$buf}' WHERE vid={$version['vid']}");
}

exit;
?>
