/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Places Unit Tests Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net>
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
const NS_APP_PROFILE_DIR_STARTUP = "ProfDS";
const NS_APP_HISTORY_50_FILE = "UHist";
const NS_APP_BOOKMARKS_50_FILE = "BMarks";

// Shortcuts to transactions type.
const TRANSITION_LINK = Ci.nsINavHistoryService.TRANSITION_LINK;
const TRANSITION_TYPED = Ci.nsINavHistoryService.TRANSITION_TYPED;
const TRANSITION_BOOKMARK = Ci.nsINavHistoryService.TRANSITION_BOOKMARK;
const TRANSITION_EMBED = Ci.nsINavHistoryService.TRANSITION_EMBED;
const TRANSITION_FRAMED_LINK = Ci.nsINavHistoryService.TRANSITION_FRAMED_LINK;
const TRANSITION_REDIRECT_PERMANENT = Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT;
const TRANSITION_REDIRECT_TEMPORARY = Ci.nsINavHistoryService.TRANSITION_REDIRECT_TEMPORARY;
const TRANSITION_DOWNLOAD = Ci.nsINavHistoryService.TRANSITION_DOWNLOAD;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "Services", function() {
  Cu.import("resource://gre/modules/Services.jsm");
  return Services;
});

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Cu.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});

XPCOMUtils.defineLazyGetter(this, "PlacesUtils", function() {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
  return PlacesUtils;
});


function LOG(aMsg) {
  aMsg = ("*** PLACES TESTS: " + aMsg);
  Services.console.logStringMessage(aMsg);
  print(aMsg);
}


let gTestDir = do_get_cwd();

// Ensure history is enabled.
Services.prefs.setBoolPref("places.history.enabled", true);

// Initialize profile.
let gProfD = do_get_profile();

// Add our own dirprovider for old history.dat.
let (provider = {
      getFile: function(prop, persistent) {
        persistent.value = true;
        if (prop == NS_APP_HISTORY_50_FILE) {
          let histFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
          histFile.append("history.dat");
          return histFile;
        }
        throw Cr.NS_ERROR_FAILURE;
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider])
    })
{
  Cc["@mozilla.org/file/directory_service;1"].
  getService(Ci.nsIDirectoryService).
  QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);
}


// Remove any old database.
clearDB();


/**
 * Shortcut to create a nsIURI.
 *
 * @param aSpec
 *        URLString of the uri.
 */
function uri(aSpec) NetUtil.newURI(aSpec);


/**
 * Gets the database connection.  If the Places connection is invalid it will
 * try to create a new connection.
 *
 * @return The database connection or null if unable to get one.
 */
function DBConn() {
  let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                              .DBConnection;
  if (db.connectionReady)
    return db;

  // If the database has been closed, then we need to open a new connection.
  let file = Services.dirsvc.get('ProfD', Ci.nsIFile);
  file.append("places.sqlite");
  return Services.storage.openDatabase(file);
};


/**
 * Reads the data from the specified nsIFile, and returns an array of bytes.
 *
 * @param aFile
 *        The nsIFile to read from.
 */
function readFileData(aFile) {
  let inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                    createInstance(Ci.nsIFileInputStream);
  // init the stream as RD_ONLY, -1 == default permissions.
  inputStream.init(aFile, 0x01, -1, null);
  let size = inputStream.available();

  // use a binary input stream to grab the bytes.
  let bis = Cc["@mozilla.org/binaryinputstream;1"].
            createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(inputStream);

  let bytes = bis.readByteArray(size);

  if (size != bytes.length)
      throw "Didn't read expected number of bytes";

  return bytes;
}


/**
 * Compares two arrays, and returns true if they are equal.
 *
 * @param aArray1
 *        First array to compare.
 * @param aArray2
 *        Second array to compare.
 */
function compareArrays(aArray1, aArray2) {
  if (aArray1.length != aArray2.length) {
    print("compareArrays: array lengths differ\n");
    return false;
  }

  for (let i = 0; i < aArray1.length; i++) {
    if (aArray1[i] != aArray2[i]) {
      print("compareArrays: arrays differ at index " + i + ": " +
            "(" + aArray1[i] + ") != (" + aArray2[i] +")\n");
      return false;
    }
  }

  return true;
}


/**
 * Deletes a previously created sqlite file from the profile folder.
 */
function clearDB() {
  try {
    let file = Services.dirsvc.get('ProfD', Ci.nsIFile);
    file.append("places.sqlite");
    if (file.exists())
      file.remove(false);
  } catch(ex) { dump("Exception: " + ex); }
}


