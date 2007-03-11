<?php
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Reporter (r.m.o).
 *
 * The Initial Developer of the Original Code is
 *      Robert Accettura <robert@accettura.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function initializeTemplate($page = ''){
    global $config;

    $object = new Smarty;
    $object->compile_check = $config['smarty_compile_check'];
    $object->debugging = $config['smarty_debug'];

    if ($config['smarty_template_directory']){
        $subdir = '';
        if($page == 'layout'){
            $subdir = '/'.$config['theme'];
        }
        $object->template_dir = $config['smarty_template_directory'].$subdir;
    }
    if ($config['smarty_compile_dir']){
        $object->compile_dir = $config['smarty_compile_dir'];
    }
    if ($config['smarty_cache_dir']){
        $object->cache_dir = $config['smarty_cache_dir'];
    }

    // Add standard variables
    $object = templateStandardVars($object);

    return $object;
}


function templateStandardVars($object){
    global $config, $securitylib;

    $object->assign('base_url', $config['base_url']);
    $object->assign('app_url', $config['base_url'].'/app');
    $object->assign('charset', 'utf-8');
    $object->assign('is_admin', $securitylib->isLoggedIn());
    $object->assign('current_product_family', urlencode($config['current_product_family']));
    return $object;
}

function displayPage($object, $path, $objectTemplate, $title = 'Mozilla Reporter'){
    global $config;

    $page = initializeTemplate('layout');
    $page->assign('content', $object->fetch($objectTemplate));
    $page->assign('title', $title);
    $page->assign('path', $path);
    $page->display('layout.tpl');
    return;
}

function resolveProblemTypes($q){
  global $problemTypes;
  if ($q > sizeof($problemTypes)){
      return "Unknown";
  }
  return $problemTypes[$q];
}

function resolveBehindLogin($q){
  if ($q == 1){
      return "Yes";
  }
  return "No";
}

function printheaders(){
    $now = date("D M j G:i:s Y T");
    header('Expires: ' . $now);
    header('Last-Modified: ' . gmdate("D, d M Y H:i:s") . ' GMT');
    header('Cache-Control: post-check=0, pre-check=0', false);
    header('Pragma: no-cache');
    header('X-Powered-By: A Barrel of Monkey\'s ');
    session_name('reportSessID');
    session_start();
    header("Cache-control: private"); //IE 6 Fix
}

function strip_all_tags($input){
    while($input != strip_tags($input)) {
        $input = strip_tags($input);
    }
    return $input;
}

function myErrorHandler($errno, $errstr, $errfile, $errline) {
    switch ($errno) {
        case E_USER_ERROR:
	    $content = initializeTemplate();
            $content->assign('title', "Error");
	    $content->assign('message', $errstr);
	    displayPage($content, 'error', 'error.tpl');
            exit(1);
        break;
        case E_USER_WARNING:
            echo "<b>My WARNING</b> [$errno] $errstr<br />\n";
            break;
        case E_USER_NOTICE:
            echo "<b>My NOTICE</b> [$errno] $errstr<br />\n";
            break;
        default:
//            echo "Unkown error type: [$errno] $errstr<br />\n";
            break;
    }
}
$old_error_handler = set_error_handler("myErrorHandler");

?>