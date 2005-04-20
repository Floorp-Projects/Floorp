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

require_once('../config.inc.php');
require_once($config['app_path'].'/includes/security.inc.php');
require_once($config['app_path'].'/includes/iolib.inc.php');

// Start Session
session_name('reportSessID');
session_start();
header("Cache-control: private"); //IE 6 Fix

include($config['app_path'].'/includes/header.inc.php');
include($config['app_path'].'/includes/message.inc.php');
?>

<fieldset>
	<legend>Look up Report</legend>
	<form method="get" action ="<?php print $config['self']; ?>report/" ID="Form1">
		<label for="report_id">Report ID</label>
		<input type="text" id="report_id" name="report_id" />
    
		<input type="submit" id="submit_reportID" name="submit_reportID" value="Lookup Report" />
	</form>
</fieldset>

<!-- Query -->
<fieldset>
	<legend>Search for a Report</legend>
	<form method="get" action="<?php print $config['self']; ?>query/">
	<table id="queryTable">
		<tr>
			<td class="label"><label for="report_description">Description:</label></td>
 			<td><input id="report_description" name="report_description" type="text" size="35" value="<?php print $_GET['report_description']; ?>"></td>
			<td rowspan="4">
				<table>
					<tr>
						<td class="label"><label for="report_useragent">User Agent:</label></td>
						<td><input id="report_useragent" name="report_useragent" type="text" size="35" value="<?php print $_GET['report_useragent']; ?>"></td>
					</tr>
					<tr>
						<td class="label"><label for="report_gecko">Gecko Version:</label></td>
						<td><input id="report_gecko" name="report_gecko" type="text" value="<?php print $_GET['report_gecko']; ?>"></td>
					</tr>
					<tr>
						<td class="label"><label for="report_language">Language (ex. en-US):</label></td>
						<td><input id="report_language" name="report_language" type="text" value="<?php print $_GET['report_language']; ?>"></td>
					</tr>
					<tr>
						<td class="label"><label for="report_platform">Platform:</label></td>
						<td>
							<table>
								<tr>
									<td>
										<input type="radio" name="report_platform" id="AllPlatforms" value="" <?php if ($_GET['report_platform'] == '') { ?>checked="true"<?php } ?>><label for="AllPlatforms">All</label><br>
										<input type="radio" name="report_platform" id="Win32" value="Win32" <?php if ($_GET['report_platform'] == 'Win32') { ?>checked="true"<?php } ?>><label for="Win32">Windows (32)</label><br>
										<input type="radio" name="report_platform" id="Windows" value="Windows" <?php if ($_GET['report_platform'] == 'Windows') { ?>checked="true"<?php } ?>><label for="Windows">Windows (64 Bit)</label><br>
										<input type="radio" name="report_platform" id="MacPPC" value="MacPPC" <?php if ($_GET['report_platform'] == 'MacPPC') { ?>checked="true"<?php } ?>><label for="MacPPC">MacOS X</label><br>
										<input type="radio" name="report_platform" id="X11" value="X11" <?php if ($_GET['report_platform'] == 'X11') { ?>checked="true"<?php } ?>><label for="X11">X11</label><br>
									</td><td>
										<input type="radio" name="report_platform" id="OS2" value="OS/2" <?php if ($_GET['report_platform'] == 'OS/2') { ?>checked="true"<?php } ?>><label for="OS2">OS/2</label><br>
										<input type="radio" name="report_platform" id="Photon" value="Photon" <?php if ($_GET['report_platform'] == 'Photon') { ?>checked="true"<?php } ?>><label for="Photon">Photon</label><br>
										<input type="radio" name="report_platform" id="BeOS" value="BeOS" <?php if ($_GET['report_platform'] == 'BeOS') { ?>checked="true"<?php } ?>><label for="BeOS">BeOS</label><br>
										<input type="radio" name="report_platform" id="unknown" value="?" <?php if ($_GET['report_platform'] == '?') { ?>checked="true"<?php } ?>><label for="unknown">unknown</label><br>
									</td>
								</tr>
							</table>
						</td>
					</tr>
					<tr>
						<td class="label"><label for="report_oscpu" title="oscpu">OS:</td>
						<td><input id="report_oscpu" name="report_oscpu" type="text" size="35" value="<?php print $_GET['report_oscpu']; ?>"></td>
					</tr>
					<tr>
						<td class="label"><label for="report_product">Product:</label></td>
						<td>
							<select name="report_product" id="report_product">
								<option value="-1">Any</option>
								<?php foreach ($config['products'] as $product){ ?>
								<option value="<?php print $product['id']; ?>" <?php if ($_GET['report_product'] == $product['id']) { ?>selected="true"<?php } ?>><?php print $product['title']; ?></option>
								<?php } ?>
							</select>      
						</td>
					</tr>
					<tr>
						<td class="label"><label for="report_file_date_start">File Date starts:</label></td>
						<td><input id="report_file_date_start" name="report_file_date_start" value="<?php if ($_GET['report_file_date_start']){ print $_GET['report_file_date_start']; } else { ?>YYYY-MM-DD<?php } ?>" onfocus="if(this.value=='YYYY-MM-DD'){this.value=''}" type="text"></td>
					</tr>
					<tr>
						<td class="label"><label for="report_file_date_end">ends:</label></td>
						<td><input id="report_file_date_end" name="report_file_date_end" value="<?php if ($_GET['report_file_date_end']){ print $_GET['report_file_date_end']; } else { ?>YYYY-MM-DD<?php } ?>" onfocus="if(this.value=='YYYY-MM-DD'){this.value=''}" type="text"></td>
					</tr>
					<tr>
						<td class="label"><label for="show">Results Per Page:</label></td>
						<td><input id="show" name="show" value="<?php if ($_GET['show']){ print $_GET['show']; } else { ?>25<?php } ?>" onfocus="if(this.value=='YYYY-MM-DD'){this.value=''}" type="text"></td>
					</tr>
					<tr>
						<td class="label"><label for="count">Get Aggregate:</label></td>
						<td><input type="checkbox" id="count" name="count" <?php if ($_GET['count']){ ?>checked="checked" <?PHP } ?>></td>
					</tr>
				</table>
			</td>
		</tr>
		<tr>
			<td class="label"><label for="host_hostname">Host:</td>
			<td><input id="host_hostname" name="host_hostname" type="text" size="35" value="<?php print $_GET['host_hostname']; ?>"></td>
		</tr>
		<tr>
			<td class="label"><label for="report_problem_type">Problem Type:</label></td>
			<td>
				<input type="radio" name="report_problem_type" id="0" value="0" <?php if (($_GET['report_problem_type'] == '0') || ($_GET['report_problem_type'] == '')) { ?>checked="true"<?php } ?>><label for="0">All types</label><br>
				<?php foreach($problemTypes as $key => $title){ ?>
				<input type="radio" name="report_problem_type" id="<?php print $key; ?>" value="<?php print $key; ?>" <?php if ($_GET['report_problem_type'] == $key) { ?>checked="true"<?php } ?>><label for="<?php print $key; ?>"><?php print $title; ?></label><br>
				<?php } ?>
			</td>
		</tr>
		<tr>
			<td class="label"><label for="report_behind_login">Site requires login:</label></td>
			<td>
				<input type="radio" name="report_behind_login" id="-1" value="-1" <?php if (($_GET['report_behind_login'] == '-1') || ($_GET['report_behind_login'] == '')) { ?>checked="true"<?php } ?>><label for="-1">Any</label>
				<input type="radio" name="report_behind_login" id="0" value="0" <?php if ($_GET['report_behind_login'] == '0') { ?>checked="true"<?php } ?>><label for="0">Yes</label>
				<input type="radio" name="report_behind_login" id="1" value="1" <?php if ($_GET['report_behind_login'] == '1') { ?>checked="true"<?php } ?>><label for="1">No</label>
			</td>
		</tr>

		<tr>
  			<td></td>
			<td colspan="2">
				<input id="submit_query" name="submit_query" value="Search" type="submit">
			</td>
		</tr>
	</table>
	</form>
</fieldset>

<div id="login">
<?php if ($userlib->isLoggedIn()){ ?>
Welcome <?php print $_SESSION['user_realname']; ?> | <a href="logout">Logout</a>
<?php } else { ?>
You are not <a href="login">logged in</a>
<?php } ?>
</div>


<br /><br />
<div id="reporter_note">
<h3>Wildcards</h3>
<p><em>Description</em>, <em>Hostname</em>, <em>Useragent</em> support wildcards.</p>
<p><code>%</code> will search for 0+ characters, <code>_</code> (underscore) is for a single character.</p>
</div>

<?php include($config['app_path'].'/includes/footer.inc.php'); ?>
