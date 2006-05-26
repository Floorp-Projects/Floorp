<?php
// ***** BEGIN LICENSE BLOCK *****
//
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
// The Original Code is AUS.
//
// The Initial Developer of the Original Code is Mike Morgan.
// 
// Portions created by the Initial Developer are Copyright (C) 2006 
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
//   Mike Morgan <morgamic@mozilla.com>
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

/**
 * AUS sanity check intro page.
 *
 * @package aus
 * @subpackage sanity
 * @author Mike Morgan
 */

// Read .ini file for config options.
$config = parse_ini_file('./sanity.ini',true);

// Include common functions.
require_once('./sanity.inc.php');



/**
 * Redirect to log or reports if they have been requested.
 */
if (!empty($_POST['redirect'])) {
    $path = (isset($_POST['logs'])) ? './log/' : './reports/';
    header('Location: '.$path.$_POST['redirect']);
    exit;
}



/**
 * Regenerate control files.
 */
if (!empty($_POST['control']) && !empty($_POST['controlName'])) {
    
    // Set destination directory.
    $dir = $config['sources']['controlFiles'].str_replace(' ','_',$_POST['controlName']);

    // If this directory does not exist, create it.
    if (!is_dir($dir)) {
        mkdir($dir);
    }

    $msg = array('Added control files successfully...');

    // For each test case, grab and store results.
    foreach ($config['testCases'] as $name=>$url) {
        $result = trim(file_get_contents($_POST['control'].$url));
        $file = $dir.'/'.str_replace(' ','_',$name).'.xml';
        write($file,$result);
        $msg[] = $file;
    }
}



/**
 * Read log and reports directories.
 */

// Gather possible options for controls.
$controls_select = '';
$controls = ls($config['sources']['controlFiles'],'/^[^.].*/');
foreach ($controls as $dir) {
    $controls_select .= '<option value="'.$dir.'">'.$dir.'</option>'."\n";
}

// Gather possible targets for select list.
$targets_select = '';
if (!empty($config['targets']) && is_array($config['targets'])) {
    foreach ($config['targets'] as $name=>$val) {
        $targets_select .= '<option value="'.$val.'">'.$name.'</option>';
    }
}

// Log files from the log directory defined in our config.
$logs_select = '';
$logs = ls($config['sources']['log'],'/^.*log$/','asc');
foreach ($logs as $filename) {
    $buf = explode('.',$filename);
    $readable = timify($buf[0]);
    $logs_select .= '<option value="'.$filename.'">'.$readable.'</option>'."\n";
}

// HTML Reports from the reports directory defined in our config.
$reports_select = '';
$reports = ls($config['sources']['reports'],'/^.*html$/','asc');
foreach ($reports as $filename) {
    $buf = explode('.',$filename);
    $readable = timify($buf[0],false);
    $reports_select .= '<option value="'.$filename.'">'.$readable.'</option>'."\n";
}





/**
 * Generate HTML.
 */
$html = '';
$html .= <<<HEADER
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
    <title>AUS Regression Tests :: mozilla.org</title>
    <meta http-equiv="content-type" content="text/html; charset=utf-8"/>
    <meta name="rating" content="General"/>
    <meta name="robots" content="All"/>
    <style type="text/css">
        .control { display: block; float: left; width: 15em; }
        .test { display: block; float: left; width: 8em; }
        .msg { color: green; }
    </style>
</head>
<body>
<h1>AUS Regression Testing</h1>
<p>All tests use the defined test cases in <kbd>sanity.ini</kbd>.</p>
HEADER;

// Messages, if any.
if (!empty($msg) && is_array($msg)) {
    $html .= '<ul class="msg">'."\n";
    foreach ($msg as $li) {
        $html .= '<li>'.$li.'</li>'."\n";
    }
    $html .= '</ul>'."\n";
}

$html .= <<<PAGE
<h2>Run a New Test</h2>
<p>To begin a test, choose a <kbd>Control</kbd> and a <kbd>Target</kbd> then hit
<kbd>Begin Test</kbd>.  You will be redirected to a static HTML report.</p>
<form action="./sanity.php" method="post">
<div>
    <label for="testControl" class="test">Control</label>
    <select name="testControl" id="testControl">
    {$controls_select}
    </select>
</div><br/>
<fieldset>
<legend>Target</legend>
<div>
    <label for="testTarget" class="test">Defined Target</label>
    <select name="testTarget" id="testTarget">
    {$targets_select}
    </select>
</div>
<p>-- OR --</p>
<div>
    <label for="testTargetOverride" class="test">Custom Target</label>
    <input type="text" name="testTargetOverride" id="testTargetOverride" value="" size="77"/>
</div>
<p><em>Note:</em> If a <kbd>Custom Target</kbd> is defined, it will override any selected <kbd>Defined Targets</kbd>.</p> 
</fieldset><br/>
<div>
    <input type="submit" name="submit" value="Begin Test &raquo;"/>
</div>
</form>

<h2>Generate New Control Files</h2>
<form action="./" method="post">
<div>
    <label for="controlName" class="control">Name of control source</label>
    <input type="text" name="controlName" id="controlName" value="{$config['defaults']['controlName']}" size="77"/>
    <input type="hidden" name="action" value="control" />
</div><br/>
<div>
    <label for="control" class="control">Location of control source</label>
    <input type="text" name="control" id="control" value="{$config['defaults']['control']}" size="77"/>
</div><br/>
<div>
    <input type="submit" name="submit" value="Generate New Control Files &raquo;"/>
</div>
</form>

<h2>Logs &amp; Reports</h2>
<form action="./" method="post">
<h3>Logs</h3>
<div>
    <select name="redirect" id="logs">
    {$logs_select}
    </select>
    <input type="submit" name="logs" value="View Log &raquo;"/>
</div>
</form>

<h3>Reports</h3>
<form action="./" method="post">
<div>
    <select name="redirect" id="reports">
    {$reports_select}
    </select>
    <input type="submit" name="reports" value="View Report &raquo;"/>
</div>
</form>
PAGE;

$html .= <<<FOOTER
</body>
</html>
FOOTER;

echo $html;
?>
