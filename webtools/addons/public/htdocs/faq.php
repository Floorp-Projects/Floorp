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

/**
 * Form a properly formatted string for comparison based on applications table
 * data.
 * @param string $major
 * @param string $minor
 * @param string $release
 * @param string $subver
 * @return string $version
 */
function buildAppVersion ($major,$minor,$release,$subver) {

    // This is our final version string.
    $version = '';

    // By default, we cat major and minor versions, because we assume they
    // exist and are always numeric.  And they typically are, for now... :\
    $version = $major.'.'.$minor;

    // If we have a release and it's not a final version, add the release.
    if (isset($release) && (empty($subver) || isset($subver) && $subver != 'final' && $subver != '+')) {

        // Only add the '.' if it's numeric.
        if (preg_match('/^\*|\w+$/',$release)) {
            $version = $version.'.'.$release;
        } else {
            $version = $version.$release;
        }
    }

    // If we have a subversion and it's not 'final', append it depending on its type.
    if (!empty($subver) && $subver != 'final') {
        if (preg_match('/^\*|\w+$/',$subver)) {
            $version  = $version.'.'.$subver;
        } else {
            $version  = $version.$subver;
        }
    }

    return $version;
}

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
        `Version`,
        `major`,
        `minor`,
        `release`,
        `SubVer`
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
            'versionNumber'     => buildAppVersion($row['major'],$row['minor'],$row['release'],$row['SubVer']),
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
