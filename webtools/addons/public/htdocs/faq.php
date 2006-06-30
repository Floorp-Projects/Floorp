<?php
/**
 * FAQ page.
 *
 * @package amo
 * @subpackage docs
 *
 * @todo FAQ search?
 */

startProcessing('faq.tpl','faq',$compileId);
require_once('includes.php');

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

$sql = "
    SELECT 
        `AppName`,
        `GUID`,
        `Version`
    FROM
        `applications`
    WHERE
        `public_ver`='YES'
    ORDER BY
        Appname, Version
";

$db->query($sql,SQL_ALL,SQL_ASSOC);

if (is_array($db->record)) {
    foreach ($db->record as $row) {
        $appVersions[] = array(
            'displayVersion'    => $row['Version'],
            'appName'           => $row['AppName'],
            'guid'              => $row['GUID']
        );
    }
}

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
   array(   'faq'       => $faq,
            'links'     => $links,
            'sidebar'   => 'inc/nav.tpl',
            'content'   => 'faq.tpl',
            'title'     => 'Frequently Asked Questions',
            'appVersions' => $appVersions   )
);
?>
