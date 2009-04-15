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

const NS_APP_USER_PROFILE_50_DIR = "ProfD";
const NS_APP_PROFILE_DIR_STARTUP = "ProfDS";
const NS_GRE_DIR                 = "GreD";

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

const PERMS_FILE      = 0644;
const PERMS_DIRECTORY = 0755;

var gAUS           = null;
var gUpdateChecker = null;
var gPrefs         = null;
var gTestserver    = null;
var gXHR           = null;
var gXHRCallback   = null;

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
 * Writes text to a file. This will replace existing text if the file exists
 * and create the file if it doesn't exist.
 * @param   aFile
 *          The file to write to. Will be created if it doesn't exist.
 * @param   aText
 *          The text to write to the file. If there is existing text it will be
 *          replaced.
 */
function writeFile(aFile, aText) {
  var fos = AUS_Cc["@mozilla.org/network/safe-file-output-stream;1"]
              .createInstance(AUS_Ci.nsIFileOutputStream);
  if (!aFile.exists())
    aFile.create(AUS_Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_FILE);
  var modeFlags = MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE;
  fos.init(aFile, modeFlags, PERMS_FILE, 0);
  fos.write(aText, aText.length);
  closeSafeOutputStream(fos);
}

/**
 * Closes a Safe Output Stream
 * @param   fos
 *          The Safe Output Stream to close
 */
function closeSafeOutputStream(aFOS) {
  if (aFOS instanceof AUS_Ci.nsISafeOutputStream) {
    try {
      aFOS.finish();
    }
    catch (e) {
      aFOS.close();
    }
  }
  else
    aFOS.close();
}

/**
 * Toggles network offline.
 *
 * Be sure to toggle back to online before the test finishes to prevent the
 * following from being printed to the test's log file.
 * WARNING: NS_ENSURE_TRUE(thread) failed: file c:/moz/mozilla-central/mozilla/netwerk/base/src/nsSocketTransportService2.cpp, line 115
 * WARNING: unable to post SHUTDOWN message
 */
function toggleOffline(aOffline) {
  const ioService = AUS_Cc["@mozilla.org/network/io-service;1"]
                      .getService(AUS_Ci.nsIIOService);

  try {
    ioService.manageOfflineStatus = !aOffline;
  }
  catch (e) {
  }
  if (ioService.offline != aOffline)
    ioService.offline = aOffline;
}

/**
 * Sets up the bare bones XMLHttpRequest implementation below. 
 *
 * @param   callback
 *          The callback function that will call the nsIDomEventListener's
 *          handleEvent method.
 *
 *          Example of the callback function
 *
 *            function callHandleEvent() {
 *              gXHR.status = gExpectedStatus;
 *              var e = { target: gXHR };
 *              gXHR.onload.handleEvent(e);
 *            }
 */
function overrideXHR(callback) {
  gXHRCallback = callback;
  gXHR = new xhr();
  var registrar = Components.manager.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  registrar.registerFactory(gXHR.classID, gXHR.classDescription,
                            gXHR.contractID, gXHR);
}

/**
 * Bare bones XMLHttpRequest implementation for testing onprogress, onerror,
 * and onload nsIDomEventListener handleEvent.
 */
function xhr() {
}
xhr.prototype = {
  overrideMimeType: function(mimetype) { },
  setRequestHeader: function(header, value) { },
  status: null,
  channel: { set notificationCallbacks(val) { } },
  _url: null,
  _method: null,
  open: function (method, url) { gXHR._method = method; gXHR._url = url; },
  send: function(body) {
    do_timeout(0, "gXHRCallback()"); // Use a timeout so the XHR completes
  },
  _onprogress: null,
  set onprogress(val) { gXHR._onprogress = val; },
  get onprogress() { return gXHR._onprogress; },
  _onerror: null,
  set onerror(val) { gXHR._onerror = val; },
  get onerror() { return gXHR._onerror; },
  _onload: null,
  set onload(val) { gXHR._onload = val; },
  get onload() { return gXHR._onload; },
  flags: AUS_Ci.nsIClassInfo.SINGLETON,
  implementationLanguage: AUS_Ci.nsIProgrammingLanguage.JAVASCRIPT,
  getHelperForLanguage: function(language) null,
  getInterfaces: function(count) {
    var interfaces = [AUS_Ci.nsIXMLHttpRequest, AUS_Ci.nsIJSXMLHttpRequest,
                      AUS_Ci.nsIXMLHttpRequestEventTarget];
    count.value = interfaces.length;
    return interfaces;
  },
  classDescription: "XMLHttpRequest",
  contractID: "@mozilla.org/xmlextras/xmlhttprequest;1",
  classID: Components.ID("{c9b37f43-4278-4304-a5e0-600991ab08cb}"),
  createInstance: function (outer, aIID) {
    if (outer != null)
      throw AUS_Cr.NS_ERROR_NO_AGGREGATION;
    return gXHR.QueryInterface(aIID);
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(AUS_Ci.nsIXMLHttpRequest) ||
        aIID.equals(AUS_Ci.nsIJSXMLHttpRequest) ||
        aIID.equals(AUS_Ci.nsIXMLHttpRequestEventTarget) ||
        aIID.equals(AUS_Ci.nsIClassInfo) ||
        aIID.equals(AUS_Ci.nsISupports))
      return gXHR;
    throw AUS_Cr.NS_ERROR_NO_INTERFACE;
  }
};

