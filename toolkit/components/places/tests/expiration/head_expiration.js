/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Places Unit Tests.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net> (Original Author)
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

const NS_APP_USER_PROFILE_50_DIR = "ProfD";
const NS_APP_HISTORY_50_FILE = "UHist";

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

function LOG(aMsg) {
  aMsg = ("*** PLACES TESTS: " + aMsg);
  Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService).
                                      logStringMessage(aMsg);
  print(aMsg);
}

// If there's no location registered for the profile direcotry, register one now.
var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
var profileDir = null;
try {
  profileDir = dirSvc.get(NS_APP_USER_PROFILE_50_DIR, Ci.nsIFile);
} catch (e) {}
if (!profileDir) {
  // Register our own provider for the profile directory.
  // It will simply return the current directory.
  var provider = {
    getFile: function(prop, persistent) {
      persistent.value = true;
      if (prop == NS_APP_USER_PROFILE_50_DIR)
        return dirSvc.get("CurProcD", Ci.nsIFile);
      if (prop == NS_APP_HISTORY_50_FILE) {
        var histFile = dirSvc.get("CurProcD", Ci.nsIFile);
        histFile.append("history.dat");
        return histFile;
      }
      throw Cr.NS_ERROR_FAILURE;
    },
    QueryInterface: function(iid) {
      if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  };
  dirSvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);
}

var iosvc = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

function uri(spec) {
  return iosvc.newURI(spec, null, null);
}

// Delete a previously created sqlite file
function clearDB() {
  try {
    var file = dirSvc.get('ProfD', Ci.nsIFile);
    file.append("places.sqlite");
    if (file.exists())
      file.remove(false);
  } catch(ex) { dump("Exception: " + ex); }
}
clearDB();

/**
 * Dumps the rows of a table out to the console.
 *
 * @param aName
 *        The name of the table or view to output.
 */
function dump_table(aName)
{
  let db = DBConn()
  let stmt = db.createStatement("SELECT * FROM " + aName);

  dump("\n*** Printing data from " + aName + ":\n");
  let count = 0;
  while (stmt.executeStep()) {
    let columns = stmt.numEntries;

    if (count == 0) {
      // Print the column names.
      for (let i = 0; i < columns; i++)
        dump(stmt.getColumnName(i) + "\t");
      dump("\n");
    }

    // Print the row.
    for (let i = 0; i < columns; i++) {
      switch (stmt.getTypeOfIndex(i)) {
        case Ci.mozIStorageValueArray.VALUE_TYPE_NULL:
          dump("NULL\t");
          break;
        case Ci.mozIStorageValueArray.VALUE_TYPE_INTEGER:
          dump(stmt.getInt64(i) + "\t");
          break;
        case Ci.mozIStorageValueArray.VALUE_TYPE_FLOAT:
          dump(stmt.getDouble(i) + "\t");
          break;
        case Ci.mozIStorageValueArray.VALUE_TYPE_TEXT:
          dump(stmt.getString(i) + "\t");
          break;
      }
    }
    dump("\n");

    count++;
  }
  dump("*** There were a total of " + count + " rows of data.\n\n");

  stmt.reset();
  stmt.finalize();
}

/**
 * Function tests to see if the place associated with the bookmark with id
 * aBookmarkId has the uri aExpectedURI.  The event will call do_test_finished()
 * if aFinish is true.
 *
 * @param aBookmarkId
 *        The bookmark to check against.
 * @param aExpectedURI
 *        The URI we expect to be in moz_places.
 * @param aExpected
 *        Indicates if we expect to get a result or not.
 * @param [optional] aFinish
 *        Indicates if the test should be completed or not.
 */
function new_test_bookmark_uri_event(aBookmarkId, aExpectedURI, aExpected, aFinish)
{
  let db = DBConn();
  let stmt = db.createStatement(
    "SELECT moz_places.url " +
    "FROM moz_bookmarks INNER JOIN moz_places " +
    "ON moz_bookmarks.fk = moz_places.id " +
    "WHERE moz_bookmarks.id = ?1"
  );
  stmt.bindInt64Parameter(0, aBookmarkId);

  if (aExpected) {
    do_check_true(stmt.executeStep());
    do_check_eq(stmt.getUTF8String(0), aExpectedURI);
  }
  else {
    do_check_false(stmt.executeStep());
  }
  stmt.reset();
  stmt.finalize();

  if (aFinish)
    do_test_finished();
}

/**
 * Function tests to see if the place associated with the visit with id aVisitId
 * has the uri aExpectedURI.  The event will call do_test_finished() if aFinish is
 * true.
 *
 * @param aVisitId
 *        The visit to check against.
 * @param aExpectedURI
 *        The URI we expect to be in moz_places.
 * @param aExpected
 *        Indicates if we expect to get a result or not.
 * @param [optional] aFinish
 *        Indicates if the test should be completed or not.
 */
