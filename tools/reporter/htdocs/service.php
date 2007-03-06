<?php
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Reporter (r.m.o).
 *
 * The Initial Developer of the Original Code is
 *            Robert Accettura <robert@accettura.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

require_once('../config.inc.php');
require_once($config['base_path'].'/includes/iolib.inc.php');
require_once($config['base_path'].'/includes/db.inc.php');
require_once($config['base_path'].'/includes/contrib/nusoap/lib/nusoap.php');

// Turn off Error Reporting because it breaks xml formatting and causes errors
error_reporting(0);

// If debugging is enabled, we turn it on (mostly a negative thing)
if($config['debug']){
    $debug = 1;
}

// Create the server instance
$server = new soap_server;

// UTF-8 support is good
$server->soap_defencoding = "UTF-8";
$server->decode_utf8 = false;

// WSDL Support
if($config['use_wsdl']){
    $server->configureWSDL('reporterwsdl', 'urn:reporterwsdl');
}

// Register the method to expose
// Note: with NuSOAP 0.6.3, only method name is used w/o WSDL
$server->register(
    'register',                              // method name
    array('language' => 'xsd:string',),  // input parameters
    array('return' => 'xsd:string'),         // output parameters
    'uri:MozillaReporter',                   // namespace
    'uri:MozillaReporter/register',          // SOAPAction
    'rpc',                                   // style
    'encoded'                                // use
);

/******************************************************
 *            submitReport Method
 *
 * In version 0.2, the service is envoked merely though /service/
 * In version 0.3 the service has a version number appended to it
 * such that it's now /service/0.3/.
 * We detect this and register the correct version of the API for
 * that client.  This is a a simple workaround for nuSOAP's inability
 * to know what is what in a SOAP request (it simply goes by order)
 * and saves us a lot of trouble.  No this isn't pretty, but it's a
 * really simple fix to this one limitation.
 ******************************************************/

/***
 * v0.3 Firefox > 2.0 (Minefield and later)
 ***/
if($_REQUEST['v'] == '0.3'){
    $server->register(
        'submitReport',                                 // method name
        array('rmoVers' => 'xsd:string',
              'url' => 'xsd:string',
              'problem_type' => 'xsd:string',
              'description' => 'xsd:string',
              'behind_login' => 'xsd:string',
              'platform' => 'xsd:string',
              'oscpu' => 'xsd:string',
              'gecko' => 'xsd:string',
              'product' => 'xsd:string',
              'useragent' => 'xsd:string',
              'buildconfig' => 'xsd:string',
              'language' => 'xsd:string',
              'email' => 'xsd:string',
              'sysid' => 'xsd:string',
              'charset' => 'xsd:string',
              'screenshot' => 'xsd:base64Binary',
              'screenshot_format' => 'xsd:string'),     // input parameters
        array('return' => 'xsd:string'),                // output parameters
        'uri:MozillaReporter',                          // namespace
        'uri:MozillaReporter/submitReport',             // SOAPAction
        'rpc',                                          // style
        'encoded'                                       // use
    );

    function submitReport($rmoVers, $url, $problem_type, $description, $behind_login,
                 $platform, $oscpu, $gecko, $product, $useragent, $buildconfig,
                 $language, $email, $sysid, $screenshot, $screenshot_format, $charset){

        // What we're really calling
        return processReport($rmoVers, $url, $problem_type, $description, $behind_login,
                             $platform, $oscpu, $gecko, $product, $useragent, $buildconfig,
                             $language, $email, $sysid, $screenshot, $screenshot_format, $charset);
    }
}

/***
 * v0.2 Shipped in Firefox 1.5.x and Firefox 2.0.x.
 *   Note it's critical we support service/  without a version param to handle older < 2.0+ clients.
 ***/
