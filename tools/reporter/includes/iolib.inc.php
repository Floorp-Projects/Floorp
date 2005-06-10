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
  return $problemTypes[$q];
}

function resolveBehindLogin($q){
  if ($q == 1){
      return "Yes";
  }
  return "No";
}

function navigation($pre_href='?page=', $post_href='', $num_items=0, $items_per_page=25, $active=1, $nearby=5, $threshold=100){
  // Original From: http://www.phpbuilder.com/snippet/detail.php?type=snippet&id=866
  // License:  GNU General Public License
  // Info: 1336  2.0.1  10/19/02 18:01  mp96brbj

/*
  The main function:
  Returns HTML string with a navigation bar, easily styled with CSS.
  Returns false if no navigation bar was necessary (if everything
  could fit on one page).

  $pre_href = everyhing in the links' HREF before the active page's number.
  $post_href = everyhing *after* the active page's number.
  $num_items = total number of items available.
  $items_per_page = well, d'uh!
  $active = the active page's number.
  $nearby = minimum number of nearby pages to $active to display.
  $threshold = the smaller the number, the more restrictive the 
    selection of links will be when $total is very high. This is to 
    conserve space.

  These default settings limit the number of links to a maximum 
  of about 20.
*/

  // &//8230; is the ellipse character: "..."
  $space = '<span class="spacer"> &#8230; '."\n\t".'</span>';

  // There's no point in printing this string if there are no items,
  // Or if they all fit on one page.
  if ($num_items > 0 && $num_items > $items_per_page)
  {
    // STEP 1:
    // Force variables into certain values.

    // $items_per_page can't be smaller than 1!
    // Also, avoid division by zero later on.
    $items_per_page = max($items_per_page, 1);

    // Calculate the number of listing pages.
    $total = ceil($num_items/$items_per_page);

    // $active can't be higher than $total or smaller than 1!
    $active = max( min($active,$total), 1);

    // STEP 2:
    // Do the rest.

    // Get the sequence of pages to show links to.
    $pages = navigationSequence($total, $active, $nearby, $threshold);
    
    // Print a descriptive string.
    $first = ($active-1)*$items_per_page + 1;
    $last  = min($first+$items_per_page-1, $num_items);
    if ($first == $last)
      $listing = $first;
    else
      // &//8211; is the EN dash, the proper hyphen to use.
      $listing = $first.' - '.$last;
    $r = '<p class="navigation">'."\n\tShowing $listing of $num_items<br />\n";
  
    // Initialize the list of links.
    $links = array();
  
    // Add "previous" link.
    if ($active > 1 && $total > 1)
      $links[] = '<a href="'.$pre_href.($active-1).$post_href.
                 '" class="prev" title="Previous">&laquo;</a>';
  
    // Decide how the each link should be presented.
    for($i=0; $i<sizeof($pages); $i++)
    {
      // Current link.
      $curr = $pages[$i];

      // See if we should any $spacer in connection to this link.
      if ($i>0 AND $i<sizeof($pages)-1)
      {
        $prev = $pages[$i-1];
        $next = $pages[$i+1];

        // See if we should any $spacer *before* this link.
        // (Don't add one if the last link is already a spacer.)
        if ($prev < $curr-1 AND $links[sizeof($links)-1] != $space)
          $links[] = $space;
      }

      // Add the link itself!
      // If the link is not the active page, link it.
      if ($curr != $active)
        $links[] = '<a href="'.$pre_href.$curr.$post_href.'">'.$curr.'</a>';
      // Else don't link it.
      else
        $links[] = '<strong class="active">'.$active.'</strong>';

      if ($i>0 AND $i<sizeof($pages)-1)
      {
        // See if we should any $spacer *after* this link.
        // (Don't add one if the last link is already a spacer.)
        if ($next > $curr+1 AND $links[sizeof($links)-1] != $space)
          $links[] = $space;
      }
    }
  
    // Add "next" link.
    if ($active < $total && $total > 1)
      $links[] = '<a href="'.$pre_href.($active+1).$post_href.
                 '" class="next" title="Next">&raquo;</a>';
  
    // Put it all together.
    $r .= "\t".implode($links, "\n\t")."\n</p>\n";
    $r = str_replace("\n\t".$space."\n\t", $space, $r);

    return $r;
  }
  else
    return false;
}

function navigationSequence($total=1, $active=1, $nearby=5, $threshold=100){
  // Original From: http://www.phpbuilder.com/snippet/detail.php?type=snippet&id=866
  // License:  GNU General Public License
  // Info: 1336  2.0.1  10/19/02 18:01  mp96brbj
  // STEP 1:
  // Force minimum and maximum values.
  $total     = (int)max($total, 1);
  $active    = (int)min( max($active,1), $total);
  $nearby    = (int)max($nearby, 2);
  $threshold = (int)max($threshold, 1);

  // STEP 2:
  // Initialize $pages.

  // Array where each key is a page.
  // That way we can easily overwrite duplicate keys, without
  // worrying about the order of elements.
  // Begin by adding the first, the active and the final page.
  $pages = array(1=>1, $active=>1, $total=>1);

  // STEP 3:
  // Add nearby pages.
  for ($diff=1; $diff<$nearby+1; $diff++)
  {
    $pages[min( max($active-$diff,1), $total)] = 1;
    $pages[min( max($active+$diff,1), $total)] = 1;
  }

  // STEP 4:
  // Add distant pages.

  // What are the greatest and smallest distances between page 1 
  // and active page, or between active page and final page?
  $biggest_diff  = max($total-$active, $active);
  $smallest_diff = min($total-$active, $active);

  // The lower $factor is, the bigger jumps (and ergo fewer pages) 
  // there will be between the distant pages.
  $factor = 0.75;
  // If we think there will be too many distant pages to list, 
  // reduce $factor.
  while(pow($total*max($smallest_diff,1), $factor) > $threshold)
    $factor *= 0.9;

  // Add a page between $active and the farthest end (both upwards 
  // and downwards). Then add a page between *that* page and 
  // active. And so on, until we touch the nearby pages.
  for ($diff=round($biggest_diff*$factor); $diff>$nearby; $diff=round($diff*$factor))
  {
    // Calculate the numbers.
    $lower  = $active-$diff;
    $higher = $active+$diff;

    // Round them to significant digits (for readability):
    // Significant digits should be half or less than half of 
    // the total number of digits, but at least 2.
    $lower  = round($lower,  -floor(max(strlen($lower),2)/2));
    $higher = round($higher, -floor(max(strlen($higher),2)/2));

    // Maxe sure they're within the valid range.
    $lower  = min(max($lower,  1),$total);
    $higher = min(max($higher, 1),$total);

    // Add them.
    $pages[$lower]  = 1;
    $pages[$higher] = 1;
  }

  // STEP 5:
  // Convert the keys into values and sort them.
  $return = array_keys($pages);
  sort($return);

  return $return;
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
