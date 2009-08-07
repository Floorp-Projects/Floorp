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
 * The Original Code is the Application Update Service.
 *
 * The Initial Developer of the Original Code is
 * Robert Strong <robert.bugzilla@gmail.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
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
 * ***** END LICENSE BLOCK *****
 */

/* General URL Construction Tests */

const URL_PREFIX = URL_HOST + DIR_DATA + "/";

const PREF_APP_UPDATE_CHANNEL        = "app.update.channel";
const PREF_PARTNER_BRANCH            = "app.partner.";
const PREF_APP_DISTRIBUTION          = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION  = "distribution.version";

var gAppInfo;

function run_test() {
  do_test_pending();
  removeUpdateDirsAndFiles();
  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  startAUS();
  startUpdateChecker();
  gAppInfo = AUS_Cc["@mozilla.org/xre/app-info;1"].
             getService(AUS_Ci.nsIXULAppInfo).
             QueryInterface(AUS_Ci.nsIXULRuntime);
  do_timeout(0, "run_test_pt1()");
}

function end_test() {
  do_test_finished();
  cleanUp();
}

// Callback function used by the custom XMLHttpRequest implementation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent() {
  var e = { target: gXHR };
  gXHR.onload.handleEvent(e);
}

// Helper function for parsing the result from the contructed url
function getResult(url) {
  return url.substr(URL_PREFIX.length).split("/")[0];
}

