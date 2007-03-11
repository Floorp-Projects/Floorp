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

require_once('../config.inc.php');
require_once($config['base_path'].'/includes/iolib.inc.php');
require_once($config['base_path'].'/includes/db.inc.php');
require_once($config['base_path'].'/includes/contrib/smarty/libs/Smarty.class.php');
require_once($config['base_path'].'/includes/security.inc.php');
require_once($config['base_path'].'/includes/query.inc.php');

printheaders();

// Open DB
$db = NewDBConnection($config['db_dsn']);
$db->SetFetchMode(ADODB_FETCH_ASSOC);

$reportQuery =& $db->Execute("SELECT *
                              FROM report, host
                              WHERE report.report_id = ".$db->quote($_GET['report_id'])."
                              AND host.host_id = report_host_id");
if(!$reportQuery){
    trigger_error("DB Error");
}

// Init to false
$screenshot = false;

// Only check for a screenshot if the user is an admin, and if we have a valid report
if($reportQuery->RecordCount() == 1 && $securitylib->isLoggedIn()){
    $screenshotQuery =& $db->Execute("SELECT screenshot.screenshot_report_id
                                      FROM screenshot
                                      WHERE screenshot.screenshot_report_id = ".$db->quote($_GET['report_id'])
                                     );
    if(!$screenshotQuery){
        trigger_error("DB Error");
    }
    if($screenshotQuery->RecordCount() == 1){
        $screenshot = true;
    }
}

// disconnect database
$db->Close();

$content = initializeTemplate();

if (!$reportQuery->fields){
    $content->assign('error', 'No Report Found');
    displayPage($content, 'report', 'report.tpl', 'Mozilla Reporter - Error');
    exit;
}

// We need this for continuity params in particular
$query = new query;
$query_input = $query->processQueryInputs();


$title = "Report for ".$reportQuery->fields['host_hostname']." - ".$reportQuery->fields['report_id'];
$content->assign('report_id',              $reportQuery->fields['report_id']);
$content->assign('report_url',             $reportQuery->fields['report_url']);

//$host_continuity_params = $query->continuityParams(array('report_id', 'report_product', 'report_file_date', 'product_family', 'page'));

$content->assign('host_continuity_params', $host_continuity_params);

$content->assign('host_url',               $config['base_url'].'/app/query/?host_hostname='.$reportQuery->fields['host_hostname'].'&amp;'.$host_continuity_params.'submit_query=Query');
$content->assign('host_hostname',          $reportQuery->fields['host_hostname']);
$content->assign('report_problem_type',    resolveProblemTypes($reportQuery->fields['report_problem_type']));
$content->assign('report_behind_login',    resolveBehindLogin($reportQuery->fields['report_behind_login']));
$content->assign('report_product',         $reportQuery->fields['report_product']);
$content->assign('report_gecko',           $reportQuery->fields['report_gecko']);
$content->assign('report_useragent',       $reportQuery->fields['report_useragent']);
$content->assign('report_buildconfig',     $reportQuery->fields['report_buildconfig']);
$content->assign('report_platform',        $reportQuery->fields['report_platform']);
$content->assign('report_oscpu',           $reportQuery->fields['report_oscpu']);
$content->assign('report_language',        $reportQuery->fields['report_language']);
$content->assign('report_charset',         $reportQuery->fields['report_charset']);
$content->assign('report_file_date',       $reportQuery->fields['report_file_date']);
$content->assign('report_email',           $reportQuery->fields['report_email']);
$content->assign('report_ip',              $reportQuery->fields['report_ip']);
$content->assign('report_description',     $reportQuery->fields['report_description']);

if($screenshot){
    $content->assign('screenshot',         $screenshot);
}

// Navigation Functionality
$nav_continuity_params = $query->continuityParams(array('report_id'));
$content->assign('nav_continuity_params',             $nav_continuity_params);

if(isset($_SESSION['reportList'])){
    $reportIndex = array_search($_GET['report_id'],   $_SESSION['reportList']);

    $content->assign('index',                         $reportIndex);
    $content->assign('total',                         sizeOf($_SESSION['reportList']));

    $content->assign('showReportNavigation',          true);

    if($reportIndex > 0){
        $content->assign('first_report',              $_SESSION['reportList'][0]);
        $content->assign('previous_report',           $_SESSION['reportList'][$reportIndex-1]);
    } else {
        $content->assign('first_report',              'disable');
        $content->assign('previous_report',           'disable');
    }
    if($reportIndex < sizeof($_SESSION['reportList'])-1){
        $content->assign('next_report',               $_SESSION['reportList'][$reportIndex+1]);
        $content->assign('last_report',               $_SESSION['reportList'][sizeof($_SESSION['reportList'])-1]);
    } else {
        $content->assign('next_report',               'disable');
        $content->assign('last_report',               'disable');
    }
} else {
    $content->assign('showReportNavigation',          false);
}

displayPage($content, 'report', 'report.tpl', $title);
?>