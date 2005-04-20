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
require_once($config['app_path'].'/includes/adodb/adodb.inc.php');
require_once($config['app_path'].'/includes/iolib.inc.php');

$title = "Searching Results";
include($config['app_path'].'/includes/header.inc.php');
include($config['app_path'].'/includes/message.inc.php');

// approved "selectable" fields
$approved_fields = array('count' /*special */, 'host_id', 'host_hostname', 'report_id', 'report_url', 'report_host_id', 'report_problem_type', 'report_description', 'report_behind_login', 'report_useragent', 'report_platform', 'report_oscpu', 'report_language', 'report_gecko', 'report_buildconfig', 'report_product', 'report_email', 'report_ip', 'report_file_date');


//  Ascending or Descending
if (strtolower($_GET['ascdesc']) == 'asc' || strtolower($_GET['ascdesc']) == 'asc'){
	$ascdesc = $_GET['ascdesc'];
} else {
	$ascdesc = 'desc';
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
$db = NewADOConnection($config['db_dsn']);
if (!$db) die("Connection failed");
$db->SetFetchMode(ADODB_FETCH_ASSOC);

// Initial selected array
$selected = array('report_id' => 'Report ID', 'host_hostname' => 'Host');

if (isset($_GET['count'])){
	$selected['count'] = 'Number';
	unset($selected['report_id']);

	// Hardcode host_id
	$_GET['count'] = 'host_id'; // XXX we just hardcode this (just easier for now, and all people will be doing).
	// XX NOTE:  We don't escape count below because 'host_id' != `host_id`.

	//Sort by
	if ($orderby == 'report_file_date'){      //XXX this isn't ideal, but nobody will sort by date (pointless and not an option)
		$orderby = 'count';
	}
}
else {
	$selected['report_file_date'] = "Date";
}

// Build SELECT clause of SQL
reset($selected);
while (list($key, $title) = each($selected)) {
	if (in_array($key, $approved_fields)){
		// we don't $db->quote here since unless it's in our approved array (exactly), we drop it anyway. i.e. report_id is on our list, 'report_id' is not.
		// we sanitize on our own
		if ($key == 'count'){
			$sql_select .= 'COUNT( '.$_GET['count'].' ) AS count';
		} else {
			$sql_select .= $key;
		}
		$sql_select .= ',';
	}
	// silently drop those not in approved array
}
$sql_select = substr($sql_select, 0, -1);

if (isset($_GET['count'])){
	$group_by = 'GROUP BY '.$_GET['count'];
}

// Build the Where clause of the SQL
if (isset($_GET['submit_reportID'])){
	$sql_where = 'report_id = '.$db->quote($_GET['report_id']).' ';
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
				if (($val != -1) && ($val != null) && ($val != '0')){
					// if there's a wildcard (%,_) we should use 'LIKE', otherwise '='
					// XX-> strpos returns 0 if the first char is % or _, so we just pad it with a 'x' to force it to do so... harmless hack
					if ((strpos('x'.$val, "%") == false) && (strpos('x'.$val, "_") == false)){
						$operator = "=";
					} else {
						$operator = "LIKE";
					}
					// Add to query
					if (in_array($param, $approved_fields)){
						$sql_where .= $param." ".$operator." ".$db->quote($val)." AND ";
					}
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
			$sql_where .= "(report_file_date BETWEEN ".$db->quote($_GET['report_file_date_start'])." and ".$db->quote($_GET['report_file_date_end']).") AND ";
		}

		// if we have only a start, then we do a >
		else if ($_GET['report_file_date_start']){
			$sql_where .= "report_file_date > ".$db->quote($_GET['report_file_date_start'])." AND ";
		}

		// if we have only a end, we do a <
		else if ($_GET['report_file_date_end']){
			$sql_where .= "report_file_date < ".$db->quote($_GET['report_file_date_end'])." AND ";
		}
	}

	$sql_where .= 'host.host_id = report_host_id AND ';
	$sql_where = substr($sql_where, 0, -5);

	if ($orderby != 'report_file_date'){
		$subOrder = ', report.report_file_date DESC';
	}
} else {
	?><h1>No Query</h1><?php
	exit;
}

