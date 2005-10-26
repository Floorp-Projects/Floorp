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

require_once('../../config.inc.php');
require_once($config['base_path'].'/includes/iolib.inc.php');
require_once($config['base_path'].'/includes/db.inc.php');
require_once($config['base_path'].'/includes/contrib/smarty/libs/Smarty.class.php');
require_once($config['base_path'].'/includes/security.inc.php');

// Start Session
session_name('reportSessID');
session_start();
header("Cache-control: private"); //IE 6 Fix
printheaders();

// Open DB
$db = NewADOConnection($config['db_dsn']);
$db->SetFetchMode(ADODB_FETCH_ASSOC);

$query =& $db->Execute("SELECT *
                        FROM report, host
                        WHERE report.report_id = ".$db->quote($_GET['report_id'])."
                        AND host.host_id = report_host_id");

// disconnect database
$db->Close();

$title = "Report for - ".$query->fields['host_hostname'];

$content = initializeTemplate();

if (!$query->fields){
    $content->assign('error', 'No Report Found');
    displayPage($content, 'report.tpl', 'Mozilla Reporter - Error');
    exit;
}

$content->assign('report_id',              $query->fields['report_id']);
$content->assign('report_url',             $query->fields['report_url']);
$content->assign('host_url',               $config['base_url'].'/app/query/?host_hostname='.$query->fields['host_hostname'].'&amp;submit_query=Query');
$content->assign('host_hostname',          $query->fields['host_hostname']);
$content->assign('report_problem_type',    resolveProblemTypes($query->fields['report_problem_type']));
$content->assign('report_behind_login',    resolveBehindLogin($query->fields['report_behind_login']));
$content->assign('report_product',         $query->fields['report_product']);
$content->assign('report_gecko',           $query->fields['report_gecko']);
$content->assign('report_useragent',       $query->fields['report_useragent']);
$content->assign('report_buildconfig',     $query->fields['report_buildconfig']);
$content->assign('report_platform',        $query->fields['report_platform']);
$content->assign('report_oscpu',           $query->fields['report_oscpu']);
$content->assign('report_language',        $query->fields['report_language']);
$content->assign('report_file_date',       $query->fields['report_file_date']);
$content->assign('report_email',           $query->fields['report_email']);
$content->assign('report_ip',              $query->fields['report_ip']);
$content->assign('report_description',     $query->fields['report_description']);

$title = 'Mozilla Reporter: '.$query->fields['report_id'];

displayPage($content, 'report.tpl', $title);
?>

