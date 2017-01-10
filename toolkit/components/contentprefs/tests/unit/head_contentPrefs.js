/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Inspired by the Places infrastructure in head_bookmarks.js

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/ContentPrefInstance.jsm');

const CONTENT_PREFS_DB_FILENAME = "content-prefs.sqlite";
const CONTENT_PREFS_BACKUP_DB_FILENAME = "content-prefs.sqlite.corrupt";

var ContentPrefTest = {
  // Convenience Getters

  __dirSvc: null,
  get _dirSvc() {
    if (!this.__dirSvc)
      this.__dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                      getService(Ci.nsIProperties);
    return this.__dirSvc;
  },

  __consoleSvc: null,
  get _consoleSvc() {
    if (!this.__consoleSvc)
      this.__consoleSvc = Cc["@mozilla.org/consoleservice;1"].
                          getService(Ci.nsIConsoleService);
    return this.__consoleSvc;
  },

  __ioSvc: null,
  get _ioSvc() {
    if (!this.__ioSvc)
      this.__ioSvc = Cc["@mozilla.org/network/io-service;1"].
                     getService(Ci.nsIIOService);
    return this.__ioSvc;
  },


  // nsISupports

  interfaces: [Ci.nsIDirectoryServiceProvider, Ci.nsISupports],

  QueryInterface: function ContentPrefTest_QueryInterface(iid) {
    if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  },


  // nsIDirectoryServiceProvider

  getFile: function ContentPrefTest_getFile(property, persistent) {
    persistent.value = true;

    if (property == "ProfD")
      return this._dirSvc.get("CurProcD", Ci.nsIFile);

    // This causes extraneous errors to show up in the log when the directory
    // service asks us first for CurProcD and MozBinD.  I wish there was a way
    // to suppress those errors.
    throw Cr.NS_ERROR_FAILURE;
  },


  // Utilities

  getURI: function ContentPrefTest_getURI(spec) {
    return this._ioSvc.newURI(spec);
  },

  /**
   * Get the profile directory.
   */
  getProfileDir: function ContentPrefTest_getProfileDir() {
    // do_get_profile can be only called from a parent process
    if (runningInParent) {
      return do_get_profile();
    }
    // if running in a content process, this just returns the path
    // profile was initialized in the ipc head file
    let env = Components.classes["@mozilla.org/process/environment;1"]
                        .getService(Components.interfaces.nsIEnvironment);
    // the python harness sets this in the environment for us
    let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
    let file = Components.classes["@mozilla.org/file/local;1"]
                         .createInstance(Components.interfaces.nsILocalFile);
    file.initWithPath(profd);
    return file;
  },

  /**
   * Delete the content pref service's persistent datastore.  We do this before
   * and after running tests to make sure we start from scratch each time. We
   * also do it during the database creation, schema migration, and backup tests.
   */
  deleteDatabase: function ContentPrefTest_deleteDatabase() {
    var file = this.getProfileDir();
    file.append(CONTENT_PREFS_DB_FILENAME);
    if (file.exists())
      try { file.remove(false); } catch (e) { /* stupid windows box */ }
    return file;
  },

  /**
   * Delete the backup of the content pref service's persistent datastore.
   * We do this during the database creation, schema migration, and backup tests.
   */
  deleteBackupDatabase: function ContentPrefTest_deleteBackupDatabase() {
    var file = this.getProfileDir();
    file.append(CONTENT_PREFS_BACKUP_DB_FILENAME);
    if (file.exists())
      file.remove(false);
    return file;
  },

  /**
   * Log a message to the console and the test log.
   */
  log: function ContentPrefTest_log(message) {
    message = "*** ContentPrefTest: " + message;
    this._consoleSvc.logStringMessage(message);
    print(message);
  }

};

var gInPrivateBrowsing = false;
function enterPBMode() {
  gInPrivateBrowsing = true;
}
function exitPBMode() {
  gInPrivateBrowsing = false;
  Services.obs.notifyObservers(null, "last-pb-context-exited", null);
}

ContentPrefTest.deleteDatabase();

do_register_cleanup(function() {
  ContentPrefTest.deleteDatabase();
  ContentPrefTest.__dirSvc = null;
});

function inChildProcess() {
  var appInfo = Cc["@mozilla.org/xre/app-info;1"];
  if (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType ==
      Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
    return false;
  }
  return true;
}

// Turn on logging for the content preferences service so we can troubleshoot
// problems with the tests. Note that we cannot do this in a child process
// without crashing (but we don't need it anyhow)
if (!inChildProcess()) {
  var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                   getService(Ci.nsIPrefBranch);
  prefBranch.setBoolPref("browser.preferences.content.log", true);
}
