<?php
/**
 * FAQ page.
 * @package amo
 * @subpackage docs
 */
$db->query("
    SELECT
        `title`,
        `text`
    FROM
        `faq`
    WHERE
        `active` = 'YES'
    ORDER BY
        `index` ASC,
        `title` ASC
",SQL_ALL, SQL_ASSOC);

$faq = $db->record;

$links = array(
    array(  'href'  => './faq.php',
            'title' => 'Frequently Asked Questions', 
            'text'  => 'FAQ'),

    array(  'href'  => './policy.php',
            'title' => 'Addons Policies', 
            'text'  => 'Policy'),

    array(  'href'  => './about.php',
            'title' => 'About this Site', 
            'text'  => 'About'),
);

// Send FAQ data to Smarty object.
$tpl->assign(
   array(   'faq'       => $faq,
            'links'     => $links,
            'sidebar'   => 'inc/nav.tpl',
            'content'   => 'faq.tpl',
            'title'     => 'Frequently Asked Questions')
);

// No need to set wrapper, since the left-nav wrapper is default.
// $wrapper = 'inc/wrappers/default.tpl';
?>
