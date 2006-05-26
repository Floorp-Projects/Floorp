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
 * AUS sanity check.
 *
 * @package aus
 * @subpackage sanity
 * @author Mike Morgan
 */

/**
 * Process test cases and config file.
 */
$config = parse_ini_file('./sanity.ini',true);

// Variables.
$results = array();  // Results array.
$count = 1;  // Test count.
$filename = date('YmdHis');  // Date-based filename for log and report file.

// For each test case, compare output and store results.
foreach ($config['testCases'] as $name=>$url) {
    $time = date('r');  // Time of actual test.

    $control    = (!empty($_POST['testControl'])) ?  $config['sources']['controlFiles'].$_POST['testControl'].'/'.str_replace(' ','_',$name).'.xml' : $config['sources']['controlFiles'].$config['defaults']['controlName'].'/'.str_replace(' ','_',$name).'.xml';

    $target     = (!empty($_POST['testTargetOverride'])) ?  $_POST['testTargetOverride'].$url : ((!empty($_POST['testTarget'])) ?  $_POST['testTarget'].$url : $config['defaults']['target'].$url);

    $controlResult  = trim(file_get_contents($control));
    $targetResult   = trim(file_get_contents($target));


    // @TODO Would be nice to diff this instead.
    // There is a PHP implementation of diff, might try that, time allowing:
    //      http://pear.php.net/package/Text_Diff
    if ($controlResult == $targetResult) {
        $result = 'OK';
    } else {
        $result = 'FAILED';
    }

    // Store results.
    $results[] = array(
        'count' => $count,
        'name' => $name,
        'result' => $result,
        'controlResult' => $controlResult,
        'controlURL' => $control,
        'targetResult' => $targetResult,
        'targetURL' => $target.$url,
        'url' => $url,
        'time' => $time
    );

    // If using the CLI, output to STDOUT.
    if (empty($_SERVER['HTTP_HOST'])) {
        if ($count == 1) {
            echo 'AUS Regression Test Started '.date('YmdHis').' ...'."\n";
        }
        echo "{$count}    {$name}    {$time}    {$result}    {$url}\n";
    }

    $count++;
}

if (empty($_SERVER['HTTP_HOST'])) {
    echo 'Test Completed.  See ./log/'.date('Ymd').'.log for more information, or ./reports/'.date('YmdHis').'.html for an HTML report.'."\n\n";
}



/**
 * Generate HTML for display/write.
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
    <style type="text/css" media="all">
        table { border: 1px solid #666; }
        td { border: 1px solid #666; white-space: nowrap; }
        td.xml { white-space: normal; }
        th { font-weight: bold; background-color: #999; white-space: nowrap; }
        .row1 { background-color: #ccc; } 
        .row2 { background-color: #eee; }
        .OK { background-color: #9f9; }
        .FAILED { background-color: #f99; }
    </style>
</head>
<body>
<h1>Regression Test Results</h1>
HEADER;

$html .= <<<TABLETOP
<table>
    <thead>
        <tr>
            <th>#</th>
            <th nowrap="nowrap">Test Name</th>
            <th nowrap="nowrap">Time</th>
            <th nowrap="nowrap">Test Result</th>
            <th nowrap="nowrap">Params</th>
            <th nowrap="nowrap">Control ({$control})</th>
            <th nowrap="nowrap">Result ({$target})</th>
        </tr>
    </thead>
    <tbody>
TABLETOP;

foreach ($results as $row) {
    $controlResultHTML = htmlentities($row['controlResult']);
    $targetResultHTML  = htmlentities($row['targetResult']);

    $class = $row['count']%2;


    $html .= <<<TABLEROW
            <tr class="row{$class}">
                <td>{$row['count']}</td>
                <td>{$row['name']}</td>
                <td>{$row['time']}</td>
                <td class="{$row['result']}">{$row['result']}</td>
                <td>{$row['url']}</td>
                <td class="xml"><pre>{$controlResultHTML}</pre></td>
                <td class="xml"><pre>{$targetResultHTML}</pre></td>
            </tr>
TABLEROW;
}

$html .= <<<TABLEBOTTOM
    </tbody>
</table>
TABLEBOTTOM;

$html .= <<<FOOTER
</body>
</html>
FOOTER;

// Write HTML report file.
$fp = fopen('./reports/'.$filename.'.html','w+');
fwrite($fp, $html);
fclose($fp);



/**
 * Store all results to log file in ./log directory.
 * Log filenames are date-based.
 */
$log = '';
foreach ($results as $row) {
    $log .= <<<LINE
{$row['count']}    {$row['name']}    {$row['time']}    {$row['result']}    {$row['url']}

LINE;
}

// Write the log file.
// Log files will be written per-day.
$fp = fopen('./log/'.date('Ymd').'.log', 'a');
fwrite($fp, $log);
fclose($fp);



/**
 * If the request is over HTTP, redirect to HTML report.
 */
if (!empty($_SERVER['HTTP_HOST']) && !empty($_POST['submit'])) {
    header('Location: ./reports/'.$filename.'.html'); 
    exit;
}
?>
