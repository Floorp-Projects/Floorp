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

// const Cc, Ci, and Cr are defined in netwerk/test/httpserver/httpd.js so we
// need to define unique ones.
const AUS_Cc = Components.classes;
const AUS_Ci = Components.interfaces;
const AUS_Cr = Components.results;

var gAUS           = null;
var gUpdateChecker = null;
var gPrefs         = null;
var gTestserver    = null;

/* Initializes the most commonly used global vars used by tests */
function startAUS() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1.0", "2.0");
  gPrefs = AUS_Cc["@mozilla.org/preferences;1"]
             .getService(AUS_Ci.nsIPrefBranch);

  // Enable Update logging (see below)
  gPrefs.setBoolPref("app.update.log.all", true);
  // Lessens the noise in the logs when using Update Service logging (see below)
  gPrefs.setBoolPref("app.update.enabled", false);
  gPrefs.setBoolPref("extensions.blocklist.enabled", false);
  gPrefs.setBoolPref("extensions.update.enabled", false);
  gPrefs.setBoolPref("browser.search.update", false);
  gPrefs.setBoolPref("browser.microsummary.updateGenerators", false);

  gAUS = AUS_Cc["@mozilla.org/updates/update-service;1"]
           .getService(AUS_Ci.nsIApplicationUpdateService);
  gUpdateChecker = AUS_Cc["@mozilla.org/updates/update-checker;1"]
                     .createInstance(AUS_Ci.nsIUpdateChecker);
  // Uncomment the following two lines to log Update Service logging to the
  // test logs.
/*
  var os = AUS_Cc["@mozilla.org/observer-service;1"]
             .getService(AUS_Ci.nsIObserverService);
  os.notifyObservers(null, "profile-after-change", null);
*/
}

/**
 * Removes the updates directory and the active-update.xml file if they exist.
 * This prevents some tests from failing due to files being left behind when
 * the tests are interrupted.
 */
function remove_dirs_and_files () {
  var fileLocator = AUS_Cc["@mozilla.org/file/directory_service;1"]
                      .getService(AUS_Ci.nsIProperties);
  var dir = fileLocator.get("XCurProcD", AUS_Ci.nsIFile);

  var file = dir.clone();
  file.append("active-update.xml");
  if (file.exists())
    file.remove(false);

  dir.append("updates");
  if (dir.exists())
    dir.remove(true);
}

/**
 * Helper for starting the http server used by the tests
 * @param   aRelativeDirName
 *          The directory name to register relative to
 *          toolkit/mozapps/update/test/unit/
 */
function start_httpserver(aRelativeDirName) {
  do_import_script("netwerk/test/httpserver/httpd.js");
  gTestserver = new nsHttpServer();
  gTestserver.registerDirectory("/data/", do_get_file("toolkit/mozapps/update/test/unit/" + aRelativeDirName));
  gTestserver.start(4444);
}

/* Helper for stopping the http server used by the tests */
function stop_httpserver() {
  gTestserver.stop();
}

/**
 * Creates an nsIXULAppInfo
 * @param   id
 *          The ID of the test application
 * @param   name
 *          A name for the test application
 * @param   version
 *          The version of the application
 * @param   platformVersion
 *          The gecko version of the application
 */
function createAppInfo(id, name, version, platformVersion)
{
  const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
  const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");
  var XULAppInfo = {
    vendor: "Mozilla",
    name: name,
    ID: id,
    version: version,
    appBuildID: "2007010101",
    platformVersion: platformVersion,
    platformBuildID: "2007010101",
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",

    QueryInterface: function QueryInterface(iid) {
      if (iid.equals(AUS_Ci.nsIXULAppInfo) ||
          iid.equals(AUS_Ci.nsIXULRuntime) ||
          iid.equals(AUS_Ci.nsISupports))
        return this;
    
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  
  var XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      return XULAppInfo.QueryInterface(iid);
    }
  };

  var registrar = Components.manager.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

remove_dirs_and_files();