/**
 * Dumps the rows of a table out to the console.
 *
 * @param aName
 *        The name of the table or view to output.
 */
function dump_table(aName)
{
  let stmt = DBConn().createStatement("SELECT * FROM " + aName);

  print("\n*** Printing data from " + aName);
  let count = 0;
  while (stmt.executeStep()) {
    let columns = stmt.numEntries;

    if (count == 0) {
      // Print the column names.
      for (let i = 0; i < columns; i++)
        dump(stmt.getColumnName(i) + "\t");
      dump("\n");
    }

    // Print the rows.
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
  print("*** There were a total of " + count + " rows of data.\n");

  stmt.finalize();
}


/**
 * Checks if an address is found in the database.
 * @param aUrl
 *        Address to look for.
 * @return place id of the page or 0 if not found
 */
function page_in_database(aUrl)
{
  let stmt = DBConn().createStatement(
    "SELECT id FROM moz_places_view WHERE url = :url"
  );
  stmt.params.url = aUrl;
  try {
    if (!stmt.executeStep())
      return 0;
    return stmt.getInt64(0);
  }
  finally {
    stmt.finalize();
  }
}


/**
 * Removes all bookmarks and checks for correct cleanup
 */
function remove_all_bookmarks() {
  let PU = PlacesUtils;
  // Clear all bookmarks
  PU.bookmarks.removeFolderChildren(PU.bookmarks.bookmarksMenuFolder);
  PU.bookmarks.removeFolderChildren(PU.bookmarks.toolbarFolder);
  PU.bookmarks.removeFolderChildren(PU.bookmarks.unfiledBookmarksFolder);
  // Check for correct cleanup
  check_no_bookmarks();
}


/**
 * Checks that we don't have any bookmark
 */
function check_no_bookmarks() {
  let query = PlacesUtils.history.getNewQuery();
  let folders = [
    PlacesUtils.bookmarks.toolbarFolder,
    PlacesUtils.bookmarks.bookmarksMenuFolder,
    PlacesUtils.bookmarks.unfiledBookmarksFolder,
  ];
  query.setFolders(folders, 3);
  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  if (root.childCount != 0)
    do_throw("Unable to remove all bookmarks");
  root.containerOpen = false;
}



/**
 * Sets title synchronously for a page in moz_places.
 *
 * @param aURI
 *        An nsIURI to set the title for.
 * @param aTitle
 *        The title to set the page to.
 * @throws if the page is not found in the database.
 *
 * @note This is just a test compatibility mock.
 */
function setPageTitle(aURI, aTitle) {
  PlacesUtils.history.setPageTitle(aURI, aTitle);
}


/**
 * Clears history invoking callback when done.
 *
 * @param aCallback
 *        Callback function to be called once clear history has finished.
 */
function waitForClearHistory(aCallback) {
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, PlacesUtils.TOPIC_EXPIRATION_FINISHED);
      aCallback();
    }
  };
  Services.obs.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  PlacesUtils.bhistory.removeAllPages();
}


/**
 * Simulates a Places shutdown.
 */
function shutdownPlaces(aKeepAliveConnection)
{
  let hs = PlacesUtils.history.QueryInterface(Ci.nsIObserver);
  hs.observe(null, "profile-change-teardown", null);
  hs.observe(null, "profile-before-change", null);
}


/**
 * Creates a bookmarks.html file in the profile folder from a given source file.
 *
 * @param aFilename
 *        Name of the file to copy to the profile folder.  This file must
 *        exist in the directory that contains the test files.
 *
 * @return nsIFile object for the file.
 */
function create_bookmarks_html(aFilename) {
  if (!aFilename)
    do_throw("you must pass a filename to create_bookmarks_html function");
  remove_bookmarks_html();
  let bookmarksHTMLFile = gTestDir.clone();
  bookmarksHTMLFile.append(aFilename);
  do_check_true(bookmarksHTMLFile.exists());
  bookmarksHTMLFile.copyTo(gProfD, FILENAME_BOOKMARKS_HTML);
  let profileBookmarksHTMLFile = gProfD.clone();
  profileBookmarksHTMLFile.append(FILENAME_BOOKMARKS_HTML);
  do_check_true(profileBookmarksHTMLFile.exists());
  return profileBookmarksHTMLFile;
}


/**
 * Remove bookmarks.html file from the profile folder.
 */
function remove_bookmarks_html() {
  let profileBookmarksHTMLFile = gProfD.clone();
  profileBookmarksHTMLFile.append(FILENAME_BOOKMARKS_HTML);
  if (profileBookmarksHTMLFile.exists()) {
    profileBookmarksHTMLFile.remove(false);
    do_check_false(profileBookmarksHTMLFile.exists());
  }
}


