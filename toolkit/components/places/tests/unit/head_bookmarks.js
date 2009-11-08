/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *  Dietrich Ayala <dietrich@mozilla.com>
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

do_get_profile();

var dirSvc = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
var provider = {
  getFile: function(prop, persistent) {
    persistent.value = true;
    if (prop == NS_APP_HISTORY_50_FILE) {
      var histFile = dirSvc.get("ProfD", Ci.nsIFile);
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

var iosvc = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

function uri(spec) {
  return iosvc.newURI(spec, null, null);
}

/*
 * Reads the data from the specified nsIFile, and returns an array of bytes.
 */
function readFileData(aFile) {
  var inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                    createInstance(Ci.nsIFileInputStream);
  // init the stream as RD_ONLY, -1 == default permissions.
  inputStream.init(aFile, 0x01, -1, null);
  var size = inputStream.available();

  // use a binary input stream to grab the bytes.
  var bis = Cc["@mozilla.org/binaryinputstream;1"].
            createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(inputStream);

  var bytes = bis.readByteArray(size);

  if (size != bytes.length)
      throw "Didn't read expected number of bytes";

  return bytes;
}

/*
 * Compares two arrays, and returns true if they are equal.
 */
function compareArrays(aArray1, aArray2) {
  if (aArray1.length != aArray2.length)
    return false;

  for (var i = 0; i < aArray1.length; i++)
    if (aArray1[i] != aArray2[i])
      return false;

  return true;
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
  let db = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsPIPlacesDatabase).
           DBConnection;
  let stmt = db.createStatement("SELECT * FROM " + aName);

  dump("\n*** Printing data from " + aName + ":\n");
  let count = 0;
  while (stmt.executeStep()) {
    let columns = stmt.numEntries;

    if (count == 0) {
      // print the column names
      for (let i = 0; i < columns; i++)
        dump(stmt.getColumnName(i) + "\t");
      dump("\n");
    }

    // print the row
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
  stmt = null;
}

/*
 * Removes all bookmarks and checks for correct cleanup
 */
function remove_all_bookmarks() {
  var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);
  // Clear all bookmarks
  bs.removeFolderChildren(bs.bookmarksMenuFolder);
  bs.removeFolderChildren(bs.toolbarFolder);
  bs.removeFolderChildren(bs.unfiledBookmarksFolder);
  // Check for correct cleanup
  check_no_bookmarks()
}

/*
 * Checks that we don't have any bookmark
 */
function check_no_bookmarks() {
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);
  var query = hs.getNewQuery();
  query.setFolders([bs.toolbarFolder, bs.bookmarksMenuFolder, bs.unfiledBookmarksFolder], 3);
  var options = hs.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 0);
  root.containerOpen = false;
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

// These tests are known to randomly fail due to bug 507790 when database
// flushes are active, so we turn off syncing for them.
let randomFailingSyncTests = [
  "test_annotations.js",
  "test_multi_word_tags.js",
  "test_removeVisitsByTimeframe.js",
  "test_tagging.js",
  "test_utils_getURLsForContainerNode.js",
];
let currentTestFilename = do_get_file(_TEST_FILE[0], true).leafName;
if (randomFailingSyncTests.indexOf(currentTestFilename) != -1) {
  print("Test " + currentTestFilename + " is known random due to bug 507790, disabling PlacesDBFlush component.");
  let sync = Cc["@mozilla.org/places/sync;1"].getService(Ci.nsIObserver);
  sync.observe(null, "places-debug-stop-sync", null);
}
