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
require_once($config['base_path'].'/includes/contrib/adodb/adodb.inc.php');
require_once($config['base_path'].'/includes/contrib/nusoap/lib/nusoap.php');

// Turn off Error Reporting
error_reporting(0);

// Create the server instance
$server = new soap_server;

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

$server->register(
    'submitReport',                     // method name
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
          'sysid' => 'xsd:string'),     // input parameters
    array('return' => 'xsd:string'),    // output parameters
    'uri:MozillaReporter',              // namespace
    'uri:MozillaReporter/submitReport', // SOAPAction
    'rpc',                              // style
    'encoded'                           // use
);
function submitReport($rmoVers, $url, $problem_type, $description, $behind_login, $platform, $oscpu, $gecko, $product, $useragent, $buildconfig, $language, $email, $sysid) {
    global $config;

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

    // check verison
    if ($rmoVers < $config['min_vers']){
        return new soap_fault('Client', '', 'Your product is out of date, please upgrade.  See http://reporter-test.mozilla.org/install for details.', $rmoVers);
    }

    $parsedURL = parse_url($url);
    if (!$url || !$parsedURL['host']){
        return new soap_fault('Client', '', 'url must use a valid URL syntax http://domain.tld/foo', $url);
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
/*  not used until we have a way to gather this info
   if (!$gecko) {
        return new soap_fault('Client', '', 'Invalid Gecko ID', $gecko);
    }*/
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
    /*    we don't require email... it's optional
    if    (!$email) {
        return new soap_fault('Client', '', 'Invalid Email', $email);
    }*/

    // create report_id.    We just MD5 it, becase we don't need people counting reports, since it's inaccurate.
    // we can have dup's, so it's not a good thing for people to be saying 'mozilla.org reports 500,000 incompatable sites'
    $report_id = 'RMO'.str_replace(".", "", array_sum(explode(' ', microtime())));

    // Open DB
    $db = NewADOConnection($config['db_dsn']);
    if (!$db) die("Connection failed");
    $db->SetFetchMode(ADODB_FETCH_ASSOC);

    $sysIDQuery = $db->Execute("SELECT `sysid_id` FROM `sysid` WHERE `sysid_id` = ".$db->quote($sysid));
    $sysidCount = $sysIDQuery->RecordCount();
    if ($sysidCount != 1){
      return new soap_fault('Client', '', 'Invalid SysID', $sysid);
    }

    $queryURL = $db->Execute("SELECT `host_id` FROM `host` WHERE `host_hostname` = ".$db->quote($parsedURL['host']));
    $resultURL = $queryURL->RecordCount();
    if ($resultURL <= 0) {
        // generate hash
        $host_id = md5($parsedURL['host'].microtime());
        // We add the URL
        $addURL = $db->Execute("INSERT INTO `host` (`host_id`, `host_hostname`, `host_date_added`)
                                VALUES (
                                    ".$db->quote($host_id).",
                                    ".$db->quote($parsedURL['host']).",
                                    now()
                                )
                    ");
        if (!$addURL) {
            return new soap_fault('SERVER', '', 'Database Error');
        }
    }
    else if ($resultURL == 1) {
        // pull the hash from DB
        $host_id = $queryURL->fields['host_id'];
    } else{
            return new soap_fault('SERVER', '', 'Host Exception Error');
    }
    $addReport = $db->Execute("INSERT INTO `report` (
                                                    `report_id`,
                                                    `report_url`,
                                                    `report_host_id`,
                                                    `report_problem_type`,
                                                    `report_description`,
                                                    `report_behind_login`,
                                                    `report_useragent`,
                                                    `report_platform`,
                                                    `report_oscpu`,
                                                    `report_language`,
                                                    `report_gecko`,
                                                    `report_buildconfig`,
                                                    `report_product`,
                                                    `report_email`,
                                                    `report_ip`,
                                                    `report_file_date`,
                                                    `report_sysid`
                                )
                                VALUES (
                                        ".$db->quote($report_id).",
                                        ".$db->quote($url).",
                                        ".$db->quote($host_id).",
                                        ".$db->quote($problem_type).",
                                        ".$db->quote($description).",
                                        ".$db->quote($behind_login).",
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
                                )
        ");

    if (!$addReport) {
        return new soap_fault('SERVER', '', 'Database Error');
    } else {
        return $report_id;
    }
}

function register($language){
    global $config;

    // Open DB
    $db = NewADOConnection($config['db_dsn']);
    if (!$db) die("Connection failed");
    $db->SetFetchMode(ADODB_FETCH_ASSOC);

    // generate an ID
    $unique = false;

    // in theory a collision could happen, though unlikely.    So just to make sure, we do this
    // since that would really suck
    while (!$unique) {
        $id = date("ymd").rand(1000,9999);

        $query =& $db->Execute("SELECT sysid.sysid_id
                                FROM sysid
                                WHERE sysid.sysid_id = '$newid'
                               ");
        $numRows = $query->RecordCount();
        if ($numRows == 0) {
            // It's unique, stop the loop.
            $unique = true;
        }
    }

    $addsysid = $db->Execute("INSERT INTO `sysid` (
                                 `sysid_id`,
                                 `sysid_created`,
                                 `sysid_created_ip`,
                                 `sysid_language`
                            )
                            VALUES (
                                  '".$id."',
                                  now(),
                                  '".$_SERVER['REMOTE_ADDR']."',
                                  ".$db->quote($language)."
                            )
                     ");
    // Disconnect Database
    $db->disconnect();

    if (!$addsysid) {
        return new soap_fault('SERVER', '', 'Database Error');
    } else {
        return $id;
    }
}

// Use the request to (try to) invoke the service
$HTTP_RAW_POST_DATA = isset($HTTP_RAW_POST_DATA) ? $HTTP_RAW_POST_DATA : '';
$server->service($HTTP_RAW_POST_DATA);
?>
