<?php
/**
 * Global initialization script.
 *
 * @package amo
 * @subpackage docs
 */



/**
 * Require our config.
 */
require_once('config.php');



/**
 * Set runtime options.
 */
ini_set('display_errors',DISPLAY_ERRORS);
ini_set('error_reporting',ERROR_REPORTING);
ini_set('magic_quotes_gpc',0);



/**
 * Name of the current script.
 *
 * This is used in important comparisons with $shadow_config and $cache_config,
 * so it was pulled out of config.php so it's immutable.
 */
define('SCRIPT_NAME',substr($_SERVER['SCRIPT_NAME'], strlen(WEB_PATH.'/'), strlen($_SERVER['SCRIPT_NAME'])));



/**
 * Set up caching if this page requires caching.  First we will include Cache_Lite and create an object.
 *
 * Second, we will check to see if the cache has already been compiled for our ID.
 *
 * This is all done before anything else happens because we want to save cycles.
 */
if (!empty($cache_config[SCRIPT_NAME])) {

    require_once('Cache/Lite.php');

    // Set our cacheLiteId based on our params.
    $cacheLiteId = md5($_SERVER['QUERY_STRING']);

    // Set options.
    $cacheOptions = array(
        'cacheDir' => CACHE_DIR.'/',
        'lifeTime' => $cache_config[SCRIPT_NAME]
    );

    // Instantiate Cache_Lite() object.
    $cache = new Cache_Lite($cacheOptions);

    // If our page is already cached, display from cache and exit.
    if ($cacheData = $cache->get($cacheLiteId,SCRIPT_NAME)) {
        echo $cacheData;
        exit;
    }
}



/**
 * Set up global variables.
 */
$tpl = null;  // Smarty.
$content = null;  // Name of .tpl file to be called for content.
$pageType = null;  // Wrapper type for wrapping content .tpl.
$cacheId = null;  // Name of the page cache for use with Smarty caching.
$currentTab = null;  // Name of the currentTab selected in the header.tpl.  This is also the cacheId for header.tpl.
$compileId = null;  // Name of the compileId for use with Smarty caching.
$clean = array();  // General array for verified inputs.  These variables should not be formatted for a specific medium.
$sql = array();  // Trusted for SQL.  Items in this array must be trusted through logic or through escaping.

// If app is not set or empty, set it to null for our switch.
$_GET['app'] = (!empty($_GET['app'])) ? $_GET['app'] : null;

// Determine our application, and set the compileId to match it.
// cacheId is set per page, since that is a bit more complicated.
switch( $_GET['app'] ) {
    case 'mozilla':
        $compileId = 'mozilla';
        break;
    case 'thunderbird':
        $compileId = 'thunderbird';
        break;
    case 'firefox':
    default:
        $compileId = 'firefox';
        break;
}



/**
 * Include Smarty class.  If we get here, we're going to generate the page.
 */
require_once(SMARTY_BASEDIR.'/Smarty.class.php');  // Smarty



/**
 * Smarty configuration.
 */
class AMO_Smarty extends Smarty
{
    function AMO_Smarty()
    {
        $this->template_dir = TEMPLATE_DIR;
        $this->compile_dir = COMPILE_DIR;
        $this->cache_dir = CACHE_DIR;
        $this->config_dir = CONFIG_DIR;

        // Pass config variables to Smarty object.
        $this->assign('config',
            array(  'webpath'   => WEB_PATH,
                    'rootpath'  => ROOT_PATH,
                    'host'      => HTTP_HOST)
        );
    }
}



/**
 * Begin template processing.  Check to see if page output is cached.
 * If it is already cached, display cached output.
 * If it is not cached, continue typical runtime.
 *
 * NOTE: In order to avoid/disable caching, pass null as the argument for aCacheId and aCompileId.
 *
 * @param string $aTplName name of the template to process
 * @param string $aCacheId name of the cache to check - this corresponds to other $_GET params
 * @param string $aCompileId name of the compileId to check - this is $_GET['app']
 * @param string $aPageType type of the page - nonav, default, etc.
 */
function startProcessing($aTplName, $aCacheId, $aCompileId, $aPageType='default')
{
    // Pass in our global variables.
    global $tpl, $pageType, $content, $cacheId, $compileId, $cache_config;

    $pageType = $aPageType;
    $content = $aTplName;
    $cacheId = $aCacheId;
    $compileId = $aCompileId;

    $tpl = new AMO_Smarty();
}
?>