else if($_REQUEST['v'] == '0.2' || !isset($_REQUEST['v']) || $_REQUEST['v'] == ''){
    $server->register(
        'submitReport',                                 // method name
        array('rmoVers' => 'xsd:string',
              'url' => 'xsd:string',
              'problem_type' => 'xsd:string',
              'description' => 'xsd:string',
              'behind_login' => 'xsd:string',
              'platform' => 'xsd:string',
              'oscpu' => 'xsd:string',
              'gecko' => 'xsd:string',
              'product' => 'xsd:string',
              'useragent' => 'xsd:string',
              'buildconfig' => 'xsd:string',
              'language' => 'xsd:string',
              'email' => 'xsd:string',
              'sysid' => 'xsd:string'),                 // input parameters
        array('return' => 'xsd:string'),                // output parameters
        'uri:MozillaReporter',                          // namespace
        'uri:MozillaReporter/submitReport',             // SOAPAction
        'rpc',                                          // style
        'encoded'                                       // use
    );

    function submitReport($rmoVers, $url, $problem_type, $description, $behind_login,
                 $platform, $oscpu, $gecko, $product, $useragent, $buildconfig,
                 $language, $email, $sysid){

        // What we're really calling
        return processReport($rmoVers, $url, $problem_type, $description, $behind_login,
                             $platform, $oscpu, $gecko, $product, $useragent, $buildconfig,
                             $language, $email, $sysid);
    }
}
// If someone attempts a version > we support (url hacking) we just won't have a submitRequest method for them.


/***
 * This is the actual submitReport.  Everything after the earliest version supported should be =null, so it's ignored.
 ***/
