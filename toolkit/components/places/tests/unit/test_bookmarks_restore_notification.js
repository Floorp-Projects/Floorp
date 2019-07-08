/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { BookmarkHTMLUtils } = ChromeUtils.import(
  "resource://gre/modules/BookmarkHTMLUtils.jsm"
);

/**
 * Tests the bookmarks-restore-* nsIObserver notifications after restoring
 * bookmarks from JSON and HTML.  See bug 470314.
 */

// The topics and data passed to nsIObserver.observe() on bookmarks restore
const NSIOBSERVER_TOPIC_BEGIN = "bookmarks-restore-begin";
const NSIOBSERVER_TOPIC_SUCCESS = "bookmarks-restore-success";
const NSIOBSERVER_TOPIC_FAILED = "bookmarks-restore-failed";
const NSIOBSERVER_DATA_JSON = "json";
const NSIOBSERVER_DATA_HTML = "html";
const NSIOBSERVER_DATA_HTML_INIT = "html-initial";

// Bookmarks are added for these URIs
var uris = [
  "http://example.com/1",
  "http://example.com/2",
  "http://example.com/3",
  "http://example.com/4",
  "http://example.com/5",
];

/**
 * Adds some bookmarks for the URIs in |uris|.
 */
async function addBookmarks() {
  for (let url of uris) {
    await PlacesUtils.bookmarks.insert({
      url,
      parentGuid: PlacesUtils.bookmarks.menuGuid,
    });
    Assert.ok(await PlacesUtils.bookmarks.fetch({ url }), "Url is bookmarked");
  }
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
  info("opening " + path);
  return OS.File.open(path, { truncate: true }).then(aFile => {
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
async function checkObservers(expectPromises, expectedData) {
  let [promiseBegin, promiseResult] = expectPromises;

  let beginData = (await promiseBegin)[1];
  Assert.equal(
    beginData,
    expectedData.data,
    "Data for current test should be what is expected"
  );

  let [resultSubject, resultData] = await promiseResult;
  Assert.equal(
    resultData,
    expectedData.data,
    "Data for current test should be what is expected"
  );

  // Make sure folder ID is what is expected.  For importing HTML into a
  // folder, this will be an integer, otherwise null.
  if (resultSubject) {
    Assert.equal(
      resultSubject.QueryInterface(Ci.nsISupportsPRInt64).data,
      expectedData.folderId
    );
  } else {
    Assert.equal(expectedData.folderId, null);
  }
}

/**
 * Run after every test cases.
 */
async function teardown(file, begin, success, fail) {
  // On restore failed, file may not exist, so wrap in try-catch.
  try {
    await OS.File.remove(file, { ignoreAbsent: true });
  } catch (e) {}

  // clean up bookmarks
  await PlacesUtils.bookmarks.eraseEverything();
}

add_task(async function test_json_restore_normal() {
  // data: the data passed to nsIObserver.observe() corresponding to the test
  // folderId: for HTML restore into a folder, the folder ID to restore into;
  //           otherwise, set it to null
  let expectedData = {
    data: NSIOBSERVER_DATA_JSON,
    folderId: null,
  };
  let expectPromises = registerObservers(true);

  info("JSON restore: normal restore should succeed");
  let file = await promiseFile("bookmarks-test_restoreNotification.json");
  await addBookmarks();

  await BookmarkJSONUtils.exportToFile(file);
  await PlacesUtils.bookmarks.eraseEverything();
  try {
    await BookmarkJSONUtils.importFromFile(file, { replace: true });
  } catch (e) {
    do_throw("  Restore should not have failed " + e);
  }

  await checkObservers(expectPromises, expectedData);
  await teardown(file);
});

add_task(async function test_json_restore_empty() {
  let expectedData = {
    data: NSIOBSERVER_DATA_JSON,
    folderId: null,
  };
  let expectPromises = registerObservers(false);

  info("JSON restore: empty file should fail");
  let file = await promiseFile("bookmarks-test_restoreNotification.json");
  await Assert.rejects(
    BookmarkJSONUtils.importFromFile(file, { replace: true }),
    /SyntaxError/,
    "Restore should reject for an empty file."
  );

  await checkObservers(expectPromises, expectedData);
  await teardown(file);
});

add_task(async function test_json_restore_nonexist() {
  let expectedData = {
    data: NSIOBSERVER_DATA_JSON,
    folderId: null,
  };
  let expectPromises = registerObservers(false);

  info("JSON restore: nonexistent file should fail");
  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append("this file doesn't exist because nobody created it 1");
  await Assert.rejects(
    BookmarkJSONUtils.importFromFile(file.path, { replace: true }),
    /Cannot restore from nonexisting json file/,
    "Restore should reject for a non-existent file."
  );

  await checkObservers(expectPromises, expectedData);
  await teardown(file);
});

add_task(async function test_html_restore_normal() {
  let expectedData = {
    data: NSIOBSERVER_DATA_HTML,
    folderId: null,
  };
  let expectPromises = registerObservers(true);

  info("HTML restore: normal restore should succeed");
  let file = await promiseFile("bookmarks-test_restoreNotification.html");
  await addBookmarks();
  await BookmarkHTMLUtils.exportToFile(file);
  await PlacesUtils.bookmarks.eraseEverything();
  try {
    BookmarkHTMLUtils.importFromFile(file).catch(
      do_report_unexpected_exception
    );
  } catch (e) {
    do_throw("  Restore should not have failed");
  }

  await checkObservers(expectPromises, expectedData);
  await teardown(file);
});

add_task(async function test_html_restore_empty() {
  let expectedData = {
    data: NSIOBSERVER_DATA_HTML,
    folderId: null,
  };
  let expectPromises = registerObservers(true);

  info("HTML restore: empty file should succeed");
  let file = await promiseFile("bookmarks-test_restoreNotification.init.html");
  try {
    BookmarkHTMLUtils.importFromFile(file).catch(
      do_report_unexpected_exception
    );
  } catch (e) {
    do_throw("  Restore should not have failed");
  }

  await checkObservers(expectPromises, expectedData);
  await teardown(file);
});

add_task(async function test_html_restore_nonexist() {
  let expectedData = {
    data: NSIOBSERVER_DATA_HTML,
    folderId: null,
  };
  let expectPromises = registerObservers(false);

  info("HTML restore: nonexistent file should fail");
  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append("this file doesn't exist because nobody created it 2");
  await Assert.rejects(
    BookmarkHTMLUtils.importFromFile(file.path),
    /Cannot import from nonexisting html file/,
    "Restore should reject for a non-existent file."
  );

  await checkObservers(expectPromises, expectedData);
  await teardown(file);
});

add_task(async function test_html_init_restore_normal() {
  let expectedData = {
    data: NSIOBSERVER_DATA_HTML_INIT,
    folderId: null,
  };
  let expectPromises = registerObservers(true);

  info("HTML initial restore: normal restore should succeed");
  let file = await promiseFile("bookmarks-test_restoreNotification.init.html");
  await addBookmarks();
  await BookmarkHTMLUtils.exportToFile(file);
  await PlacesUtils.bookmarks.eraseEverything();
  try {
    BookmarkHTMLUtils.importFromFile(file, { replace: true }).catch(
      do_report_unexpected_exception
    );
  } catch (e) {
    do_throw("  Restore should not have failed");
  }

  await checkObservers(expectPromises, expectedData);
  await teardown(file);
});

add_task(async function test_html_init_restore_empty() {
  let expectedData = {
    data: NSIOBSERVER_DATA_HTML_INIT,
    folderId: null,
  };
  let expectPromises = registerObservers(true);

  info("HTML initial restore: empty file should succeed");
  let file = await promiseFile("bookmarks-test_restoreNotification.init.html");
  try {
    BookmarkHTMLUtils.importFromFile(file, { replace: true }).catch(
      do_report_unexpected_exception
    );
  } catch (e) {
    do_throw("  Restore should not have failed");
  }

  await checkObservers(expectPromises, expectedData);
  await teardown(file);
});

add_task(async function test_html_init_restore_nonexist() {
  let expectedData = {
    data: NSIOBSERVER_DATA_HTML_INIT,
    folderId: null,
  };
  let expectPromises = registerObservers(false);

  info("HTML initial restore: nonexistent file should fail");
  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append("this file doesn't exist because nobody created it 3");
  await Assert.rejects(
    BookmarkHTMLUtils.importFromFile(file.path, { replace: true }),
    /Cannot import from nonexisting html file/,
    "Restore should reject for a non-existent file."
  );

  await checkObservers(expectPromises, expectedData);
  await teardown(file);
});
