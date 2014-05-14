/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/BookmarkHTMLUtils.jsm");

/**
 * Tests the bookmarks-restore-* nsIObserver notifications after restoring
 * bookmarks from JSON and HTML.  See bug 470314.
 */

// The topics and data passed to nsIObserver.observe() on bookmarks restore
const NSIOBSERVER_TOPIC_BEGIN    = "bookmarks-restore-begin";
const NSIOBSERVER_TOPIC_SUCCESS  = "bookmarks-restore-success";
const NSIOBSERVER_TOPIC_FAILED   = "bookmarks-restore-failed";
const NSIOBSERVER_DATA_JSON      = "json";
const NSIOBSERVER_DATA_HTML      = "html";
const NSIOBSERVER_DATA_HTML_INIT = "html-initial";

// Bookmarks are added for these URIs
var uris = [
  "http://example.com/1",
  "http://example.com/2",
  "http://example.com/3",
  "http://example.com/4",
  "http://example.com/5",
];

// Add tests here.  Each is an object with these properties:
//   desc:       description printed before test is run
//   currTopic:  the next expected topic that should be observed for the test;
//               set to NSIOBSERVER_TOPIC_BEGIN to begin
//   finalTopic: the last expected topic that should be observed for the test,
//               which then causes the next test to be run
//   data:       the data passed to nsIObserver.observe() corresponding to the
//               test
//   file:       the nsILocalFile that the test creates
//   folderId:   for HTML restore into a folder, the folder ID to restore into;
//               otherwise, set it to null
//   run:        a method that actually runs the test
var tests = [
  {
    desc:       "JSON restore: normal restore should succeed",
    currTopic:  NSIOBSERVER_TOPIC_BEGIN,
    finalTopic: NSIOBSERVER_TOPIC_SUCCESS,
    data:       NSIOBSERVER_DATA_JSON,
    folderId:   null,
    run:        function () {
      Task.spawn(function () {
        this.file = yield promiseFile("bookmarks-test_restoreNotification.json");
        addBookmarks();

        yield BookmarkJSONUtils.exportToFile(this.file);
        remove_all_bookmarks();
        try {
          yield BookmarkJSONUtils.importFromFile(this.file, true);
        }
        catch (e) {
          do_throw("  Restore should not have failed");
        }
      }.bind(this));
    }
  },

  {
    desc:       "JSON restore: empty file should succeed",
    currTopic:  NSIOBSERVER_TOPIC_BEGIN,
    finalTopic: NSIOBSERVER_TOPIC_SUCCESS,
    data:       NSIOBSERVER_DATA_JSON,
    folderId:   null,
    run:        function () {
      Task.spawn(function() {
        this.file = yield promiseFile("bookmarks-test_restoreNotification.json");
        try {
          yield BookmarkJSONUtils.importFromFile(this.file, true);
        }
        catch (e) {
          do_throw("  Restore should not have failed" + e);
        }
      }.bind(this));
    }
  },

  {
    desc:       "JSON restore: nonexistent file should fail",
    currTopic:  NSIOBSERVER_TOPIC_BEGIN,
    finalTopic: NSIOBSERVER_TOPIC_FAILED,
    data:       NSIOBSERVER_DATA_JSON,
    folderId:   null,
    run:        function () {
      this.file = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
      this.file.append("this file doesn't exist because nobody created it 1");
      Task.spawn(function() {
        try {
          yield BookmarkJSONUtils.importFromFile(this.file, true);
          do_throw("  Restore should have failed");
        }
        catch (e) {
        }
      }.bind(this));
    }
  },

  {
    desc:       "HTML restore: normal restore should succeed",
    currTopic:  NSIOBSERVER_TOPIC_BEGIN,
    finalTopic: NSIOBSERVER_TOPIC_SUCCESS,
    data:       NSIOBSERVER_DATA_HTML,
    folderId:   null,
    run:        function () {
      Task.spawn(function() {
        this.file = yield promiseFile("bookmarks-test_restoreNotification.html");
        addBookmarks();
        yield BookmarkHTMLUtils.exportToFile(this.file);
        remove_all_bookmarks();
        try {
          BookmarkHTMLUtils.importFromFile(this.file, false)
                           .then(null, do_report_unexpected_exception);
        }
        catch (e) {
          do_throw("  Restore should not have failed");
        }
      }.bind(this));
    }
  },

  {
    desc:       "HTML restore: empty file should succeed",
    currTopic:  NSIOBSERVER_TOPIC_BEGIN,
    finalTopic: NSIOBSERVER_TOPIC_SUCCESS,
    data:       NSIOBSERVER_DATA_HTML,
    folderId:   null,
    run:        function () {
      Task.spawn(function (){
        this.file = yield promiseFile("bookmarks-test_restoreNotification.init.html");
        try {
          BookmarkHTMLUtils.importFromFile(this.file, false)
                           .then(null, do_report_unexpected_exception);
        }
        catch (e) {
          do_throw("  Restore should not have failed");
        }
      }.bind(this));
    }
  },

  {
    desc:       "HTML restore: nonexistent file should fail",
    currTopic:  NSIOBSERVER_TOPIC_BEGIN,
    finalTopic: NSIOBSERVER_TOPIC_FAILED,
    data:       NSIOBSERVER_DATA_HTML,
    folderId:   null,
    run:        Task.async(function* () {
      this.file = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
      this.file.append("this file doesn't exist because nobody created it 2");
      try {
        yield BookmarkHTMLUtils.importFromFile(this.file, false);
        do_throw("Should fail!");
      }
      catch (e) {}
    }.bind(this))
  },

  {
    desc:       "HTML initial restore: normal restore should succeed",
    currTopic:  NSIOBSERVER_TOPIC_BEGIN,
    finalTopic: NSIOBSERVER_TOPIC_SUCCESS,
    data:       NSIOBSERVER_DATA_HTML_INIT,
    folderId:   null,
    run:        function () {
      Task.spawn(function () {
        this.file = yield promiseFile("bookmarks-test_restoreNotification.init.html");
        addBookmarks();
        yield BookmarkHTMLUtils.exportToFile(this.file);
        remove_all_bookmarks();
        try {
          BookmarkHTMLUtils.importFromFile(this.file, true)
                           .then(null, do_report_unexpected_exception);
        }
        catch (e) {
          do_throw("  Restore should not have failed");
        }
      }.bind(this));
    }
  },

  {
    desc:       "HTML initial restore: empty file should succeed",
    currTopic:  NSIOBSERVER_TOPIC_BEGIN,
    finalTopic: NSIOBSERVER_TOPIC_SUCCESS,
    data:       NSIOBSERVER_DATA_HTML_INIT,
    folderId:   null,
    run:        function () {
      Task.spawn(function () {
        this.file = yield promiseFile("bookmarks-test_restoreNotification.init.html");
        try {
          BookmarkHTMLUtils.importFromFile(this.file, true)
                           .then(null, do_report_unexpected_exception);
        }
        catch (e) {
          do_throw("  Restore should not have failed");
        }
      }.bind(this));
    }
  },

  {
    desc:       "HTML initial restore: nonexistent file should fail",
    currTopic:  NSIOBSERVER_TOPIC_BEGIN,
    finalTopic: NSIOBSERVER_TOPIC_FAILED,
    data:       NSIOBSERVER_DATA_HTML_INIT,
    folderId:   null,
    run:        Task.async(function* () {
      this.file = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
      this.file.append("this file doesn't exist because nobody created it 3");
      try {
        yield BookmarkHTMLUtils.importFromFile(this.file, true);
        do_throw("Should fail!");
      }
      catch (e) {}
    }.bind(this))
  }
];

