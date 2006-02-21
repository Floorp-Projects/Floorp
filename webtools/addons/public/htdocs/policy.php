<?php
/**
 * Policy page.
 *
 * @package amo
 * @subpackage docs
 *
 * @todo talk to cbeard and rebron about establishing the policy document.
 */

startProcessing('policy.tpl','policy',$compileId);
require_once('includes.php');

$links = array(
    array(  'href'  => './faq.php',
            'title' => 'Frequently Asked Questions', 
            'text'  => 'FAQ'),

    array(  'href'  => './policy.php',
            'title' => 'Addons Policies', 
            'text'  => 'Policy')
);

// Send FAQ data to Smarty object.
$tpl->assign(
   array(   'links'     => $links,
            'sidebar'   => 'inc/nav.tpl',
            'content'   => 'policy.tpl',
            'title'     => 'Policy')
);
?>
