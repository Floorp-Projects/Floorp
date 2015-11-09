/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * What this is aimed to test:
 *
 * Session annotations should be expired when browsing session ends.
 */

var as = Cc["@mozilla.org/browser/annotation-service;1"].
         getService(Ci.nsIAnnotationService);

function run_test() {
  run_next_test();
}

add_task(function* test_annos_expire_session() {
  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  // Add some visited page and a couple session annotations for each.
  let now = Date.now() * 1000;
  for (let i = 0; i < 10; i++) {
    let pageURI = uri("http://session_page_anno." + i + ".mozilla.org/");
    yield PlacesTestUtils.addVisits({ uri: pageURI, visitDate: now++ });
    as.setPageAnnotation(pageURI, "test1", "test", 0, as.EXPIRE_SESSION);
    as.setPageAnnotation(pageURI, "test2", "test", 0, as.EXPIRE_SESSION);
  }

  // Add some bookmarked page and a couple session annotations for each.
  for (let i = 0; i < 10; i++) {
    let pageURI = uri("http://session_item_anno." + i + ".mozilla.org/");
    let bm = yield PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: pageURI,
      title: null
    });
    let id = yield PlacesUtils.promiseItemId(bm.guid);
    as.setItemAnnotation(id, "test1", "test", 0, as.EXPIRE_SESSION);
    as.setItemAnnotation(id, "test2", "test", 0, as.EXPIRE_SESSION);
  }


  let pages = as.getPagesWithAnnotation("test1");
  do_check_eq(pages.length, 10);
  pages = as.getPagesWithAnnotation("test2");
  do_check_eq(pages.length, 10);
  let items = as.getItemsWithAnnotation("test1");
  do_check_eq(items.length, 10);
  items = as.getItemsWithAnnotation("test2");
  do_check_eq(items.length, 10);

  let deferred = Promise.defer();
  waitForConnectionClosed(function() {
    let stmt = DBConn(true).createAsyncStatement(
      `SELECT id FROM moz_annos
       UNION ALL
       SELECT id FROM moz_items_annos`
    );
    stmt.executeAsync({
      handleResult: function(aResultSet) {
        dump_table("moz_annos");
        dump_table("moz_items_annos");
        do_throw("Should not find any leftover session annotations");
      },
      handleError: function(aError) {
        do_throw("Error code " + aError.result + " with message '" +
                 aError.message + "' returned.");
      },
      handleCompletion: function(aReason) {
        do_check_eq(aReason, Ci.mozIStorageStatementCallback.REASON_FINISHED);
        deferred.resolve();
      }
    });
    stmt.finalize();
  });
  yield deferred.promise;
});
