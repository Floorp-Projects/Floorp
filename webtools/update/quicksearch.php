<?php
// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
//
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
//
// The Original Code is Mozilla Update.
//
// The Initial Developer of the Original Code is
// Chris "Wolf" Crews.
// Portions created by the Initial Developer are Copyright (C) 2004
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
//   Chris "Wolf" Crews <psychoticwolf@carolina.rr.com>
//   Chris "CTho" Thomas <cst@andrew.cmu.edu>
//   Alan "alanjstr" Starr <bugzilla-alanjstrBugs@sneakemail.com>
//   Ted "luser" Mielczarek <ted.mielczarek@gmail.com>
//
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
//
// ***** END LICENSE BLOCK *****
?>
<?php
require"core/config.php";
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html lang="en">
<head>

<?php
// Prints which items are being displayed and what page is being displayed
// i.e. "X - Y of Z | Page M of N"
function print_page_list($startitem, $enditem, $totalresults, $num_pages, $pageid)
{
  global $searchStr, $section;

  echo"<h1>Results</h1>\n";

  echo "$startitem - $enditem of $totalresults&nbsp;&nbsp;|&nbsp;&nbsp;";

  $previd=$pageid-1;
  if ($previd >"0") {
    echo"<a href=\"?q=$searchStr&amp;section=$section&amp;pageid=$previd\">&#171; Previous</a> &bull; ";
  }
  echo "Page $pageid of $num_pages";

  $nextid=$pageid+1;
  if ($pageid <$num_pages) {
    echo" &bull; <a href=\"?q=$searchStr&amp;section=$section&amp;pageid=$nextid\">Next &#187;</a>";
  }
}

 //----------------------------
 //Global General $_GET variables
 //----------------------------
 //Detection Override
$didSearch = 0;
if ($_GET["q"]) $didSearch = 1;

$items_per_page="10"; //Default Num per Page is 10

//Default PageID is 1
if (!$_GET["pageid"]) {
  $pageid="1";
}
else {
  $pageid = intval($_GET["pageid"]);
  if($pageid == 0)
    $pageid = 1;
}

// grab query string
if ($_GET["q"]) {
  $searchStr = escape_string($_GET["q"]);
  $section = escape_string($_GET["section"]);
}

?>
  <title>Mozilla Update :: Search<?php if ($didSearch) {echo " - Results - Page $pageid"; } ?></title>
<?php
include"$page_header";

// -----------------------------------------------
// Begin Content of the Page Here
// -----------------------------------------------

?>
<div id="mBody">
<?php
if ($didSearch) {
  // build our query
  $sql = "FROM `main` TM
WHERE (TM.name LIKE '%$searchStr%' OR TM.Description LIKE '%$searchStr%')";

  // search extensions/themes/etc
  if($section !== "A") {
    $sql .= " AND TM.Type = '$section'";
  }

  // get the number of items first.
  // this is so that we can figure out what page we're on
  // and only pull $items_per_page results in the real query
  $resultsquery = "SELECT COUNT(*) " . $sql;

  // Get Total Results from Result Query & Populate Page Control Vars.
  $sql_result = mysql_query($resultsquery, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
  $row = mysql_fetch_array($sql_result);
  $totalresults = $row[0];
  unset($row);

  // Total # of Pages
  $num_pages = ceil($totalresults/$items_per_page);
  $num_pages = max($num_pages,1);

  // Check PageId for Validity
  $pageid = min($num_pages, max(1, $pageid));
  // Determine startitem,startpoint, enditem
  $startpoint = ($pageid-1) * $items_per_page;
  $startitem = $startpoint + 1;
  $enditem = $startpoint + $items_per_page;
  if ($totalresults == "0") { $startitem = "0"; }

  // Verify EndItem
  if ($enditem > $totalresults) { $enditem = $totalresults; }

  // now build the query to get the item details
  // only $items_per_page number results,
  // starting at $startpoint
  $resultsquery = "SELECT TM.ID, TM.Name, TM.Description, TM.Type
" . $sql ."
ORDER BY TM.Name
LIMIT $items_per_page OFFSET $startpoint";

  print_page_list($startitem, $enditem, $totalresults, $num_pages, $pageid);
  echo"<br><br>\n";

    //---------------------------------
    // Begin List
    //---------------------------------
    // Query for items (if there are any)
    if($totalresults > 0) {
      //XXX this absolutely sucks.  can't we have a function
      // to get the info URL for an item given its id?
      $typedirs = array("E"=>"extensions","T"=>"themes","U"=>"update");

      $sql_result = mysql_query($resultsquery, $connection) or trigger_error("MySQL Error ".mysql_errno().": ".mysql_error()."", E_USER_NOTICE);
      while ($row = mysql_fetch_array($sql_result)) {
        $id = $row["ID"];
        $name = $row["Name"];
        $description = $row["Description"];
        $itemtype = $row["Type"];

        echo "<div class=\"item\">\n";
        $typedir = $typedirs[$itemtype];
        echo "<h2 class=\"first\"><a href=\"$typedir/moreinfo.php?id=$id\">";
        echo htmlspecialchars($name);
        echo "</a></h2>";

        // Description
        echo htmlspecialchars(substr($description,0,250));
        echo"</div>\n";

      } //End While Loop
    }
    else {

?>
<div id="item" class="noitems">
No items found matching "<?=$searchStr?>".
</div>
<?php

    }
  }
  else {  // no search, display a form
    ?>
<form action="quicksearch.php" method="GET">
<div class="key-point">
Search For: <input type="text" name="q">
<select name="section" id="sectionsearch">
  <option value="A">Entire Site</option>
  <option value="E">Extensions</option>
  <option value="T">Themes</option>
<!--
  <option value="P">Plugins</option>
  <option value="S">Search Engines</option>
-->
</select>
<input type="submit" value="search">
</div>
</form>
    <?
  }

// Footer page # display
if ($didSearch) {
  print_page_list($startitem, $enditem, $totalresults, $num_pages, $pageid);
  echo"<br>\n";

  // Skip to Page...
  if ($pageid <= $num_pages && $num_pages > 1) {
      echo "Jump to Page: ";
      $pagesperpage = 9; // Plus 1 by default..
      $i = 01;
      //Dynamic Starting Point
      if ($pageid > 11) {
	$nextpage = $pageid - 10;
      }
      $i = $nextpage;

      // Dynamic Ending Point
      $maxpagesonpage = $pageid + $pagesperpage;
      // Page #s
      while ($i <= $maxpagesonpage && $i <= $num_pages) {
	if ($i==$pageid) {
	  echo"<span style=\"color: #FF0000\">$i</span>&nbsp;";
	} else {
	  echo"<a href=\"?q=$searchStr&amp;section=$section&amp;pageid=$i\">$i</a>&nbsp;";
	}

	$i++;
      }
  }
}

?>
</div>
<?php
include"$page_footer";
?>
</body>
</html>