// Security note:  we escapeSimple() $select as we generate it above (escape each $key), so it would be redundant to do so here.
//					Not to mention it would break things

/* SelectLimit isn't bad, but there's no documentation on getting it to use ASC rather than DESC... to investigate */

$start = ($_GET['page']-1)*$_GET['show'];

$sql = "SELECT $sql_select
	FROM `report`, `host`
	WHERE $sql_where
	$group_by
	ORDER BY ".$db->quote($orderby)." ".$ascdesc.$subOrder;
$query = $db->SelectLimit($sql,$_GET['show'],$start,$inputarr=false);
$numresults = $query->RecordCount();

if (isset($_GET['count'])){
	$totalresults = $_GET['show'];
}
else {
	$trq = $db->Execute("SELECT count(*)
				FROM `report`, `host`
				WHERE $sql_where");
	$totalresults = $trq->fields['count(*)'];
}
?><table id="query_table">
  <?php /*  RESULTS LIST HEADER */ ?>
	<tr>
  	<?php
	// Continuity params
	reset($_GET);
	while (list($param, $val) = each($_GET)) {
	if (($param != 'orderby') && ($param != 'ascdesc'))
		$continuity_params .= $param.'='.rawurlencode($val).'&amp;';
	}

	reset($selected);
	while (list($key, $title) = each($selected)) { ?>
		<th>
		<?PHP if ($key != 'report_id'){ ?>
		<a href="<?php print $config['self']; ?>?orderby=<?php print $key; ?>&amp;ascdesc=<?php
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
		?>">
		<?PHP } ?>
		<?php print $title; ?><?PHP if ($key != 'report_id'){ ?></a><?PHP } ?></th>
	<?php } ?>
	</tr>
	<?php if ($numresults < 1){ ?>
		<tr><td colspan="<?php print sizeof($selected); ?>"><h3>No Results found</h3></td></tr>
	<?php } else { ?>
		<?php for ($i=0; !$query->EOF; $i++) { ?>
			<tr <?PHP if ($i % 2 == 1){ ?>class="alt" <?PHP } ?> >
				<?php reset($selected);
					  while (list($key, $title) = each($selected)) { ?>
				<td><?php
					// For report_id we create a url, for anything else:  just dump it to screen
					if ($key == 'report_id'){
						?><a href="<?php print $config['app_url'].'/report/?report_id='.$query->fields[$key] ?>">Report</a><?php
					}
					else if (substr($key, 0, 5) == "COUNT"){
						print $query->fields['count'];
					} else {
						if(($key == $_GET['count']) || ($key == 'host_hostname' && $_GET['count'] == 'host_id')){
							if ($key == 'host_hostname' && $_GET['count'] == 'host_id'){
								$subquery = 'host_hostname='.$query->fields['host_hostname'];
							}
							else {
								$subquery = $_GET['count'].'='.$query->fields[$key];
							}
							?><a href="<?php print $config['app_url']; ?>/query/?<?PHP print $subquery; ?>&submit_query=true">
							<?PHP print $query->fields[$key]; ?></a>
				<?PHP }
						else {
							print $query->fields[$key];
						}
					 }	?>
					</td>
				<?php $count++;
				     } ?>
			</tr>
	<?php		$query->MoveNext();
			}
		}
	?>

<?php
	// disconnect database
	$db->Close();
?>
</table>
<?php
	reset($_GET);
	while (list($param, $val) = each($_GET)) {
		if (($param != 'page') && ($param != 'show'))
			$paginate_params .= $param.'='.rawurlencode($val).'&amp;';
	}
	$paginate_params = substr($paginate_params, 0, -5);
?>
<?php print navigation('?page=', '&amp;'.$paginate_params.'&amp;show='.$_GET['show'], $totalresults, $_GET['show'], $_GET['page']); ?>
<p><a href="<?PHP print $config['app_url']; ?>/?<?PHP print $continuity_params; ?>">Edit Query</a> &nbsp; | &nbsp; <a href="<?PHP print $config['app_url']; ?>/">New Query</a></p>
<?php include($config['app_path'].'/includes/footer.inc.php'); ?>
