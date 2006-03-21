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

// These are fields we will populate based on their values
$fields = array('report_description',
                'report_useragent',
                'report_gecko',
                'report_language',
                'report_platform',
                'report_oscpu',
                'report_product',
                'report_file_date_start',
                'report_file_date_end',
                'show',
                'count',
                'host_hostname',
                'report_problem_type',
                'report_behind_login',
);

foreach($fields as $field){
    if(isset($_GET[$field])){
        $content->assign($field,          htmlspecialchars($_GET[$field]));
    }
}

// Problem Types
if(isset($problemTypes)){
    $content->assign('problem_types',               $problemTypes);
}

// Get the list of products from the database
$db = NewDBConnection($config['db_dsn']);
$db->SetFetchMode(ADODB_FETCH_ASSOC);

$productDescQuery =& $db->Execute("SELECT product.product_value, product.product_description
                                   FROM product
                                   ORDER BY product.product_description desc");
if($productDescQuery){
    $products = array();
    while (!$productDescQuery->EOF) {
        $products[$productDescQuery->fields['product_value']] = $productDescQuery->fields['product_description'];
        $productDescQuery->MoveNext();
    }
}
if(isset($products) && sizeof($products) > 0){
    $content->assign('product_options',             $products);
}
$db->Close();

if(isset($products)){
    $content->assign('products',                    $products);
}

// View
// Remove fields that you can't manually select
foreach ($config['unselectablefields'] as $unselectableChild){
    unset($config['fields'][$unselectableChild]);
}

$content->assign('selected_options',            $config['fields']);

// Remove fields that you can't manually select
foreach ($config['unselectablefields'] as $unselectableChild){
    unset($config['fields'][$unselectableChild]);
}

$content->assign('selected_options',            $config['fields']);

// Selected is a special case
if (isset($_GET['selected'])){
    $content->assign('selected',               	htmlspecialchars($_GET['selected']));
}
else {
    $content->assign('selected',                array('host_hostname', 'report_file_date'));
}

// Output page
displayPage($content, 'index', 'index.tpl');
?>
