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

if(!isset($_GET['method'])){
    $method = 'html';
}

$title = "Searching Results";

$content = initializeTemplate();
$content->assign('method', $method);

// Open DB
$db = NewDBConnection($config['db_dsn']);
$db->SetFetchMode(ADODB_FETCH_ASSOC);

$query = new query;
$query->doQuery();

// disconnect database
$db->Close();

if ($query->totalResults == 0){
    $content->assign('error', 'No Results found');
    displayPage($content, 'query', 'query.tpl');
    exit;
}

// Start Next/Prev Navigation
/*******
 * We cap the navigation at 2000 items because php sometimes acts wierd
 * when sessions get to big.  In most cases, this won't effect anyone.  Note
 * that reportList won't ever return more than max_nav_count by design.
 *******/
if($query->totalResults < $config['max_nav_count']){
    $_SESSION['reportList'] = $query->reportList;
} else {
    unset($_SESSION['reportList']);
    $content->assign('notice', 'This query returned too many reports for next/previous navigation to work');
}

$content->assign('column',             $query->columnHeaders());
$content->assign('row',                $query->outputHTML());

/* this particular continuity_params is for pagination (it doesn't include 'page') */
$content->assign('continuity_params',  $query->continuityParams(array('page')));

/* Pagination */
$pages = ceil($query->totalResults/$query->show);

/* These variables are also used for pagination purposes */
$content->assign('count',              $query->totalResults);
$content->assign('show',               $query->show);
$content->assign('page',               $query->page);
$content->assign('pages',              $pages);

if($query->page > 10){
    $start = $query->page-10;
}
if($query->page < 10){
    $start = 1;
}

$content->assign('start',              $start);
$content->assign('step',               1);
if(ceil($query->totalResults/$query->show) < 20){
    $content->assign('amt',            ceil($query->totalResults/$query->show));
} else {
    $content->assign('amt',            20);
}

displayPage($content, 'query', 'query.tpl');
?>