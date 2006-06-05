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
 *      Robert Accettura <robert@accettura.com>.
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

/**************************************
 * I need to be renamed config.inc.php
 * once the below is correct
 **************************************/

// Debug
$config['debug']                           = false;

// Service Active
$config['service_active']                  = true; // true=on | false=off

// Screenshot Formats
$config['screenshot_imageTypes']           = array('png' => 'image/png',
                                                   'jpg' => 'image/jpeg');

// Turn on WSDL??
$config['use_wsdl']                        = false;

// Paths
$config['base_url']                        = 'http://reporter.server.tld'; // no trailing slash
$config['base_path']                       = '/path/to/reporter'; // no trailing slash

// Database
$config['db_type']                         = 'mysql';
$config['db_server']                       = 'SERVER';
$config['db_user']                         = 'USERNAME';
$config['db_pass']                         = 'PASSWORD';
$config['db_database']                     = 'DATABASE';
$config['db_dsn']                          = $config['db_type'].'://'.$config['db_user'].':'.$config['db_pass'].'@'.$config['db_server'].'/'.$config['db_database'];

// Theme
$config['theme']                           = 'mozilla';

// Smarty Configurations
$config['smarty_template_directory']       = $config['base_path'].'/templates/';
$config['smarty_compile_dir']              = '/tmp';
$config['smarty_cache_dir']                = '';
$config['smarty_compile_check']            = true;
$config['smarty_debug']                    = false;

// Privacy Policy
$config['privacy_policy_web']              = 'http://www.mozilla.com/privacy-policy.html';
$config['privacy_policy_inline']           = 'http://reporter.mozilla.org/privacy/privacy-policy.html';

// Service
/**********************************************************
 * If using reporter in a corporate environment, you can
 * set reporter as a proxy, so that all reports that match
 * a regular exression are forwarded to your database, and
 * the rest go outwards to the main reporter.mozilla.org
 * instance
***********************************************************/
$config['proxy']                           = false;
/**********************************************************
 * if this statement evaluates to true, it uses the local
 * instance, otherwise it goes to reporter.mozilla.org.
 * You can have as many as you want (though less is better),
 * sequentially number the array $config['proxy_regex'][0],
 * $config['proxy_regex'][1], etc.
************************************************************/
$config['proxy_regex'][0]                  = '';

// should we gzip encode (more cpu, less bandwidth) XX-> Not implemented
$config['gzip']                             = false;

// minimum version to allow to contact the service
$config['min_vers']                        = '0.2';

// current product family
$config['current_product_family']          = 'Firefox/1.5';

// How many items to show by default
$config['show'] = 25;
$config['max_show'] = 200;

// Max items to remember for next/prev navigation
$config['max_nav_count'] = 2000;

// Field Names, and how they should appear in the UI
$config['fields']['report_id']             = 'Report ID';
$config['fields']['report_url']            = 'URL';
$config['fields']['host_url']              = 'Host';
$config['fields']['host_hostname']         = 'Hostname';
$config['fields']['report_problem_type']   = 'Problem Type';
$config['fields']['report_behind_login']   = 'Behind Login';
$config['fields']['report_product']        = 'Product';
$config['fields']['report_gecko']          = 'Gecko';
$config['fields']['report_useragent']      = 'User Agent';
$config['fields']['report_buildconfig']    = 'Build Config';
$config['fields']['report_platform']       = 'Platform';
$config['fields']['report_oscpu']          = 'OSCPU';
$config['fields']['report_language']       = 'Language';
$config['fields']['report_file_date']      = 'File Date';
$config['fields']['report_email']          = 'Email';
$config['fields']['report_ip']             = 'IP';
$config['fields']['report_description']    = 'Description';
$config['fields']['report_file_date']      = 'Report File Date';

$config['unselectablefields']              = array('host_url', 'report_email', 'report_ip');


/*****************************/
// Shouldn't need to touch this
/*****************************/
$config['self']                         = 'http://' . $_SERVER['SERVER_NAME'] . substr($_SERVER['PHP_SELF'], 0, strrpos($_SERVER['PHP_SELF'], '/')) . '/';

$boolTypes[-1] = '--';
$boolTypes[0] = 'No';
$boolTypes[1] = 'Yes';

//$problemTypes[0] = '--';
$problemTypes[1] = 'Browser not supported';
$problemTypes[2] = 'Can\'t log in';
$problemTypes[3] = 'Plugin not shown';
$problemTypes[4] = 'Other content missing';
$problemTypes[5] = 'Behavior wrong';
$problemTypes[6] = 'Appearance wrong';
$problemTypes[7] = 'Other problem';

?>
