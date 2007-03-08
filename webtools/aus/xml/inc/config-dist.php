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
 * Configuration file.
 * @package aus
 * @subpackage inc
 * @author Mike Morgan
 */
// define('SOURCE_DIR','/home/morgamic/public_html/auslite/source');
define('SOURCE_DIR',getcwd().'/data');

// This is the directory containin channel-specific updates.
// Snippets in this directory override normal updates.
define('OVERRIDE_DIR',getcwd().'/data/3');

// Uncomment this line in order to echo text debug information.
define('DEBUG',false);

// Define default for Update blocks.
define('UPDATE_TYPE','minor');
define('UPDATE_VERSION','1.0+');
define('UPDATE_EXTENSION_VERSION','1.0+');

// These are channels that have access to nightly updates.
// All other channels only have access to the OVERRIDE_DIR for update info.
$nightlyChannels = array(
    'nightly'
);

// This hash defines the product->version->patch relationships for nightlies
// It determines which patches are associated to which incoming client versions.
// @todo replace this with a better datasource that can be easily managed via a GUI.
$productBranchVersions = array(
    'Firefox'     =>  array(
        '1.5.0.*' =>  '1.5.0.x',
        '1.6a1'   =>  'trunk',
        '1.*'     =>  '1.5',
        '2.0*'    =>  '2.0',
        '3.0a*'   =>  'trunk'
    ),
    'Thunderbird' =>  array(
        '1.5.0.*' =>  '1.5.0.x',
        '2.0*'    =>  '2.0',
        '3.0a*'   =>  'trunk'
    ),
    'Sunbird'     =>  array(
        '0.4a1'   =>  'branch',
        '0.5*'    =>  'branch',
        '0.6a1'   =>  'trunk'
    )
);

// Array that defines which %OS_VERSION% values are no longer supported.
// For incoming URIs containing these as their platformVersion, no updates
// will be offered.
$unsupportedPlatforms = array(
    'Windows 3.1 Professional Edition XP Limited Edition with Batteries (tm)',
    'CareBears 1.0',
    'Windows_98'
);
?>
