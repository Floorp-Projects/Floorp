/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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

///////////////////////////////////////////////////////////////////////////////

/**
 * Adds some bookmarks for the URIs in |uris|.
 */
function* addBookmarks() {
  for (let url of uris) {
    yield PlacesUtils.bookmarks.insert({
      url: url, parentGuid: PlacesUtils.bookmarks.menuGuid
    })
  }
  checkBookmarksExist();
}

/**
 * Checks that all of the bookmarks created for |uris| exist.  It works by
 * creating one query per URI and then ORing all the queries.  The number of
 * results returned should be uris.length.
 */
function checkBookmarksExist() {
  let hs = PlacesUtils.history;
  let queries = uris.map(function (u) {
    let q = hs.getNewQuery();
    q.uri = uri(u);
    return q;
  });
  let options = hs.getNewQueryOptions();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let root = hs.executeQueries(queries, uris.length, options).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, uris.length);
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
  do_print("opening " + path);
  return OS.File.open(path, { truncate: true })
                .then(aFile => {
                  aFile.close();
                  return path;
                });
}

/**
 * Register observers via promiseTopicObserved helper.
 *
 * @param  {boolean} expectSuccess pass true when expect a success notification
 * @return {Promise[]}
 */
function registerObservers(expectSuccess) {
  let promiseBegin = promiseTopicObserved(NSIOBSERVER_TOPIC_BEGIN);
  let promiseResult;
  if (expectSuccess) {
    promiseResult = promiseTopicObserved(NSIOBSERVER_TOPIC_SUCCESS);
  } else {
    promiseResult = promiseTopicObserved(NSIOBSERVER_TOPIC_FAILED);
  }

  return [promiseBegin, promiseResult];
}

/**
 * Check notification results.
 *
 * @param  {Promise[]} expectPromises array contain promiseBegin and promiseResult
 * @param  {object} expectedData contain data and folderId
 */
function* checkObservers(expectPromises, expectedData) {
  let [promiseBegin, promiseResult] = expectPromises;

  let beginData = (yield promiseBegin)[1];
  Assert.equal(beginData, expectedData.data,
    "Data for current test should be what is expected");

  let [resultSubject, resultData] = yield promiseResult;
  Assert.equal(resultData, expectedData.data,
    "Data for current test should be what is expected");

  // Make sure folder ID is what is expected.  For importing HTML into a
  // folder, this will be an integer, otherwise null.
  if (resultSubject) {
    Assert.equal(aSubject.QueryInterface(Ci.nsISupportsPRInt64).data,
                expectedData.folderId);
  } else {
    Assert.equal(expectedData.folderId, null);
  }
}

/**
 * Run after every test cases.
 */
function* teardown(file, begin, success, fail) {
  // On restore failed, file may not exist, so wrap in try-catch.
  try {
    yield OS.File.remove(file, {ignoreAbsent: true});
  } catch (e) {}

  // clean up bookmarks
  yield PlacesUtils.bookmarks.eraseEverything();
}

///////////////////////////////////////////////////////////////////////////////
add_task(function* test_json_restore_normal() {
  // data: the data passed to nsIObserver.observe() corresponding to the test
  // folderId: for HTML restore into a folder, the folder ID to restore into;
  //           otherwise, set it to null
  let expectedData = {
    data:       NSIOBSERVER_DATA_JSON,
    folderId:   null
  }
  let expectPromises = registerObservers(true);

  do_print("JSON restore: normal restore should succeed");
  let file = yield promiseFile("bookmarks-test_restoreNotification.json");
  yield addBookmarks();

  yield BookmarkJSONUtils.exportToFile(file);
  yield PlacesUtils.bookmarks.eraseEverything();
  try {
    yield BookmarkJSONUtils.importFromFile(file, true);
  } catch (e) {
    do_throw("  Restore should not have failed" + e);
  }

  yield checkObservers(expectPromises, expectedData);
  yield teardown(file);
});

add_task(function* test_json_restore_empty() {
  let expectedData = {
    data:       NSIOBSERVER_DATA_JSON,
    folderId:   null
  }
  let expectPromises = registerObservers(true);

  do_print("JSON restore: empty file should succeed");
  let file = yield promiseFile("bookmarks-test_restoreNotification.json");
  try {
    yield BookmarkJSONUtils.importFromFile(file, true);
  } catch (e) {
    do_throw("  Restore should not have failed" + e);
  }

  yield checkObservers(expectPromises, expectedData);
  yield teardown(file);
});

