/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/NewTabUtils.jsm");
Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");
Cu.import("resource://gre/modules/Services.jsm");

/**
 * Asynchronously load test data from chromeappstore.sqlite.
 *
 * @param aDBFile
 *        the database file to load
 * @return {Promise} resolved when the load is complete
  */
function promiseLoadChromeAppsStore(aDBFile) {
  let deferred = Promise.defer();

  let pinnedLinks = [];
  let blockedLinks = [];

  let db = Services.storage.openUnsharedDatabase(aDBFile);
  let stmt = db.createAsyncStatement(
    "SELECT key, value FROM webappsstore2 WHERE scope = 'batwen.:about'");
  try {
    stmt.executeAsync({
      handleResult: function(aResultSet) {
        for (let row = aResultSet.getNextRow(); row;
             row = aResultSet.getNextRow()) {
          let value = JSON.parse(row.getResultByName("value"));
          if (row.getResultByName("key") == "pinnedLinks") {
            pinnedLinks = value;
          } else {
            for (let url of Object.keys(value)) {
              blockedLinks.push({ url: url, title: "" });
            }
          }
        }
      },
      handleError: function(aError) {
        deferred.reject(new Components.Exception("Error", Cr.NS_ERROR_FAILURE));
      },
      handleCompletion: function(aReason) {
        if (aReason === Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          deferred.resolve([pinnedLinks, blockedLinks]);
        }
      }
    });
  } finally {
    stmt.finalize();
    db.asyncClose();
  }

  return deferred.promise;
}

function run_test() {
  do_test_pending();

  // First of all copy the chromeappsstore.sqlite file to the profile folder.
  let dbFile = do_get_file("chromeappsstore.sqlite");
  let profileDBFile = do_get_profile();
  dbFile.copyTo(profileDBFile, "chromeappsstore.sqlite");
  profileDBFile.append("chromeappsstore.sqlite");
  do_check_true(profileDBFile.exists());

  // Load test data from the database.
  promiseLoadChromeAppsStore(dbFile).then(function success(aResults) {
    let [pinnedLinks, blockedLinks] = aResults;

    do_check_true(pinnedLinks.length > 0);
    do_check_eq(pinnedLinks.length, NewTabUtils.pinnedLinks.links.length);
    do_check_true(pinnedLinks.every(
      function(aLink) NewTabUtils.pinnedLinks.isPinned(aLink)
    ));

    do_check_true(blockedLinks.length > 0);
    do_check_eq(blockedLinks.length,
                Object.keys(NewTabUtils.blockedLinks.links).length);
    do_check_true(blockedLinks.every(
      function(aLink) NewTabUtils.blockedLinks.isBlocked(aLink)
    ));

    try {
      profileDBFile.remove(true);
    } catch (ex) {
      // May fail due to OS file locking, not a blocking error though.
      do_print("Unable to remove chromeappsstore.sqlite file.");
    }

    do_test_finished();
  }, do_report_unexpected_exception);
}
