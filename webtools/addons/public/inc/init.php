<?php
/**
 * Global initialization script.
 *
 * @package amo
 * @subpackage docs
 * @todo find a more elegant way to push in global template data (like $cats)
 */
// Include config file.
require_once('config.php');
require_once('/usr/local/lib/php/Smarty/Smarty.class.php');  // Smarty

// Set runtime options.
ini_set('display_errors',DISPLAY_ERRORS);
ini_set('error_reporting',ERROR_REPORTING);
ini_set('magic_quotes_gpc',0);

$tpl = null;

// Smarty configuration.
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
                    'repo'      => REPO_DIR)
        );
    }
}

function startProcessing($aTplName, $aPageType)
{
    // Global template object.
    global $tpl, $pageType, $content;
    $pageType = $aPageType;
    $content = $aTplName;

    $tpl = new AMO_Smarty();

    if ($tpl->is_cached($aTplName)) {
      include ("finish.php");
      exit;
    }
}


?>
