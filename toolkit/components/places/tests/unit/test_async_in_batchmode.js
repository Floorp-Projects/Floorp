// This is testing the frankenstein situation Sync forces Places into.
// Sync does runInBatchMode() and before the callback returns the Places async
// APIs are used (either by Sync itself, or by any other code in the system)
// As seen in bug 1197856 and bug 1190131.

Cu.import("resource://gre/modules/PlacesUtils.jsm");

// This function "waits" for a promise to resolve by spinning a nested event
// loop.
function waitForPromise(promise) {
  let thread = Cc["@mozilla.org/thread-manager;1"].getService().currentThread;

  let finalResult, finalException;

  promise.then(result => {
    finalResult = result;
  }, err => {
    finalException = err;
  });

  // Keep waiting until our callback is triggered (unless the app is quitting).
  while (!finalResult && !finalException) {
    thread.processNextEvent(true);
  }
  if (finalException) {
    throw finalException;
  }
  return finalResult;
}

add_test(function() {
  let testCompleted = false;
  PlacesUtils.bookmarks.runInBatchMode({
    runBatched() {
      // create a bookmark.
      let info = { parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                   type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                   url: "http://example.com/" };
      let insertPromise = PlacesUtils.bookmarks.insert(info);
      let bookmark = waitForPromise(insertPromise);
      // Check we got a bookmark (bookmark creation failed completely in
      // bug 1190131)
      equal(bookmark.url, info.url);
      // Check the promiseItemGuid and promiseItemId helpers - failure in these
      // was the underlying reason for the failure.
      let id = waitForPromise(PlacesUtils.promiseItemId(bookmark.guid));
      let guid = waitForPromise(PlacesUtils.promiseItemGuid(id));
      equal(guid, bookmark.guid, "id and guid round-tripped correctly");
      testCompleted = true;
    }
  }, null);
  // make sure we tested what we think we tested.
  ok(testCompleted);
  run_next_test();
});
