<?php
/**
 * Dictionaries page.
 *
 * @package amo
 * @subpackage docs
 */

startProcessing('dictionaries.tpl', 'dictionaries', $compileId, 'rustico');
require_once('includes.php');

setApp();

$amo = new AMO_Object();

$dicts = $amo->getDictionaries();

// Assign template variables.
$tpl->assign(
    array(  'dicts'             => $dicts,
            'content'           => 'dictionaries.tpl',
            'currentTab'        => 'dictionaries')
);
?>
