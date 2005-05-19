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

if (!isset($_GET['plain'])) {
	include($config['app_path'].'/includes/header.inc.php');
} else {
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
	"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
	<title><?php print $title; ?></title>
	<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
	<style type="text/css" media="screen">
	body {
		padding: 5px;
	}
	body, h1, h2, h3, h4, h5, h6, li {
	    font-family: verdana, sans-serif;
        font-size: x-small;
        voice-family: "\"}\"";
        voice-family: inherit;
        font-size: small;
    }
	h1, h2, h3, h4, h5, h6 {
		margin: 1em 0 0.2em 0;
		font-family: arial, verdana, sans-serif;
	}
	li h1, li h2, li h3, li h4, li h5, li h6 {
		border: none;
	}
	#header h1 { border: 0; }
		h1 { font-size: 160%; font-weight: normal; }
		h2 { font-size: 150%; font-weight: normal; }
		h3 { font-size: 120%; }
		h4 { font-size: 100%; }
		h5 { font-size: 90%; }
		h6 { font-size: 90%; border: 0; }
	}
	</style>
</head>
<?php } ?>

<h4>How it works</h4>
<p>When you find a site, simply launch the reporter in the help menu, and fill out the short form.  Then submit it</p>

<h4>Why should I participate</h4>
<p>By helping alert us of broken web sites, we can work with the webmaster to correct the problem, and make the website compatible.  You can help make sure Mozilla can access the entire internet</p>

<h4>What about my privacy</h4>
<p>Your privacy is important.  We do collect some information, but we only make anonymous bits of it available in to the general community.  We collect the following: information.</p>
<ul>
  <li>List for the paranoid goes here</li>
  <li>To do later</li>
</ul>

<?php 
if (!isset($_GET['plain'])) {
	include($config['app_path'].'/includes/footer.inc.php');
} else {
?>	</body>
	</html>
<?php } ?>
