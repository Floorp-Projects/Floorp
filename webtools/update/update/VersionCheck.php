<?php
/* -*- Mode: php; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is the Extension Update Service.
 *
 * The Initial Developer of the Original Code is Vladimir Vukicevic.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Vladimir Vukicevic <vladimir@pobox.com>
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

/// config bits:
$db_server = "localhost";
$db_user = "";
$db_pass = "";
$db_name = "";

// map the mysql t_main.type enum into the right type
$ext_typemap = array('T' => 'theme',
                     'E' => 'extension',
                     'P' => 'plugin');

header("Content-type: text/rdf");

// error handling
function bail ($errstr) {
    die("Error: " . $errstr);
}


// major.minor.release.build[+]
// make sure this is a valid version
function expandversion ($vstr) {
    $v = explode('.', $vstr);

    if ($vstr == '' || count($v) == 0 || count($v) > 4) {
        bail ('Bogus version.');
    }

    $vlen = count($v);
    $ret = array();
    $hasplus = 0;

    for ($i = 0; $i < 4; $i++) {
        if ($i > $vlen-1) {
            // this version chunk was not specified; give 0
            $ret[] = 0;
        } else {
            $s = $v[$i];
            if ($i == 3) {
                // need to check for +
                $slen = strlen($s);
                if ($s{$slen-1} == '+') {
                    $s = substr($s, 0, $slen-1);
                    $hasplus = 1;
                }
            }

            $ret[] = intval($s);
        }
    }

    $ret[] = $hasplus;

    return $ret;
}

function vercmp ($a, $b) {
    if ($a == $b)
        return 0;

    $va = expandversion($a);
    $vb = expandversion($b);

    for ($i = 0; $i < 5; $i++)
        if ($va[$i] != $vb[$i])
            return ($vb[$i] - $va[$i]);

    return 0;
}


//
// These are passed in the GET string
//

if (!array_key_exists('reqVersion', $_GET))
    bail ("Invalid request.");

$reqVersion = $_GET['reqVersion'];

if ($reqVersion == 1) {

    if (!array_key_exists('id', $_GET) ||
        !array_key_exists('version', $_GET) ||
        !array_key_exists('maxAppVersion', $_GET) ||
        !array_key_exists('appID', $_GET) ||
        !array_key_exists('appVersion', $_GET))
        bail ("Invalid request.");

    $reqItemGuid = $_GET['id'];
    $reqItemVersion = $_GET['version'];
    $reqItemMaxAppVersion = $_GET['maxAppVersion'];
    $reqTargetAppGuid = $_GET['appID'];
    $reqTargetAppVersion = $_GET['appVersion'];
} else {
    // bail
    bail ("Bad request version received");
}

// check args
if (empty($reqItemGuid) || empty($reqItemVersion) || empty($reqTargetAppGuid)) {
    bail ("Invalid request.");
}

// XXX PUT VALUES IN
mysql_connect($db_server, $db_user, $db_pass)
    || bail ("Failed to connect to database.");

mysql_select_db ($db_name)
    || bail ("Failed to select database.");

// We need to fetch two things for the database:
// 1) The current extension version's info, for a possibly updated max version
// 2) The latest version available, if different from the above.
//
// We know:
//  - $reqItemGuid
//  - $reqItemVersion
//  - $reqTargetAppGuid
//  - $reqTargetAppVersion
//
// We need to get:
//  - extension GUID
//  - extension version
//  - extension xpi link
//  - app ID
//  - app min version
//  - app max version

$query = "SELECT t_main.guid AS extguid,
                 t_main.type AS exttype,
                 t_version.version AS extversion,
                 t_version.uri AS exturi,
                 t_version.minappver AS appminver,
                 t_version.maxappver AS appmaxver,
                 t_applications.guid AS appguid
          FROM t_main, t_version, t_applications
          WHERE t_main.guid = '" . mysql_real_escape_string($reqItemGuid) . "' AND
                t_main.id = t_version.id AND
                t_version.appid = t_applications.appid AND
                t_version.approved = 'YES' AND
                t_applications.guid = '" . mysql_real_escape_string($reqTargetAppGuid) . "'";

$result = mysql_query ($query);

if (!$result) {
    bail ('Query error: ' . mysql_error());
}

// info for this version
$thisVersionData = '';
// info for highest version
$highestVersion = '';
$highestVersionData = '';

$itemType = '';

while ($line = mysql_fetch_array($result, MYSQL_ASSOC)) {
    if (empty($itemType)) {
        $itemType = $ext_typemap[$line['exttype']];
    }

    // is this row for the current version?
    if ($line['extversion'] == $reqItemVersion) {
        $thisVersionData = $line;
    } else if (vercmp ($reqItemVersion, $line['extversion']) > 0) {
        // did we already see an update with a higher version than this?
        if ($highestVersion != '' && vercmp ($highestVersion, $line['extversion']) < 0)
            continue;

        // does this update support my current app version?
        if (vercmp($line['appmaxver'], $reqTargetAppVersion) > 0 ||
            vercmp($reqTargetAppVersion, $line['appminver']) > 0)
            continue;

        $highestVersion = $line['extversion'];
        $highestVersionData = $line;
    }
}

mysql_free_result ($result);

//
// Now to spit out the RDF.  We hand-generate because the data is pretty simple.
//

print "<?xml version=\"1.0\"?>\n";
print "<RDF:RDF xmlns:RDF=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" xmlns:em=\"http://www.mozilla.org/2004/em-rdf#\">\n\n";

print "<RDF:Description about=\"urn:mozilla:{$itemType}:{$reqItemGuid}\">\n";

// output list of updates (just highest and this)
print " <em:updates><RDF:Seq>\n";
if (!empty($thisVersionData))
    print "  <RDF:li resource=\"urn:mozilla:{$itemType}:{$reqItemGuid}:{$thisVersionData['extversion']}\"/>\n";
if (!empty($highestVersionData))
    print "  <RDF:li resource=\"urn:mozilla:{$itemType}:{$reqItemGuid}:{$highestVersionData['extversion']}\"/>\n";
print " </RDF:Seq></em:updates>\n";

// output compat bits for firefox 0.9
if (!empty($highestVersionData)) {
    print " <em:version>{$highestVersionData['extversion']}</em:version>\n";
    print " <em:updateLink>{$highestVersionData['exturi']}</em:updateLink>\n";
}

print "</RDF:Description>\n\n";

function print_update ($data) {
    global $ext_typemap;
    $dataItemType = $ext_typemap[$data['exttype']];
    print "<RDF:Description about=\"urn:mozilla:{$dataItemType}:{$data['extguid']}:{$data['extversion']}\">\n";
    print " <em:version>{$data['extversion']}</em:version>\n";
    print " <em:targetApplication>\n";
    print "  <RDF:Description>\n";
    print "   <em:id>{$data['appguid']}</em:id>\n";
    print "   <em:minVersion>{$data['appminver']}</em:minVersion>\n";
    print "   <em:maxVersion>{$data['appmaxver']}</em:maxVersion>\n";
    print "   <em:updateLink>{$data['exturi']}</em:updateLink>\n";
    print "  </RDF:Description>\n";
    print " </em:targetApplication>\n";
    print "</RDF:Description>\n";
}

if (!empty($thisVersionData))
    print_update ($thisVersionData);
if (!empty($highestVersionData))
    print_update ($highestVersionData);

print "</RDF:RDF>\n";

?>