function processReport($rmoVers, $url, $problem_type, $description, $behind_login,
                       $platform, $oscpu, $gecko, $product, $useragent, $buildconfig,
                       $language, $email, $sysid, $screenshot = null, $screenshot_format = null, $charset = null) {
    global $config;

    if ($config['service_active'] == false){
            return new soap_fault('SERVER', '', 'The service is currently unavailable.  Please try again in a few minutes.');
    }

    /**********
     * Sanitize and Validate
     **********/
    // Remove any HTML tags and whitespace
    $rmoVers = trim(strip_all_tags($rmoVers));
    $url = trim(strip_all_tags($url));
    $problem_type = trim(strip_all_tags($problem_type));
    $description = trim(strip_all_tags($description));
    $behind_login = trim(strip_all_tags($behind_login));
    $platform = trim(strip_all_tags($platform));
    $oscpu = trim(strip_all_tags($oscpu));
    $gecko = trim(strip_all_tags($gecko));
    $product = trim(strip_all_tags($product));
    $useragent = trim(strip_all_tags($useragent));
    $buildconfig = trim(strip_all_tags($buildconfig));
    $language = trim(strip_all_tags($language));
    $email = trim(strip_all_tags($email));
    $sysid = trim(strip_all_tags($sysid));
    $screenshot_format = trim(strip_all_tags($screenshot_format));
    $screenshot_width = trim(strip_all_tags($screenshot_width));
    $screenshot_height = trim(strip_all_tags($screenshot_height));
    $charset = trim(strip_all_tags($charset));

    // check verison
    if ($rmoVers < $config['min_vers']){
        return new soap_fault('Client', '', 'Your product is out of date, please upgrade.  See http://reporter.mozilla.org/install for details.', $rmoVers);
    }

    $parsedUrl = parse_url($url);
    if (!$url || !$parsedUrl['host']){
        return new soap_fault('Client', '', 'url must use a valid URL syntax http://mozilla.com/page', $url);
    }
    if (!$problem_type || $problem_type == -1 || $problem_type == "0") {
    }
    if ($behind_login != 1 && $behind_login != 0) {
        return new soap_fault('Client', '', 'behind_login must be type bool int', $behind_login);
    }
    if (!$platform) {
        return new soap_fault('Client', '', 'Invalid Platform Type', $platform);
    }
    if (!$product) {
        return new soap_fault('Client', '', 'Invalid Product', $product);
    }
    if (!$language) {
        return new soap_fault('Client', '', 'Invalid Localization', $language);
    }
    /*  We don't explicity require this since some older clients may not return this.
   if (!$gecko) {
        return new soap_fault('Client', '', 'Invalid Gecko ID', $gecko);
    }
    */
    if (!$oscpu) {
        return new soap_fault('Client', '', 'Invalid OS CPU', $oscpu);
    }
    if (!$useragent) {
        return new soap_fault('Client', '', 'Invalid Useragent', $useragent);
    }
    if (!$buildconfig) {
        return new soap_fault('Client', '', 'Invalid Build Config', $buildconfig);
    }
    if (!$sysid) {
        return new soap_fault('Client', '', 'No SysID Entered', $sysid);
    }

    // Image Validation
    if($screenshot != null) {
        // If no format specified, it's invalid
        if($screenshot_format == null) {
            return new soap_fault('Client', '', 'Invalid Screenshot', $screenshot_format);
        }
        // Must be in our list of approved formats.
        if(!in_array($screenshot_format, $config['screenshot_imageTypes'])){
            return new soap_fault('Client', '', 'Invalid Screenshot Format', $screenshot_format);
        }
    }

    // create report_id.    We just use a timestamp, because we don't need people counting reports, since it's inaccurate.
    // we can have dup's, so it's not a good thing for people to be saying 'mozilla.org reports 500,000 incompatable sites'
    $id = str_replace(".", "", array_sum(explode(' ', microtime())));

    // Make sure it's always 14 chars long
    $idlen = strlen($id);
    if($idlen < 14){
        for($i=$idlen;$i<14; $i++){
            $id = '0'.$id;
        }
    }
    unset($idlen);

    $report_id = 'RMO'.$id;
    unset($id);


    /**********
     * Open DB
     **********/
    $db = NewDBConnection($config['db_dsn']);
    $db->SetFetchMode(ADODB_FETCH_ASSOC);
    $db->debug = false;  // no good reason to ever let this be true, since it breaks things

    /**********
     * Check for valid sysid
     **********/
    $sysIdQuery = $db->Execute("SELECT sysid.sysid_id
                                FROM sysid
                                WHERE sysid.sysid_id = ".$db->quote($sysid));
    if(!$sysIdQuery){
        return new soap_fault('SERVER', '', 'Database Error SR1');
    }

    if ($sysIdQuery->RecordCount() != 1){
        return new soap_fault('Client', '', 'Invalid SysID', $sysid);
    }

    /**********
     * Check Hostname
     **********/
    $hostnameQuery = $db->Execute("SELECT host.host_id
                                   FROM host
                                   WHERE host.host_hostname = ".$db->quote($parsedUrl['host']));
    if(!$hostnameQuery){
        return new soap_fault('SERVER', '', 'Database Error SR2');
    }

    /**********
     * Add Host
     **********/
    if ($hostnameQuery->RecordCount() <= 0) {
        // generate hash
        $host_id = md5($parsedUrl['host'].microtime());
        // We add the URL
        $addUrlQuery = $db->Execute("INSERT INTO host (host.host_id, host.host_hostname, host.host_date_added)
                                         VALUES (
                                             ".$db->quote($host_id).",
                                             ".$db->quote($parsedUrl['host']).",
                                             now()
                                         )");
        if (!$addUrlQuery) {
            return new soap_fault('SERVER', '', 'Database Error SR3');
        }
    }
    else if ($hostnameQuery->RecordCount() == 1) {
        // pull the hash from DB
        $host_id = $hostnameQuery->fields['host_id'];
    } else{
        return new soap_fault('SERVER', '', 'Host Exception Error');
    }

    /**********
     * Add Report
     **********/
    $addReportQuery = $db->Execute("INSERT INTO report (
                                        report.report_id,
                                        report.report_url,
                                        report.report_host_id,
                                        report.report_problem_type,
                                        report.report_description,
                                        report.report_behind_login,
                                        report.report_charset,
                                        report.report_useragent,
                                        report.report_platform,
                                        report.report_oscpu,
                                        report.report_language,
                                        report.report_gecko,
                                        report.report_buildconfig,
                                        report.report_product,
                                        report.report_email,
                                        report.report_ip,
                                        report.report_file_date,
                                        report.report_sysid
                                    )
                                    VALUES (
                                        ".$db->quote($report_id).",
                                        ".$db->quote($url).",
                                        ".$db->quote($host_id).",
                                        ".$db->quote($problem_type).",
                                        ".$db->quote($description).",
                                        ".$db->quote($behind_login).",
                                        ".$db->quote($charset).",
                                        ".$db->quote($useragent).",
                                        ".$db->quote($platform).",
                                        ".$db->quote($oscpu).",
                                        ".$db->quote($language).",
                                        ".$db->quote($gecko).",
                                        ".$db->quote($buildconfig).",
                                        ".$db->quote($product).",
                                        ".$db->quote($email).",
                                        ".$db->quote($_SERVER['REMOTE_ADDR']).",
                                        now(),
                                        ".$db->quote($sysid)."
                                    );");
    if (!$addReportQuery) {
        return new soap_fault('SERVER', '', 'Database Error SR4');
    }

    /**********
     * Process Screenshot
     **********/
    if($screenshot != null){

        // Screenshots come in base64 encoded, so we need to decode.
        $screenshot = base64_decode($screenshot);

        // Note we addslashes() not quote() the image, because quote() is not
        // binary compatible and has ugly consequences.
        $insertSsQuery = $db->Execute("INSERT screenshot(
                                           screenshot.screenshot_report_id,
                                           screenshot.screenshot_data,
                                           screenshot.screenshot_format
                                       )
                                       VALUES (".$db->quote($report_id).",
                                               '".addslashes($screenshot)."',
                                               ".$db->quote($screenshot_format)."
                                       );
        ");
        if(!$insertSsQuery){
            return new soap_fault('SERVER', '', 'Database Error SR5');
        }
        // If we got this far, the screenshot was successfully added!
    }

    /**********
     * Disconnect (optional really)
     **********/
    $db->disconnect();
    return $report_id;
}

function register($language){
    global $config;

    /**********
     * Open DB
     **********/
    $db = NewDBConnection($config['db_dsn']);
    $db->SetFetchMode(ADODB_FETCH_ASSOC);
    $db->debug = false;  // no good reason to ever let this be true, since it breaks things

    /**********
     * Generate an ID
     **********/
    $unique = false;

    // in theory a collision could happen, though unlikely.    So just to make sure, we do this
    // since that would really suck
    while (!$unique) {
        $id = date("ymd").rand(1000,9999);

        $uniqueQuery =& $db->Execute("SELECT sysid.sysid_id
                                FROM sysid
                                WHERE sysid.sysid_id = '$id'
                               ");
        if(!$uniqueQuery){
            return new soap_fault('SERVER', '', 'Database Error R1');
        }
        $numRows = $uniqueQuery->RecordCount();
        if ($numRows == 0) {
            // It's unique, stop the loop.
            $unique = true;
        }
    }

    /**********
     * Register ID
     **********/
    $addSysIdQuery = $db->Execute("INSERT INTO sysid (
                                       sysid.sysid_id,
                                       sysid.sysid_created,
                                       sysid.sysid_created_ip,
                                       sysid.sysid_language
                                   )
                                   VALUES (
                                       '".$id."',
                                       now(),
                                       ".$db->quote($_SERVER['REMOTE_ADDR']).",
                                       ".$db->quote($language)."
                                   )");
    if (!$addSysIdQuery) {
        return new soap_fault('SERVER', '', 'Database Error R2');
    }

    /**********
     * Disconnect
     **********/
    $db->disconnect();

    return $id;
}

// Use the request to (try to) invoke the service
$HTTP_RAW_POST_DATA = isset($HTTP_RAW_POST_DATA) ? $HTTP_RAW_POST_DATA : '';
$server->service($HTTP_RAW_POST_DATA);

?>