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

printheaders();

// Open DB
$db = NewDBConnection($config['db_dsn']);
$db->SetFetchMode(ADODB_FETCH_ASSOC);

$content = initializeTemplate();

// Total Reports
$reports_q =& $db->Execute("SELECT COUNT(report_id) as total
                            FROM report");
$reports = $reports_q->fields['total'];
$content->assign('reports_quant', $reports);

// Total Unique Users
$uniqueusers_q =& $db->Execute("SELECT COUNT(sysid_id) as total
                                FROM sysid");
$uniqueusers = $uniqueusers_q->fields['total'];
$content->assign('users_quant', $uniqueusers);

// Average # of reports per user
$avgRepPerUsr = $reports/$uniqueusers;
$content->assign('avgRepPerUsr', $avgRepPerUsr);

// Total Reports per product
$product_q =& $db->Execute("SELECT DISTINCT (report.report_product),
                             COUNT( report.report_product ) AS total
                             FROM report
                             GROUP BY report.report_product
                             ORDER BY total DESC");

while(!$product_q->EOF){
    $product[] = $product_q->fields;
    $product_q->MoveNext();
}
$content->assign('product', $product);

// Total Reports per platform
$platform_q =& $db->Execute("SELECT DISTINCT (report.report_platform),
                             COUNT( report.report_platform ) AS total
                             FROM report
                             GROUP BY report.report_platform
                             ORDER BY total DESC");

while(!$platform_q->EOF){
    $platform[] = $platform_q->fields;
    $platform_q->MoveNext();
}
$content->assign('platform', $platform);


// Total Hosts
$uniquehosts_q =& $db->Execute("SELECT COUNT(host_hostname) as total
                                FROM host");
$uniquehosts = $uniquehosts_q->fields['total'];
$content->assign('hosts_quant', $uniquehosts);

// Reports in last 24 hours
$yesterday  = mktime(date("H"), date("i"), date("s"), date("m")  , date("d")-1, date("Y"));
$reports24_q =& $db->Execute("SELECT COUNT(report_id) as total
                              FROM report
                              WHERE  report_file_date > "."'".date('Y-m-d H:i:s', $yesterday)."'");
$reports24 = $reports24_q->fields['total'];
$content->assign('reports24', $reports24);

// Reports in last week
$last7days  = mktime(date("H"), date("i"), date("s"), date("m")  , date("d")-7, date("Y"));
$last7days_q =& $db->Execute("SELECT COUNT(report_id) as total
                              FROM report
                              WHERE  report_file_date > "."'".date('Y-m-d H:i:s', $last7days)."'");
$last7days = $last7days_q->fields['total'];
$content->assign('last7days', $last7days);

// disconnect database
$db->Close();

$title = "Statistics";

displayPage($content, 'stats', 'stats.tpl');
?>