add_task(function* test_json_restore_nonexist() {
  let expectedData = {
    data:       NSIOBSERVER_DATA_JSON,
    folderId:   null
  }
  let expectPromises = registerObservers(false);

  do_print("JSON restore: nonexistent file should fail");
  let file = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  file.append("this file doesn't exist because nobody created it 1");
  try {
    yield BookmarkJSONUtils.importFromFile(file, true);
    do_throw("  Restore should have failed");
  } catch (e) {}

  yield checkObservers(expectPromises, expectedData);
  yield teardown(file);
});

add_task(function* test_html_restore_normal() {
  let expectedData = {
    data:       NSIOBSERVER_DATA_HTML,
    folderId:   null
  }
  let expectPromises = registerObservers(true);

  do_print("HTML restore: normal restore should succeed");
  let file = yield promiseFile("bookmarks-test_restoreNotification.html");
  yield addBookmarks();
  yield BookmarkHTMLUtils.exportToFile(file);
  yield PlacesUtils.bookmarks.eraseEverything();
  try {
    BookmarkHTMLUtils.importFromFile(file, false)
                     .then(null, do_report_unexpected_exception);
  } catch (e) {
    do_throw("  Restore should not have failed");
  }

  yield checkObservers(expectPromises, expectedData);
  yield teardown(file);
});

add_task(function* test_html_restore_empty() {
  let expectedData = {
    data:       NSIOBSERVER_DATA_HTML,
    folderId:   null
  }
  let expectPromises = registerObservers(true);

  do_print("HTML restore: empty file should succeed");
  let file = yield promiseFile("bookmarks-test_restoreNotification.init.html");
  try {
    BookmarkHTMLUtils.importFromFile(file, false)
                     .then(null, do_report_unexpected_exception);
  } catch (e) {
    do_throw("  Restore should not have failed");
  }

  yield checkObservers(expectPromises, expectedData);
  yield teardown(file);
});

add_task(function* test_html_restore_nonexist() {
  let expectedData = {
    data:       NSIOBSERVER_DATA_HTML,
    folderId:   null
  }
  let expectPromises = registerObservers(false);

  do_print("HTML restore: nonexistent file should fail");
  let file = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  file.append("this file doesn't exist because nobody created it 2");
  try {
    yield BookmarkHTMLUtils.importFromFile(file, false);
    do_throw("Should fail!");
  } catch (e) {}

  yield checkObservers(expectPromises, expectedData);
  yield teardown(file);
});

add_task(function* test_html_init_restore_normal() {
  let expectedData = {
    data:       NSIOBSERVER_DATA_HTML_INIT,
    folderId:   null
  }
  let expectPromises = registerObservers(true);

  do_print("HTML initial restore: normal restore should succeed");
  let file = yield promiseFile("bookmarks-test_restoreNotification.init.html");
  yield addBookmarks();
  yield BookmarkHTMLUtils.exportToFile(file);
  yield PlacesUtils.bookmarks.eraseEverything();
  try {
    BookmarkHTMLUtils.importFromFile(file, true)
                     .then(null, do_report_unexpected_exception);
  } catch (e) {
    do_throw("  Restore should not have failed");
  }

  yield checkObservers(expectPromises, expectedData);
  yield teardown(file);
});

add_task(function* test_html_init_restore_empty() {
  let expectedData = {
    data:       NSIOBSERVER_DATA_HTML_INIT,
    folderId:   null
  }
  let expectPromises = registerObservers(true);

  do_print("HTML initial restore: empty file should succeed");
  let file = yield promiseFile("bookmarks-test_restoreNotification.init.html");
  try {
    BookmarkHTMLUtils.importFromFile(file, true)
                     .then(null, do_report_unexpected_exception);
  } catch (e) {
    do_throw("  Restore should not have failed");
  }

  yield checkObservers(expectPromises, expectedData);
  yield teardown(file);
});

add_task(function* test_html_init_restore_nonexist() {
  let expectedData = {
    data:       NSIOBSERVER_DATA_HTML_INIT,
    folderId:   null
  }
  let expectPromises = registerObservers(false);

  do_print("HTML initial restore: nonexistent file should fail");
  let file = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  file.append("this file doesn't exist because nobody created it 3");
  try {
    yield BookmarkHTMLUtils.importFromFile(file, true);
    do_throw("Should fail!");
  } catch (e) {}

  yield checkObservers(expectPromises, expectedData);
  yield teardown(file);
});
