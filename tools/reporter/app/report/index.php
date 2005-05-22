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
require_once($config['app_path'].'/includes/security.inc.php');

// Start Session
session_name('reportSessID');
session_start();
header("Cache-control: private"); //IE 6 Fix


// Open DB
$db = NewADOConnection($config['db_dsn']);
if (!$db) die("Connection failed");
$db->SetFetchMode(ADODB_FETCH_ASSOC);

$query =& $db->Execute("SELECT *
                      FROM report, host
                      WHERE report.report_id = ".$db->quote($_GET['report_id'])."
                      AND host.host_id = report_host_id");

// disconnect database
$db->Close();

$title = "Report for - ".$query->fields['host_hostname'];

include($config['app_path'].'/includes/header.inc.php');
include($config['app_path'].'/includes/message.inc.php');

if (!$query->fields){
	?><h1>No Report Found</h1><?php
	exit;
}
?>

<table id="report_table">
	<tr>
		<th>Report ID:</th>
		<td><?php print $query->fields['report_id']; ?></td>
	</tr>
	<tr>
		<th>URL:</th>
		<td><a href="<?php print $query->fields['report_url']; ?>" target="_blank" rel="nofollow"><?php print $query->fields['report_url']; ?></a></td>
	</tr>
	<tr>
		<th>Host:</th>
		<td><a href="<?php print $config['app_url']; ?>/query/?host_hostname=<?php print $query->fields['host_hostname']; ?>&submit_query=Query">Reports For This Host</a></td>
	</tr>

	<tr>
		<th>Problem Type:</th>
		<td><?php print resolveProblemTypes($query->fields['report_problem_type']); ?></td>
	</tr>
	<tr>
		<th>Behind Login:</th>
		<td><?php print $boolTypes[$query->fields['report_behind_login']]; ?></td>
	</tr>
	<tr>
		<th>Product:</th>
		<td><?php print $query->fields['report_product']; ?></td>
	</tr>
	<tr>
		<th>Gecko:</th>
		<td><?php print $query->fields['report_gecko']; ?></td>
	</tr>
	<tr>
		<th>Useragent:</th>
		<td><?php print $query->fields['report_useragent']; ?></td>
	</tr>
	<tr>
		<th>Build Config:</th>
		<td><?php print $query->fields['report_buildconfig']; ?></td>
	</tr>

	<tr>
		<th>Platform:</th>
		<td><?php print $query->fields['report_platform']; ?></td>
	</tr>
	<tr>
		<th>OS:</th>
		<td><?php print $query->fields['report_oscpu']; ?></td>
	</tr>

	<tr>
		<th>Language:</th>
		<td><?php print $query->fields['report_language']; ?></td>
	</tr>
	<tr>
		<th>Date Filed:</th>
		<td><?php print $query->fields['report_file_date']; ?></td>
	</tr>
	<?php if ($userlib->isLoggedIn()){ ?>
	<tr>
		<th>Email:</th>
		<td><a href="mailto:<?php print $query->fields['report_email']; ?>"><?php print $query->fields['report_email']; ?></a></td>
	</tr>
	<tr>
		<th>IP:</th>
		<td><a href="http://ws.arin.net/cgi-bin/whois.pl?queryinput=<?php print $query->fields['report_ip']; ?>" target="_blank"><?php print $query->fields['report_ip']; ?></a></td>
	</tr>
	<?php } ?>
	<tr>
		<th>Description:</th>
		<td><?php print str_replace("\n", "<br />", $query->fields['report_description']); ?></td>
	</tr>
</table>
<?php include($config['app_path'].'/includes/footer.inc.php'); ?>
