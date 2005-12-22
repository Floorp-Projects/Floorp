<?php
/**
 * Basic error functions for triggering fatal runtime errors.
 * This class is generally called when a page load should be terminated because of bad inputs.
 *
 * @package amo
 * @subpackage lib
 */

/**
 * @param string $errorMessage Message to display.
 * @param string $errorTemplate Template to call (defaults to error template).
 */
function triggerError($errorMessage=null,$errorTemplate='error.tpl') 
{
    $tpl =& new AMO_Smarty();

    $tpl->assign(
        array(
            'error'=>$errorMessage,
            'content'=>$errorTemplate,
            'backtrace'=>debug_backtrace()
        )
    );

    $tpl->display('inc/wrappers/nonav.tpl');
    exit;
}
