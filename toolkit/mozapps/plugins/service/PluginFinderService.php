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
 * The Original Code is the Plugin Finder Service.
 *
 * The Initial Developer of the Original Code is Vladimir Vukicevic.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Vladimir Vukicevic <vladimir@pobox.com>
 *  Doron Rosenberg <doronr@us.ibm.com>
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

if (!array_key_exists('mimetype', $_GET))
    bail ("Invalid request.");

$mimetype = $_GET['mimetype'];

if (!array_key_exists('appID', $_GET) 
    || !array_key_exists('appVersion', $_GET)
    || !array_key_exists('clientOS', $_GET))    
    || !array_key_exists('chromeLocale', $_GET))
   )
    bail ("Invalid request.");

$reqTargetAppGuid = $_GET['appID'];
$reqTargetAppVersion = $_GET['appVersion'];
$clientOS = $_GET['clientOS'];
$chromeLocale = $_GET['chromeLocale'];

// check args
if (empty($reqTargetAppVersion) || empty($reqTargetAppGuid)) {
    bail ("Invalid request.");
}

//
// Now to spit out the RDF.  We hand-generate because the data is pretty simple.
//

if ($mimetype == "application/x-mtx") {
  $name = "Viewpoint Media Player";
  $guid = "{03F998B2-0E00-11D3-A498-00104B6EB52E}";
  $version = "5.0";
  $iconUrl = "";
  $XPILocation = "http://www.nexgenmedia.net/flashlinux/invalid.xpi";
  $InstallerShowsUI = false;
  $manualInstallationURL = "http://www.viewpoint.com/pub/products/vmp.html";
  $licenseURL = "http://www.viewpoint.com/pub/privacy.html";
} else if ($mimetype == "application/x-shockwave-flash") {
  $name = "Flash Player";
  $guid = "{D27CDB6E-AE6D-11cf-96B8-444553540000}";
  $version = "7.0.16";
  $iconUrl = "http://goat.austin.ibm.com:8080/flash.gif";
  $XPILocation = "http://www.nexgenmedia.net/flashlinux/flash-linux.xpi";
  $InstallerShowsUI = "false";
  $manualInstallationURL = "http://www.macromedia.com/go/getflashplayer";
  $licenseURL = "http://www.macromedia.com/shockwave/download/license/desktop/";
} else {
  $name = "";
  $guid = "-1";
  $version = "";
  $iconUrl = "";
  $XPILocation = "";
  $InstallerShowsUI = "";
  $manualInstallationURL = "";
  $licenseURL = "";
}

header("Content-type: application/xml");
print "<?xml version=\"1.0\"?>\n";
print "<RDF:RDF xmlns:RDF=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\" xmlns:pfs=\"http://www.mozilla.org/2004/pfs-rdf#\">\n\n";

print "<RDF:Description about=\"urn:mozilla:plugin-results:{$mimetype}\">\n";

// output list of matching plugins
print " <pfs:plugins><RDF:Seq>\n";
print "   <RDF:li resource=\"urn:mozilla:plugin:{$guid}\"/>\n";
print " </RDF:Seq></pfs:plugins>\n";
print "</RDF:Description>\n\n";

print "<RDF:Description about=\"urn:mozilla:plugin:{$guid}\">\n";
print " <pfs:updates><RDF:Seq>\n";
print "   <RDF:li resource=\"urn:mozilla:plugin:{$guid}:{$version}\"/>\n";
print " </RDF:Seq></pfs:updates>\n";
print "</RDF:Description>\n\n";

print "<RDF:Description about=\"urn:mozilla:plugin:{$guid}:{$version}\">\n";
print " <pfs:name>{$name}</pfs:name>\n";
print " <pfs:requestedMimetype>{$mimetype}</pfs:requestedMimetype>\n";
print " <pfs:guid>{$guid}</pfs:guid>\n";
print " <pfs:version>{$version}</pfs:version>\n";
print " <pfs:IconUrl>{$iconUrl}</pfs:IconUrl>\n";
print " <pfs:XPILocation>{$XPILocation}</pfs:XPILocation>\n";
print " <pfs:InstallerShowsUI>{$InstallerShowsUI}</pfs:InstallerShowsUI>\n";
print " <pfs:manualInstallationURL>{$manualInstallationURL}</pfs:manualInstallationURL>\n";
print " <pfs:licenseURL>{$licenseURL}</pfs:licenseURL>\n";
print "</RDF:Description>\n\n";

print "</RDF:RDF>\n";

?>

