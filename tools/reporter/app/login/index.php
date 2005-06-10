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
require_once("../../config.inc.php");
require_once('DB.php');
require_once($config['app_path'].'/includes/iolib.inc.php');
require_once($config['app_path'].'/includes/security.inc.php');

// Start Session
// start the session
session_name('reportSessID');
session_start();
header("Cache-control: private"); //IE 6 Fix
printheaders();

include($config['app_path'].'/includes/header.inc.php');
include($config['app_path'].'/includes/message.inc.php');

if (isset($_POST['redirect'])){
	$redirect = $_POST['redirect'];
}
else if (isset($_GET['redirect'])){
	$redirect = $_GET['redirect'];
}
else {
	$redirect = $config['app_url'];
}


if ($_SESSION['login'] != true){
	// submit form?
	if (isset($_POST['submit_login'])){

		// Open DB
		PEAR::setErrorHandling(PEAR_ERROR_CALLBACK, 'handleErrors');
		$db =& DB::connect($config['db_dsn']);

		$loginCheck = false;		
		$loginCheck = $userlib->login($_POST['username'], $_POST['password']);

		// disconnect database
       $db->disconnect();

		if ($loginCheck[0] == true){
			header("Location: ".$redirect);
			exit;
		} else {
			$error = true;
			?>Login Failed<?php
		}
	}
		?>
		<table>
			<tr>
				<td>
					<fieldset>
						<legend>Login</legend>
						<form method="post" action ="<?php print $config['app_url']; ?>/login/" ID="login">
							<table>
								<tr>
									<td><label for="username">Username: </label></td>
									<td><input type="text" id="username" name="username" <?php if ($error == true){ print 'value="'.$_POST['username'].'" ';}?>/></td>
								</tr>
								<tr>
									<td><label for="password">Password: </label></td>
									<td><input type="password" id="password" name="password" /></td>
								</tr>
							</table>
			 				<input type="hidden" id="redirect" name="redirect" value="<?php print $redirect; ?>" />
							<input type="submit" id="submit_login" name="submit_login" value="Login" />
						</form>
					</fieldset>		
				</td>
				<td valign="top">
					<h5>Administrator Login</h5>
					<p>Contact <a href="http://robert.accettura.com/contact/?subject=Reporter%20Access%20Request">Robert Accettura</a> if you need an admin account.  This is for special circumstances only.</p>
				</td>
			</tr>
		</table>
		<?
} else {
	header("Location: ".$redirect);
}
include($config['app_path'].'/includes/footer.inc.php');
?>