// url constructed with %PRODUCT%
function run_test_pt1() {
  gCheckFunc = check_test_pt1;
  var url = URL_PREFIX + "%PRODUCT%/";
  dump("Testing: url constructed with %PRODUCT% - " + url + "\n");
  getPrefBranch().setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt1() {
  do_check_eq(getResult(gRequestURL), gAppInfo.name);
  run_test_pt2();
}

// url constructed with %VERSION%
function run_test_pt2() {
  gCheckFunc = check_test_pt2;
  var url = URL_PREFIX + "%VERSION%/";
  dump("Testing: url constructed with %VERSION% - " + url + "\n");
  getPrefBranch().setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt2() {
  do_check_eq(getResult(gRequestURL), gAppInfo.version);
  run_test_pt3();
}

// url constructed with %BUILD_ID%
function run_test_pt3() {
  gCheckFunc = check_test_pt3;
  var url = URL_PREFIX + "%BUILD_ID%/";
  dump("Testing: url constructed with %BUILD_ID% - " + url + "\n");
  getPrefBranch().setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt3() {
  do_check_eq(getResult(gRequestURL), gAppInfo.appBuildID);
  run_test_pt4();
}

// url constructed with %BUILD_TARGET%
// XXX TODO - it might be nice if we tested the actual ABI
function run_test_pt4() {
  gCheckFunc = check_test_pt4;
  var url = URL_PREFIX + "%BUILD_TARGET%/";
  dump("Testing: url constructed with %BUILD_TARGET% - " + url + "\n");
  getPrefBranch().setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt4() {
  try {
    abi = gAppInfo.XPCOMABI;
  }
  catch (e) {
    do_throw("nsIXULAppInfo:XPCOMABI not defined\n");
  }

  if (IS_MACOSX) {
    // Mac universal build should report a different ABI than either macppc
    // or mactel. This is necessary since nsUpdateService.js will set the ABI to
    // Universal-gcc3 for Mac universal builds.
    var macutils = AUS_Cc["@mozilla.org/xpcom/mac-utils;1"].
                   getService(AUS_Ci.nsIMacUtils);

    if (macutils.isUniversalBinary)
      abi = "Universal-gcc3";
  }

  do_check_eq(getResult(gRequestURL), gAppInfo.OS + "_" + abi);
  run_test_pt5();
}

// url constructed with %LOCALE%
// Bug 488936 added the update.locale file that stores the update locale
function run_test_pt5() {
  gCheckFunc = check_test_pt5;
  var url = URL_PREFIX + "%LOCALE%/";
  dump("Testing: url constructed with %LOCALE% - " + url + "\n");
  getPrefBranch().setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  try {
    gUpdateChecker.checkForUpdates(updateCheckListener, true);
  }
  catch (e) {
    dump("***\n*** The following error is most likely due to a missing " +
         "update.locale file\n***\n");
    do_throw(e);
  }
}

function check_test_pt5() {
  do_check_eq(getResult(gRequestURL), INSTALL_LOCALE);
  run_test_pt6();
}

// url constructed with %CHANNEL%
function run_test_pt6() {
  gCheckFunc = check_test_pt6;
  var url = URL_PREFIX + "%CHANNEL%/";
  dump("Testing: url constructed with %CHANNEL% - " + url + "\n");
  var pb = getPrefBranch();
  pb.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  var defaults = pb.QueryInterface(AUS_Ci.nsIPrefService).getDefaultBranch(null);
  defaults.setCharPref(PREF_APP_UPDATE_CHANNEL, "bogus_channel");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt6() {
  do_check_eq(getResult(gRequestURL), "bogus_channel");
  run_test_pt7();
}

// url constructed with %CHANNEL% with distribution partners
function run_test_pt7() {
  gCheckFunc = check_test_pt7;
  var url = URL_PREFIX + "%CHANNEL%/";
  dump("Testing: url constructed with %CHANNEL% - " + url + "\n");
  var pb = getPrefBranch();
  pb.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  var defaults = pb.QueryInterface(AUS_Ci.nsIPrefService).getDefaultBranch(null);
  defaults.setCharPref(PREF_PARTNER_BRANCH + "bogus_partner1", "bogus_partner1");
  defaults.setCharPref(PREF_PARTNER_BRANCH + "bogus_partner2", "bogus_partner2");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt7() {
  do_check_eq(getResult(gRequestURL), "bogus_channel-cck-bogus_partner1-bogus_partner2");
  run_test_pt8();
}

// url constructed with %PLATFORM_VERSION%
function run_test_pt8() {
  gCheckFunc = check_test_pt8;
  var url = URL_PREFIX + "%PLATFORM_VERSION%/";
  dump("Testing: url constructed with %PLATFORM_VERSION% - " + url + "\n");
  getPrefBranch().setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt8() {
  do_check_eq(getResult(gRequestURL), gAppInfo.platformVersion);
  run_test_pt9();
}

// url constructed with %OS_VERSION%
function run_test_pt9() {
  gCheckFunc = check_test_pt9;
  var url = URL_PREFIX + "%OS_VERSION%/";
  dump("Testing: url constructed with %OS_VERSION% - " + url + "\n");
  getPrefBranch().setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt9() {
  var osVersion;
  var sysInfo = AUS_Cc["@mozilla.org/system-info;1"].
                getService(AUS_Ci.nsIPropertyBag2);
  osVersion = sysInfo.getProperty("name") + " " + sysInfo.getProperty("version");

  if (osVersion) {
    try {
      osVersion += " (" + sysInfo.getProperty("secondaryLibrary") + ")";
    }
    catch (e) {
      // Not all platforms have a secondary widget library, so an error is
      // nothing to worry about.
    }
    osVersion = encodeURIComponent(osVersion);
  }

  do_check_eq(getResult(gRequestURL), osVersion);
  run_test_pt10();
}

// url constructed with %DISTRIBUTION%
function run_test_pt10() {
  gCheckFunc = check_test_pt10;
  var url = URL_PREFIX + "%DISTRIBUTION%/";
  dump("Testing: url constructed with %DISTRIBUTION% - " + url + "\n");
  var pb = getPrefBranch();
  pb.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  var defaults = pb.QueryInterface(AUS_Ci.nsIPrefService).getDefaultBranch(null);
  defaults.setCharPref(PREF_APP_DISTRIBUTION, "bogus_distro");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt10() {
  do_check_eq(getResult(gRequestURL), "bogus_distro");
  run_test_pt11();
}

// url constructed with %DISTRIBUTION_VERSION%
function run_test_pt11() {
  gCheckFunc = check_test_pt11;
  var url = URL_PREFIX + "%DISTRIBUTION_VERSION%/";
  dump("Testing: url constructed with %DISTRIBUTION_VERSION% - " + url + "\n");
  var pb = getPrefBranch();
  pb.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  var defaults = pb.QueryInterface(AUS_Ci.nsIPrefService).getDefaultBranch(null);
  defaults.setCharPref(PREF_APP_DISTRIBUTION_VERSION, "bogus_distro_version");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt11() {
  do_check_eq(getResult(gRequestURL), "bogus_distro_version");
  run_test_pt12();
}

// url constructed that doesn't have a parameter - bug 454357
function run_test_pt12() {
  gCheckFunc = check_test_pt12;
  var url = URL_PREFIX;
  dump("Testing: url constructed that doesn't have a parameter - " + url + "\n");
  getPrefBranch().setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt12() {
  do_check_eq(getResult(gRequestURL), "?force=1");
  run_test_pt13();
}

// url constructed that has a parameter - bug 454357
function run_test_pt13() {
  gCheckFunc = check_test_pt13;
  var url = URL_PREFIX + "?bogus=param";
  dump("Testing: url constructed that has a parameter - " + url + "\n");
  getPrefBranch().setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, url);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt13() {
  do_check_eq(getResult(gRequestURL), "?bogus=param&force=1");
  end_test();
}
