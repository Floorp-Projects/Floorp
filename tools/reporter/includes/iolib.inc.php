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

function initializeTemplate(){
    global $config;

    $object = new Smarty;
    $object->compile_check = $config['smarty_compile_check'];
    $object->debugging = $config['smarty_debug'];

    if ($config['smarty_template_directory']){
        $object->template_dir = $config['smarty_template_directory'];
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
    global $config, $userlib;

    $object->assign('base_url', $config['base_url']);
    $object->assign('app_url', $config['base_url'].'/app');
    $object->assign('charset', 'utf-8');
    $object->assign('is_admin', $userlib->isLoggedIn());
    return $object;
}

function displayPage($object, $objectTemplate, $title = 'Mozilla Reporter'){
    $page = initializeTemplate();
    $page->assign('content', $object->fetch($objectTemplate));
    $page->assign('title', $title);
    $page->display('layout.tpl');
    return;
}
function strMiddleReduceWordSensitive($string, $max = 50, $rep = '[...]') {
   $strlen = strlen($string);

   if ($strlen <= $max)
       return $string;

   $lengthtokeep = $max - strlen($rep);
   $start = 0;
   $end = 0;

   if (($lengthtokeep % 2) == 0) {
       $start = $lengthtokeep / 2;
       $end = $start;
   } else {
       $start = intval($lengthtokeep / 2);
       $end = $start + 1;
   }

   $i = $start;
   $tmp_string = $string;
   while ($i < $strlen) {
       if ($tmp_string[$i] == ' ') {
           $tmp_string = substr($tmp_string, 0, $i) . $rep;
           $return = $tmp_string;
       }
       $i++;
   }

   $i = $end;
   $tmp_string = strrev ($string);
   while ($i < $strlen) {
       if ($tmp_string[$i] == ' ') {
           $tmp_string = substr($tmp_string, 0, $i);
           $return .= strrev ($tmp_string);
       }
       $i++;
   }
   return $return;
   return substr($string, 0, $start) . $rep . substr($string, - $end);
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
}

function strip_all_tags($input){
	while($input != strip_tags($input)) {
		$input = strip_tags($input);
	}
	return $input; 
}
?>