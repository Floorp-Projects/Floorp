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

require_once('../../config.inc.php');
require_once('DB.php');
require_once($config['app_path'].'/includes/iolib.inc.php');

$title = "Searching Results";
include($config['app_path'].'/includes/header.inc.php');
include($config['app_path'].'/includes/message.inc.php');

//  Ascending or Descending
if (!$_GET['ascdesc']){
	$ascdesc = 'asc';
} else {
	$ascdesc = $_GET['ascdesc'];
}

// order by
if (!$_GET['orderby']){
        $orderby = 'report_file_date';
} else {
        $orderby = $_GET['orderby'];
}

if (!$_GET['show']){
	$_GET['show'] = $config['show'];
}
// no more than 200 results per page
if (!$_GET['show'] > 200){
	$_GET['show'] = 200;
}

if (!$_GET['page']){
	$_GET['page'] = 1;
}

if (isset($_GET['count']) && $_GET['count'] == null){
	$_GET['count'] = 'host_id';
}

// Open DB
PEAR::setErrorHandling(PEAR_ERROR_CALLBACK, 'handleErrors');
$db =& DB::connect($config['db_dsn']);

$selected = array('report_id' => 'Report ID', 'host_hostname' => 'Host', 'report_file_date' => "Date");

if (isset($_GET['count'])){
	$selected['count'] = 'Number';
	unset($selected['report_id']);
}

// Build SELECT clause of SQL
reset($selected);
while (list($key, $title) = each($selected)) {	
	if ($key == 'count'){
		$sql_select .= 'COUNT( `'.$db->escapeSimple($_GET['count']).'` ) AS count';	
	} else {
		$key = $db->escapeSimple($key);
		$sql_select .= "`$key`";
	}

	$sql_select .= ',';
}
$sql_select = substr($sql_select, 0, -1);

if (isset($_GET['count'])){
	$group_by = "GROUP BY ".$db->escapeSimple($_GET['count']);
}

// Build the Where clause of the SQL
if (isset($_GET['submit_reportID'])){
	$sql_where = "report_id = '".$db->escapeSimple($_GET['report_id'])."' ";
	$sql_where .= 'AND host.host_id = report_host_id';
}
else if ($_GET['submit_query']){
	reset($_GET);
	while (list($param, $val) = each($_GET)) {
		// To help prevent stupidity with params, we only add it to the WHERE statement if it's passes as a param we allow
		if (
			($param == 'report_description') || 
			($param == 'host_hostname') || 
			($param == 'report_problem_type') || 
			($param == 'report_behind_login') || 
			($param == 'report_useragent') || 
			($param == 'report_gecko') || 
			($param == 'report_language') || 
			($param == 'report_platform') || 
			($param == 'report_oscpu') ||
			($param == 'report_product')){
				// there sare our various ways of saying "no value"
				if (($val != -1) && ($val != null) && ($val != '0_0')){
					// if there's a wildcard (%,_) we should use 'LIKE', otherwise '='
					// XX-> strpos returns 0 if the first char is % or _, so we just pad it with a 'x' to force it to do so... harmless hack
					if ((strpos('x'.$val, "%") == false) && (strpos('x'.$val, "_") == false)){
						$operator = "=";
					} else {
						$operator = "LIKE";
					}
					// Add to query
					$sql_where .= $db->escapeSimple($param)." ".$operator." '".$db->escapeSimple($val)."' AND ";
				}
		}
	}
	// we do the datetime stuff outside the loop, so it doesn't get fubar

	// if the user didn't delete the default YYYY-MM-DD mask, we do it for them
	if ($_GET['report_file_date_start'] == 'YYYY-MM-DD'){
		$_GET['report_file_date_start'] = null;
	}
	if ($_GET['report_file_date_end'] == 'YYYY-MM-DD'){
		$_GET['report_file_date_end'] = null;
	}
	if (($_GET['report_file_date_start'] != null)  || ($_GET['report_file_date_end'] != null)){
		
		// if we have both, we do a BETWEEN
		if ($_GET['report_file_date_start'] && $_GET['report_file_date_end']){
			$sql_where .= "(report_file_date BETWEEN '".$db->escapeSimple($_GET['report_file_date_start'])."' and '".$db->escapeSimple($_GET['report_file_date_end'])."') AND ";
		}

		// if we have only a start, then we do a >
		else if ($_GET['report_file_date_start']){
			$sql_where .= "report_file_date > '".$db->escapeSimple($_GET['report_file_date_start'])."' AND ";
		}

		// if we have only a end, we do a <
		else if ($_GET['report_file_date_end']){
			$sql_where .= "report_file_date < '".$db->escapeSimple($_GET['report_file_date_end'])."' AND ";
		}
	}
	
	$sql_where .= 'host.host_id = report_host_id AND ';
	$sql_where = substr($sql_where, 0, -5);
} else {
	?><h1>No Query</h1><?php
	exit;
}

