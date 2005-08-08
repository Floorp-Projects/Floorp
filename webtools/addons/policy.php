<?php
/**
 * Policy page.
 *
 * @package amo
 * @subpackage docs
 *
 * @todo talk to cbeard and rebron about establishing the policy document.
 */

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