function new_test_visit_uri_event(aVisitId, aExpectedURI, aExpected, aFinish)
{
  let db = DBConn();
  let stmt = db.createStatement(
    "SELECT moz_places.url " +
    "FROM moz_historyvisits INNER JOIN moz_places " +
    "ON moz_historyvisits.place_id = moz_places.id " +
    "WHERE moz_historyvisits.id = ?1"
  );
  stmt.bindInt64Parameter(0, aVisitId);

  if (aExpected) {
    do_check_true(stmt.executeStep());
    do_check_eq(stmt.getUTF8String(0), aExpectedURI);
  }
  else {
    do_check_false(stmt.executeStep());
  }
  stmt.reset();
  stmt.finalize();

  if (aFinish)
    do_test_finished();
}

/**
 * Function gets current database connection, if the connection has been closed
 * it will try to reconnect to the places.sqlite database.
 */
function DBConn()
{
  let db = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsPIPlacesDatabase).
           DBConnection;
  if (db.connectionReady)
    return db;

  // open a new connection if needed
  let file = dirSvc.get('ProfD', Ci.nsIFile);
  file.append("places.sqlite");
  let storageService = Cc["@mozilla.org/storage/service;1"].
                       getService(Ci.mozIStorageService);

  try {
    return dbConn = storageService.openDatabase(file);
  }
  catch(ex) {}

  return null;
}

/**
 * Flushes any events in the event loop of the main thread.
 */
function flush_main_thread_events()
{
  let tm = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);
  while (tm.mainThread.hasPendingEvents())
    tm.mainThread.processNextEvent(false);
}

// Simulates a Places shutdown.
function shutdownPlaces()
{
  const TOPIC_XPCOM_SHUTDOWN = "xpcom-shutdown";
  let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsIObserver);
  hs.observe(null, TOPIC_XPCOM_SHUTDOWN, null);
  let sync = Cc["@mozilla.org/places/sync;1"].getService(Ci.nsIObserver);
  sync.observe(null, TOPIC_XPCOM_SHUTDOWN, null);
  let expire = Cc["@mozilla.org/places/expiration;1"].getService(Ci.nsIObserver);
  expire.observe(null, TOPIC_XPCOM_SHUTDOWN, null);
}

// Simulates an expiration at shutdown.
function shutdownExpiration()
{
  const TOPIC_XPCOM_SHUTDOWN = "xpcom-shutdown";
  let expire = Cc["@mozilla.org/places/expiration;1"].getService(Ci.nsIObserver);
  expire.observe(null, TOPIC_XPCOM_SHUTDOWN, null);
}

/**
 * Causes expiration component to start, otherwise it would wait for the first
 * history notification.
 */
function force_expiration_start() {
  Cc["@mozilla.org/places/expiration;1"].getService(Ci.nsISupports);
}

/**
 * Forces an expiration run.
 */
function force_expiration_step(aLimit) {
  if (!aLimit)
    aLimit = -1; // No limit.
  const TOPIC_DEBUG_START_EXPIRATION = "places-debug-start-expiration";
  let expire = Cc["@mozilla.org/places/expiration;1"].getService(Ci.nsIObserver);
  expire.observe(null, TOPIC_DEBUG_START_EXPIRATION, aLimit);
}

/**
 * Clears history invoking callback when done.
 */
function waitForClearHistory(aCallback) {
  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      os.removeObserver(this, TOPIC_EXPIRATION_FINISHED);
      aCallback();
    }
  };
  os.addObserver(observer, TOPIC_EXPIRATION_FINISHED, false);

  let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  hs.QueryInterface(Ci.nsIBrowserHistory).removeAllPages();
}

/**
 * Expiration preferences helpers.
 */
let prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);

function setInterval(aNewInterval) {
  prefs.setIntPref("places.history.expiration.interval_seconds", aNewInterval);
}
function getInterval() {
  return prefs.getIntPref("places.history.expiration.interval_seconds");
}
function clearInterval() {
  try {
    prefs.clearUserPref("places.history.expiration.interval_seconds");
  }
  catch(ex) {}
}

function setMaxPages(aNewMaxPages) {
  prefs.setIntPref("places.history.expiration.max_pages", aNewMaxPages);
}
function getMaxPages() {
  return prefs.getIntPref("places.history.expiration.max_pages");
}
function clearMaxPages() {
  try {
    prefs.clearUserPref("places.history.expiration.max_pages");
  }
  catch(ex) {}
}

function setHistoryEnabled(aHistoryEnabled) {
  prefs.setBoolPref("places.history.enabled", aHistoryEnabled);
}
function getHistoryEnabled() {
  return prefs.getBoolPref("places.history.enabled");
}
function clearHistoryEnabled() {
  try {
    prefs.clearUserPref("places.history.enabled");
  }
  catch(ex) {}
}