/**
 * Check bookmarks.html file exists in the profile folder.
 *
 * @return nsIFile object for the file.
 */
function check_bookmarks_html() {
  let profileBookmarksHTMLFile = gProfD.clone();
  profileBookmarksHTMLFile.append(FILENAME_BOOKMARKS_HTML);
  do_check_true(profileBookmarksHTMLFile.exists());
  return profileBookmarksHTMLFile;
}


/**
 * Creates a JSON backup in the profile folder folder from a given source file.
 *
 * @param aFilename
 *        Name of the file to copy to the profile folder.  This file must
 *        exist in the directory that contains the test files.
 *
 * @return nsIFile object for the file.
 */
function create_JSON_backup(aFilename) {
  if (!aFilename)
    do_throw("you must pass a filename to create_JSON_backup function");
  remove_all_JSON_backups();
  let bookmarksBackupDir = gProfD.clone();
  bookmarksBackupDir.append("bookmarkbackups");
  if (!bookmarksBackupDir.exists()) {
    bookmarksBackupDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0777);
    do_check_true(bookmarksBackupDir.exists());
  }
  let bookmarksJSONFile = gTestDir.clone();
  bookmarksJSONFile.append(aFilename);
  do_check_true(bookmarksJSONFile.exists());
  bookmarksJSONFile.copyTo(bookmarksBackupDir, FILENAME_BOOKMARKS_JSON);
  let profileBookmarksJSONFile = bookmarksBackupDir.clone();
  profileBookmarksJSONFile.append(FILENAME_BOOKMARKS_JSON);
  do_check_true(profileBookmarksJSONFile.exists());
  return profileBookmarksJSONFile;
}


/**
 * Remove bookmarksbackup dir and all backups from the profile folder.
 */
function remove_all_JSON_backups() {
  let bookmarksBackupDir = gProfD.clone();
  bookmarksBackupDir.append("bookmarkbackups");
  if (bookmarksBackupDir.exists()) {
    bookmarksBackupDir.remove(true);
    do_check_false(bookmarksBackupDir.exists());
  }
}


/**
 * Check a JSON backup file for today exists in the profile folder.
 *
 * @return nsIFile object for the file.
 */
function check_JSON_backup() {
  let profileBookmarksJSONFile = gProfD.clone();
  profileBookmarksJSONFile.append("bookmarkbackups");
  profileBookmarksJSONFile.append(FILENAME_BOOKMARKS_JSON);
  do_check_true(profileBookmarksJSONFile.exists());
  return profileBookmarksJSONFile;
}


/**
 * Compares two times in usecs, considering eventual platform timers skews.
 *
 * @param aTimeBefore
 *        The older time in usecs.
 * @param aTimeAfter
 *        The newer time in usecs.
 * @return true if times are ordered, false otherwise.
 */
function is_time_ordered(before, after) {
  // Windows has an estimated 16ms timers precision, since Date.now() and
  // PR_Now() use different code atm, the results can be unordered by this
  // amount of time.  See bug 558745 and bug 557406.
  let isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);
  // Just to be safe we consider 20ms.
  let skew = isWindows ? 20000000 : 0;
  return after - before > -skew;
}


// These tests are known to randomly fail due to bug 507790 when database
// flushes are active, so we turn off syncing for them.
let (randomFailingSyncTests = [
  "test_multi_word_tags.js",
  "test_removeVisitsByTimeframe.js",
  "test_utils_getURLsForContainerNode.js",
  "test_exclude_livemarks.js",
  "test_402799.js",
  "test_results-as-visit.js",
  "test_sorting.js",
  "test_redirectsMode.js",
  "test_384228.js",
  "test_395593.js",
  "test_containersQueries_sorting.js",
  "test_browserGlue_smartBookmarks.js",
  "test_browserGlue_distribution.js",
  "test_331487.js",
  "test_tags.js",
  "test_385829.js",
  "test_405938_restore_queries.js",
]) {
  let currentTestFilename = do_get_file(_TEST_FILE[0], true).leafName;
  if (randomFailingSyncTests.indexOf(currentTestFilename) != -1) {
    print("Test " + currentTestFilename +
          " is known random due to bug 507790, disabling PlacesDBFlush.");
    let sync = Cc["@mozilla.org/places/sync;1"].getService(Ci.nsIObserver);
    sync.observe(null, "places-debug-stop-sync", null);
  }
}