// nsIObserver that observes bookmarks-restore-begin.
var beginObserver = {
  observe: function _beginObserver(aSubject, aTopic, aData) {
    var test = tests[currTestIndex];

    print("  Observed " + aTopic);
    print("  Topic for current test should be what is expected");
    do_check_eq(aTopic, test.currTopic);

    print("  Data for current test should be what is expected");
    do_check_eq(aData, test.data);

    // Update current expected topic to the next expected one.
    test.currTopic = test.finalTopic;
  }
};

// nsIObserver that observes bookmarks-restore-success/failed.  This starts
// the next test.
var successAndFailedObserver = {
  observe: function _successAndFailedObserver(aSubject, aTopic, aData) {
    var test = tests[currTestIndex];

    print("  Observed " + aTopic);
    print("  Topic for current test should be what is expected");
    do_check_eq(aTopic, test.currTopic);

    print("  Data for current test should be what is expected");
    do_check_eq(aData, test.data);

    // On restore failed, file may not exist, so wrap in try-catch.
    try {
      test.file.remove(false);
    }
    catch (exc) {}

    // Make sure folder ID is what is expected.  For importing HTML into a
    // folder, this will be an integer, otherwise null.
    if (aSubject) {
      do_check_eq(aSubject.QueryInterface(Ci.nsISupportsPRInt64).data,
                  test.folderId);
    }
    else
      do_check_eq(test.folderId, null);

    remove_all_bookmarks();
    do_execute_soon(doNextTest);
  }
};

