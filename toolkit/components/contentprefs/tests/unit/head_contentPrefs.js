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
 * The Original Code is Content Preferences (cpref).
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
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

// Inspired by the Places infrastructure in head_bookmarks.js

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const CONTENT_PREFS_DB_FILENAME = "content-prefs.sqlite";
const CONTENT_PREFS_BACKUP_DB_FILENAME = "content-prefs.sqlite.corrupt";

var ContentPrefTest = {
  //**************************************************************************//
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


  //**************************************************************************//
  // nsISupports
  
  interfaces: [Ci.nsIDirectoryServiceProvider, Ci.nsISupports],

  QueryInterface: function ContentPrefTest_QueryInterface(iid) {
    if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  },


  //**************************************************************************//
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


  //**************************************************************************//
  // Utilities

  getURI: function ContentPrefTest_getURI(spec) {
    return this._ioSvc.newURI(spec, null, null);
  },

  /**
   * Get the profile directory, registering ourselves as a provider
   * of that directory if necessary.
   */
  getProfileDir: function ContentPrefTest_getProfileDir() {
    var profileDir;

    try {
      profileDir = this._dirSvc.get("ProfD", Ci.nsIFile);
    }
    catch (e) {}

    if (!profileDir) {
      this._dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(this);
      profileDir = this._dirSvc.get("ProfD", Ci.nsIFile);
      this._dirSvc.unregisterProvider(this);
    }

    return profileDir;
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
      try { file.remove(false); } catch(e) { /* stupid windows box */ }
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

ContentPrefTest.deleteDatabase();

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

