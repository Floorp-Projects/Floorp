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
require_once('DB.php');
require_once($config['app_path'].'/includes/iolib.inc.php');
require_once($config['nusoap_path'].'/nusoap.php');

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
    if (!$problem_type || $problem_type == -1 || $problem_type == "0_0") {
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

    // Initialize Database
    $db =& DB::connect($config['db_dsn']);

    $sysIDQuery = $db->query("SELECT `sysid_id` FROM `sysid` WHERE `sysid_id` = '".$db->escapeSimple($sysid)."'");
    $sysidCount = $sysIDQuery->numRows();
	if ($sysidCount != 1){
        return new soap_fault('Client', '', 'Invalid SysID', $sysid);
	}

    $queryURL = $db->query("SELECT `host_id` FROM `host` WHERE `host_hostname` = '".$db->escapeSimple($parsedURL['host'])."'");
    $resultURL = $queryURL->numRows();
    if ($resultURL <= 0) {
        // generate hash
        $host_id = md5($parsedURL['host'].microtime());
        // We add the URL
        $addURL = $db->query("INSERT INTO `host` (`host_id`, `host_hostname`, `host_date_added`)
                                VALUES (
                                    '".$db->escapeSimple($host_id)."',
                                    '".$db->escapeSimple($parsedURL['host'])."',
                                    now()
                                )
                    ");
        if (!$addURL) {
            return new soap_fault('SERVER', '', 'Database Error');
        }
    }
    else if ($resultURL == 1) {
        // pull the hash from DB
		$queryURLResult = $queryURL->fetchRow(DB_FETCHMODE_ASSOC);
        $host_id = $queryURLResult['host_id'];
    } else{
            return new soap_fault('SERVER', '', 'Host Exception Error');
    }

    $addReport = $db->query("INSERT INTO `report` (
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
                                        '".$db->escapeSimple($report_id)."',
                                        '".$db->escapeSimple($url)."', 
                                        '".$db->escapeSimple($host_id)."', 
                                        '".$db->escapeSimple($problem_type)."', 
                                        '".$db->escapeSimple($description)."', 
                                        '".$db->escapeSimple($behind_login)."', 
                                        '".$db->escapeSimple($useragent)."',
                                        '".$db->escapeSimple($platform)."', 
                                        '".$db->escapeSimple($oscpu)."', 
                                        '".$db->escapeSimple($language)."', 
                                        '".$db->escapeSimple($gecko)."', 
                                        '".$db->escapeSimple($buildconfig)."', 
                                        '".$db->escapeSimple($product)."', 
                                        '".$db->escapeSimple($email)."', 
                                        '".$db->escapeSimple($_SERVER['REMOTE_ADDR'])."', 
                                        now(), 
                                        '".$db->escapeSimple($sysid)."'
                                )
        ");

    // Disconnect Database
    $db->disconnect();

    if (!$addReport) {
        return new soap_fault('SERVER', '', 'Database Error');
    } else {
        return $report_id;
    }
}

function register($language){
    global $config;

    // Initialize Database
    //PEAR::setErrorHandling(PEAR_ERROR_CALLBACK, 'handleErrorsSOAP');
    $db =& DB::connect($config['db_dsn']);

    // generate an ID
    $unique = false;

    // in theory a collision could happen, though unlikely.    So just to make sure, we do this
    // since that would really suck
    while (!$unique) {
        $id = date("ymd").rand(1000,9999);

        $query =& $db->query("SELECT sysid.sysid_id
		                      FROM sysid
                              WHERE sysid.sysid_id = '$newid'
                            ");
        $numRows = $query->numRows();
        if ($numRows == 0) {
            // It's unique, stop the loop.
            $unique = true;
        }
    }

    $addsysid = $db->query("INSERT INTO `sysid` (
                                 `sysid_id`, 
                                 `sysid_created`, 
                                 `sysid_created_ip`, 
                                 `sysid_language` 
                            )
                            VALUES (
                                  '".$id."', 
                                  now(),
                                  '".$_SERVER['REMOTE_ADDR']."',
                                  '".$db->escapeSimple($language)."'
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
