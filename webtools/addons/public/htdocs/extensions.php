<?php
/**
 * Home page for extensions, switchable on application.  Since v1 used GUIDs, the
 * flow on this page is a little confusing (we need to support both name and GUID.
 *
 * @package amo
 * @subpackage docs
 */

$currentTab = 'extensions';

startProcessing('extensions.tpl', 'extensions', $compileId);
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
    $newestExtensions  = $amo->getNewestAddonsByGuid($_app,'E',10);
    $popularExtensions = $amo->getPopularAddonsByGuid($_app,'E',10);
    /* This is a bit of a cheesy hack because of the way the templates are written.
     * It's looking for the name of the app in $_GET, so here we are...*/
    $_GET['app']       = strtolower($amo->getAppNameFromGuid($_app));
} else {
    $newestExtensions  = $amo->getNewestAddons($clean['app'],'E',10);
    $popularExtensions = $amo->getPopularAddons($clean['app'],'E',10);
}

// Assign template variables.
$tpl->assign(
    array(  'newestExtensions'  => $newestExtensions,
            'popularExtensions' => $popularExtensions,
            'title'             => 'Add-ons',
            'currentTab'        => $currentTab,
            'content'           => 'extensions.tpl',
            'sidebar'           => 'inc/category-sidebar.tpl',
            'cats'              => $amo->getCats('E'),
            'type'              => 'E')
);
?>