// Security note:  we escapeSimple() $select as we generate it above (escape each $key), so it would be redundant to do so here.  
//					Not to mention it would break things


$start = ($_GET['page']-1)*$_GET['show'];

print "<!-- SELECT $sql_select
                      FROM `report`, `host`
                      WHERE $sql_where
                      ORDER BY ".$db->escapeSimple($orderby)." ".$db->escapeSimple($ascdesc).
                      " LIMIT $start, ".$db->escapeSimple($_GET['show'])."-->";
$result = $db->query("SELECT $sql_select
                      FROM `report`, `host`
                      WHERE $sql_where
					  $group_by
                      ORDER BY ".$db->escapeSimple($orderby)." ".$db->escapeSimple($ascdesc).
                      " LIMIT $start, ".$db->escapeSimple($_GET['show']));
$numresults	= $result->numRows();

if (isset($_GET['count'])){
	$totalresults = 2;
}
else {
$totalresults = $db->getRow("SELECT count(*)
                      FROM `report`, `host`
                      WHERE $sql_where");
$totalresults = $totalresults[0];
}
?><table id="query_table">
  <?php /*  RESULTS LIST HEADER */ ?>
	<tr>
  	<?php 
		  // Continuity params
          reset($_GET);
          while (list($param, $val) = each($_GET)) {	
          if (($param != 'orderby') && ($param != 'ascdesc'))
            $continuity_params .= $param.'='.$val.'&amp;';
          }

          reset($selected); 
  	      while (list($key, $title) = each($selected)) { ?>
		<th><a href="<?php print $config['self']; ?>?orderby=<?php print $key; ?>&amp;ascdesc=<?php 
		if ($orderby == $key) {
			if ($ascdesc == 'asc'){
				print 'desc';
			}
			else if ($ascdesc == 'desc'){
				print 'asc';
			}
		} else {
			print $ascdesc;
		}
    // Always print continuity params:
	$continuity_params = substr($continuity_params, 0, -1);

    print '&'.$continuity_params;
    ?>"><?php print $title; ?></a></th>
	<?php } ?>
	
	</tr>
	<?php if ($numresults < 1){ ?>
		<tr><td colspan="<?php print sizeof($selected); ?>"><h3>No Results found</h3></td></tr>		
	<?php } else { ?>
		<?php for ($i=0; $row = $result->fetchRow(DB_FETCHMODE_ASSOC); $i++) { ?>
			<tr <?PHP if ($i % 2 == 1){ ?>class="alt" <?PHP } ?> >
			  	<?php reset($selected);
			  		  while (list($key, $title) = each($selected)) { ?>	
			  	<td><?php 
			  		// For report_id we create a url, for anything else:  just dump it to screen
			  		if ($key == 'report_id'){
			  			?><a href="<?php print $config['app_url'].'/report/?report_id='.$row[$key] ?>">Report</a><?php
			  		}
			  		else if (substr($key, 0, 5) == "COUNT"){
			  			print $row['count'];			  		
			  		} else {
			  			if($key == $_GET['count']){
			  				?><a href="<?php print $config['app_url']; ?>/query/?<?PHP print $_GET['count'].'='.$row[$key]; ?>&submit_query=true">
			  				<?PHP print $row[$key]; ?></a>
			  	<?PHP   }
			  			else {
			  				print $row[$key];
			  			}	  			
		  			 }	?>
		  			</td>
				<?php $count++; 
				     } ?>
			</tr>
	<?php     } 
		}
	?>
	

<?php  // disconnect database
       $db->disconnect();
 ?>
</table>
<?php 
    reset($_GET);
    while (list($param, $val) = each($_GET)) {	
	if (($param != 'page') && ($param != 'show'))
		$paginate_params .= $param.'='.$val.'&amp;';
	}
	$paginate_params = substr($paginate_params, 0, -5);
?>
<?php print navigation('?page=', '&amp;'.$paginate_params.'&amp;show='.$_GET['show'], $totalresults, $_GET['show'], $_GET['page']); ?>
<p><a href="<?PHP print $config['app_url']; ?>/?<?PHP print $continuity_params; ?>">Edit Query</a> &nbsp; | &nbsp; <a href="<?PHP print $config['app_url']; ?>/">New Query</a></p>
<?php include($config['app_path'].'/includes/footer.inc.php'); ?>
