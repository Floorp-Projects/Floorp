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
require_once($config['base_path'].'/includes/security.inc.php');
require_once($config['base_path'].'/includes/iolib.inc.php');
require_once($config['base_path'].'/includes/db.inc.php');
require_once($config['base_path'].'/includes/contrib/smarty/libs/Smarty.class.php');

printheaders();

$content = initializeTemplate();
if(isset($_GET['report_description'])){
    $content->assign('report_description',          $_GET['report_description']);
}
if(isset($_GET['report_useragent'])){
    $content->assign('report_useragent',            $_GET['report_useragent']);
}
if(isset($_GET['report_gecko'])){
    $content->assign('report_gecko',                $_GET['report_gecko']);
}
if(isset($_GET['report_language'])){
    $content->assign('report_language',             $_GET['report_language']);
}
if(isset($_GET['report_platform'])){
    $content->assign('report_platform',             $_GET['report_platform']);
}
if(isset($_GET['report_oscpu'])){
    $content->assign('report_oscpu',                $_GET['report_oscpu']);
}

$db = NewDBConnection($config['db_dsn']);
$db->SetFetchMode(ADODB_FETCH_ASSOC);

$productDescQuery =& $db->Execute("SELECT product.product_value, product.product_description
                                   FROM product
                                   ORDER BY product.product_description desc");
if(!$productDescQuery){
    die("DB Error");
}

$products = array();
while (!$productDescQuery->EOF) {
    $products[$productDescQuery->fields['product_value']] = $productDescQuery->fields['product_description'];
    $productDescQuery->MoveNext();
}


if(isset($config['products'])){
    $content->assign('product_options',             $products);
}
if(isset($_GET['report_product'])){
    $content->assign('report_product',              $_GET['report_product']);
}
if(isset($_GET['report_file_date_start'])){
    $content->assign('report_file_date_start',      $_GET['report_file_date_start']);
}
if(isset($_GET['report_file_date_end'])){
    $content->assign('report_file_date_end',        $_GET['report_file_date_end']);
}
if(isset($_GET['show'])){
    $content->assign('show',                        $_GET['show']);
}
if(isset($_GET['count'])){
    $content->assign('count',                       $_GET['count']);
}
if(isset($_GET['host_hostname'])){
    $content->assign('host_hostname',               $_GET['host_hostname']);
}
if(isset($_GET['report_problem_type'])){
    $content->assign('report_problem_type',         $_GET['report_problem_type']);
}
if(isset($_GET['report_behind_login'])){
    $content->assign('report_behind_login',         $_GET['report_behind_login']);
}
if(isset($products)){
    $content->assign('products',                    $products);
}
if(isset($problemTypes)){
    $content->assign('problem_types',               $problemTypes);
}

// Remove fields that you can't manually select
foreach ($config['unselectablefields'] as $unselectableChild){
    unset($config['fields'][$unselectableChild]);
}

$content->assign('selected_options',            $config['fields']);
if (isset($_GET['selected'])){
    $content->assign('selected',               	$_GET['selected']);
}
else {
    $content->assign('selected',                array('report_id', 'host_hostname', 'report_file_date'));
}

displayPage($content, 'index', 'index.tpl');
?>
