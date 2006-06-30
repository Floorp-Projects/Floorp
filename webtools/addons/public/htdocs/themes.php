<?php
/**
 * Home page for themes, switchable on application.  Since v1 used GUIDs, the
 * flow on this page is a little confusing (we need to support both name and GUID.
 *
 * @package amo
 * @subpackage docs
 */

$currentTab = 'themes';

startProcessing('themes.tpl', 'themes', $compileId);
require_once('includes.php');

$_app = array_key_exists('app', $_GET) ? $_GET['app'] : null;

// Determine our application.
switch( $_app ) {
    case 'mozilla':
        $clean['app'] = 'Mozilla';
        break;
    case 'thunderbird':
        $clean['app'] = 'Thunderbird';
        break;
    case 'firefox':
    default:
        $clean['app'] = 'Firefox';
        break;
}

$amo = new AMO_Object();

// Despite what $clean holds, GUIDs were used in v1 so we have to support them
if (preg_match('/^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i',$_app)) {
    $newestThemes  = $amo->getNewestAddonsByGuid($_app,'T',10);
    $popularThemes = $amo->getPopularAddonsByGuid($_app,'T',10);
    /* This is a bit of a cheesy hack because of the way the templates are written.
     * It's looking for the name of the app in $_GET, so here we are...(clouserw)*/
    $_GET['app']       = strtolower($amo->getAppNameFromGuid($_app));
} else {
    $newestThemes  = $amo->getNewestAddons($clean['app'],'T',10);
    $popularThemes = $amo->getPopularAddons($clean['app'],'T',10);
}

// Assign template variables.
$tpl->assign(
    array(  'newestThemes'      => $newestThemes,
            'popularThemes'     => $popularThemes,
            'title'             => 'Add-ons',
            'currentTab'        => $currentTab,
            'content'           => 'themes.tpl',
            'sidebar'           => 'inc/category-sidebar.tpl',
            'cats'              => $amo->getCats('T'),
            'type'              => 'T')
);
?>