/**
 * Removes the updates directory and the active-update.xml file if they exist.
 * This prevents some tests from failing due to files being left behind when
 * the tests are interrupted.
 */
function remove_dirs_and_files () {
  var dir = gDirSvc.get(NS_GRE_DIR, AUS_Ci.nsIFile);

  var file = dir.clone();
  file.append("active-update.xml");
  try {
    if (file.exists())
      file.remove(false);
  }
  catch (e) {
    dump("Unable to remove file\npath: " + file.path +
         "\nException: " + e + "\n");
  }

  file = dir.clone();
  file.append("updates.xml");
  try {
    if (file.exists())
      file.remove(false);
  }
  catch (e) {
    dump("Unable to remove file\npath: " + file.path +
         "\nException: " + e + "\n");
  }

  file = dir.clone();
  file.append("updates");
  file.append("last-update.log");
  try {
    if (file.exists())
      file.remove(false);
  }
  catch (e) {
    dump("Unable to remove file\npath: " + file.path +
         "\nException: " + e + "\n");
  }

  var updatesSubDir = dir.clone();
  updatesSubDir.append("updates");
  updatesSubDir.append("0");
  if (updatesSubDir.exists()) {
    file = updatesSubDir.clone();
    file.append("update.mar");
    try {
      if (file.exists())
        file.remove(false);
    }
    catch (e) {
      dump("Unable to remove file\npath: " + file.path +
           "\nException: " + e + "\n");
    }

    file = updatesSubDir.clone();
    file.append("update.status");
    try {
      if (file.exists())
        file.remove(false);
    }
    catch (e) {
      dump("Unable to remove file\npath: " + file.path +
           "\nException: " + e + "\n");
    }

    file = updatesSubDir.clone();
    file.append("update.version");
    try {
      if (file.exists())
        file.remove(false);
    }
    catch (e) {
      dump("Unable to remove file\npath: " + file.path +
           "\nException: " + e + "\n");
    }

    try {
      updatesSubDir.remove(true);
    }
    catch (e) {
      dump("Unable to remove directory\npath: " + updatesSubDir.path +
           "\nException: " + e + "\n");
    }
  }

  // This fails sporadically on Mac OS X so wrap it in a try catch
  dir.append("updates");
  try {
    if (dir.exists())
      dir.remove(true);
  }
  catch (e) {
    dump("Unable to remove directory\npath: " + dir.path +
         "\nException: " + e + "\n");
  }
}

/**
 * Helper for starting the http server used by the tests
 * @param   aRelativeDirName
 *          The directory name to register relative to
 *          toolkit/mozapps/update/test/unit/
 */
function start_httpserver(aRelativeDirName) {
  do_load_httpd_js();
  gTestserver = new nsHttpServer();
  gTestserver.registerDirectory("/data/", do_get_file(aRelativeDirName));
  gTestserver.start(4444);
}

/* Helper for stopping the http server used by the tests */
function stop_httpserver(callback) {
  do_check_true(!!callback);
  gTestserver.stop(callback);
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
    
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    }
  };
  
  var XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw AUS_Cr.NS_ERROR_NO_AGGREGATION;
      return XULAppInfo.QueryInterface(iid);
    }
  };

  var registrar = Components.manager.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

// Use a custom profile dir to keep the bin dir clean... not necessary but nice
var gDirSvc = AUS_Cc["@mozilla.org/file/directory_service;1"].
             getService(AUS_Ci.nsIProperties);
// Remove '/unit/*.js'.
var gTestRoot = __LOCATION__.parent.parent;
gTestRoot.normalize();

// Create and register a profile directory.
var gProfD = gTestRoot.clone();
gProfD.append("profile");
if (gProfD.exists())
  gProfD.remove(true);
gProfD.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, 0755);

var dirProvider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    if (prop == NS_APP_USER_PROFILE_50_DIR ||
        prop == NS_APP_PROFILE_DIR_STARTUP)
      return gProfD.clone();
    return null;
  },
  QueryInterface: function(iid) {
    if (iid.equals(AUS_Ci.nsIDirectoryServiceProvider) ||
        iid.equals(AUS_Ci.nsISupports)) {
      return this;
    }
    throw AUS_Cr.NS_ERROR_NO_INTERFACE;
  }
};
gDirSvc.QueryInterface(AUS_Ci.nsIDirectoryService).registerProvider(dirProvider);

remove_dirs_and_files();