// Index of the currently running test.  See doNextTest().
var currTestIndex = -1;

var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
            getService(Ci.nsINavBookmarksService);

var obssvc = Cc["@mozilla.org/observer-service;1"].
             getService(Ci.nsIObserverService);

///////////////////////////////////////////////////////////////////////////////

/**
 * Adds some bookmarks for the URIs in |uris|.
 */
function addBookmarks() {
  uris.forEach(function (u) bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder,
                                                 uri(u),
                                                 bmsvc.DEFAULT_INDEX,
                                                 u));
  checkBookmarksExist();
}

/**
 * Checks that all of the bookmarks created for |uris| exist.  It works by
 * creating one query per URI and then ORing all the queries.  The number of
 * results returned should be uris.length.
 */
function checkBookmarksExist() {
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  var queries = uris.map(function (u) {
    var q = hs.getNewQuery();
    q.uri = uri(u);
    return q;
  });
  var options = hs.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  var root = hs.executeQueries(queries, uris.length, options).root;
  root.containerOpen = true;
  do_check_eq(root.childCount, uris.length);
  root.containerOpen = false;
}

/**
 * Creates an file in the profile directory.
 *
 * @param  aBasename
 *         e.g., "foo.txt" in the path /some/long/path/foo.txt
 * @return {Promise}
 * @resolves to an OS.File path
 */
function promiseFile(aBasename) {
  let path = OS.Path.join(OS.Constants.Path.profileDir, aBasename);
  dump("\n\nopening " + path + "\n\n");
  return OS.File.open(path, { truncate: true }).then(aFile => { aFile.close(); return path; });
}

/**
 * Runs the next test or if all tests have been run, finishes.
 */
function doNextTest() {
  currTestIndex++;
  if (currTestIndex >= tests.length) {
    obssvc.removeObserver(beginObserver, NSIOBSERVER_TOPIC_BEGIN);
    obssvc.removeObserver(successAndFailedObserver, NSIOBSERVER_TOPIC_SUCCESS);
    obssvc.removeObserver(successAndFailedObserver, NSIOBSERVER_TOPIC_FAILED);
    do_test_finished();
  }
  else {
    var test = tests[currTestIndex];
    print("Running test: " + test.desc);
    test.run();
  }
}

///////////////////////////////////////////////////////////////////////////////

function run_test() {
  do_test_pending();
  obssvc.addObserver(beginObserver, NSIOBSERVER_TOPIC_BEGIN, false);
  obssvc.addObserver(successAndFailedObserver, NSIOBSERVER_TOPIC_SUCCESS, false);
  obssvc.addObserver(successAndFailedObserver, NSIOBSERVER_TOPIC_FAILED, false);
  doNextTest();
}
