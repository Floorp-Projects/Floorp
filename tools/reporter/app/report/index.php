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
require_once($config['app_path'].'/includes/security.inc.php');

// Start Session
session_name('reportSessID');
session_start();
header("Cache-control: private"); //IE 6 Fix


// Open DB
PEAR::setErrorHandling(PEAR_ERROR_CALLBACK, 'handleErrors');
$db =& DB::connect($config['db_dsn']);

$data =& $db->getRow("SELECT *
                      FROM report, host
                      WHERE report.report_id = '".$db->escapeSimple($_GET['report_id'])."' 
                      AND host.host_id = report_host_id", DB_FETCHMODE_ASSOC);

// disconnect database
$db->disconnect();
       
$title = "Report for - ".$data['host_hostname'];

include($config['app_path'].'/includes/header.inc.php');
include($config['app_path'].'/includes/message.inc.php');

if (!$data){
	?><h1>No Report Found</h1><?php
	exit;
}
?>

<table id="report_table">
	<tr>
		<th>Report ID:</th>
		<td><?php print $data['report_id']; ?></td>
	</tr>
	<tr>
		<th>URL:</th>
		<td><a href="<?php print $data['report_url']; ?>" target="_blank"><?php print $data['report_url']; ?></a></td>
	</tr>
	<tr>
		<th>Host:</th>
		<td><a href="<?php print $config['app_url']; ?>/query/?host_hostname=<?php print $data['host_hostname']; ?>&submit_query=Query">Reports For This Host</a></td>
	</tr>
	
	<tr>
		<th>Problem Type:</th>
		<td><?php print resolveProblemTypes($data['report_problem_type']); ?></td>
	</tr>
	<tr>
		<th>Behind Login:</th>
		<td><?php print $boolTypes[$data['report_behind_login']]; ?></td>
	</tr>
	<tr>
		<th>Product:</th>
		<td><?php print $data['report_product']; ?></td>
	</tr>
	<tr>
		<th>Gecko:</th>
		<td><?php print $data['report_gecko']; ?></td>
	</tr>
	<tr>
		<th>Useragent:</th>
		<td><?php print $data['report_useragent']; ?></td>
	</tr>
	<tr>
		<th>Build Config:</th>
		<td><?php print $data['report_buildconfig']; ?></td>
	</tr>

	<tr>
		<th>Platform:</th>
		<td><?php print $data['report_platform']; ?></td>
	</tr>
	<tr>
		<th>OS:</th>
		<td><?php print $data['report_oscpu']; ?></td>
	</tr>

	<tr>
		<th>Language:</th>
		<td><?php print $data['report_language']; ?></td>
	</tr>
	<tr>
		<th>Date Filed:</th>
		<td><?php print $data['report_file_date']; ?></td>
	</tr>
	<?php if ($userlib->isLoggedIn()){ ?> 
	<tr>
		<th>Email:</th>
		<td><a href="mailto:<?php print $data['report_email']; ?>"><?php print $data['report_email']; ?></a></td>
	</tr>
	<tr>
		<th>IP:</th>
		<td><a href="http://ws.arin.net/cgi-bin/whois.pl?queryinput=<?php print $data['report_ip']; ?>" target="_blank"><?php print $data['report_ip']; ?></a></td>
	</tr>
	<?php } ?>
	<tr>
		<th>Description:</th>
		<td><?php print str_replace("\n", "<br />", $data['report_description']); ?></td>
	</tr>
</table>

<!--report_ip-->
<?php include($config['app_path'].'/includes/footer.inc.php'); ?>
