<?php
/* -*- Mode: php; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// config bits:
$db_server = "";
$db_user = "";
$db_pass = "";
$db_name = "";

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
                 t_version.version AS extversion,
                 t_version.uri AS exturi,
                 t_version.minappver AS appminver,
                 t_version.maxappver AS appmaxver,
                 t_applications.guid AS appguid
          FROM t_main, t_version, t_applications
          WHERE t_main.guid = '" . mysql_real_escape_string($reqItemGuid) . "' AND
                t_main.id = t_version.id AND
                t_version.appid = t_applications.appid AND
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

while ($line = mysql_fetch_array($result, MYSQL_ASSOC)) {
    // is this row for the current version?
    if ($line['extversion'] == $reqItemVersion) {
        // if so
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

print "<RDF:Description about=\"urn:mozilla:extension:{$reqItemGuid}\">\n";

// output list of updates (just highest and this)
print " <em:updates><RDF:Seq>\n";
if (!empty($thisVersionData))
    print "  <RDF:li resource=\"urn:mozilla:extension:{$reqItemGuid}:{$thisVersionData['extversion']}\"/>\n";
if (!empty($highestVersionData))
    print "  <RDF:li resource=\"urn:mozilla:extension:{$reqItemGuid}:{$highestVersionData['extversion']}\"/>\n";
print " </RDF:Seq></em:updates>\n";

// output compat bits for firefox 0.9
if (!empty($highestVersionData)) {
    print " <em:version>{$highestVersionData['extversion']}</em:version>\n";
    print " <em:updateLink>{$highestVersionData['exturi']}</em:updateLink>\n";
}

print "</RDF:Description>\n\n";

function print_update ($data) {
    print "<RDF:Description about=\"urn:mozilla:extension:{$reqItemGuid}:{$data['extversion']}\">\n";
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

