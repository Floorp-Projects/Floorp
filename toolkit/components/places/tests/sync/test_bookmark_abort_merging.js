/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_abort_merging() {
  let buf = await openMirror("abort_merging");

  let promise = new Promise((resolve, reject) => {
    let callback = {
      handleResult() {
        reject(new Error("Shouldn't have merged after aborting"));
      },
      handleError(code, message) {
        equal(code, Cr.NS_ERROR_ABORT, "Should abort merge with result code");
        resolve();
      },
    };
    // `merge` schedules a runnable to start the merge on the storage thread, on
    // the next turn of the event loop. In the same turn, before the runnable is
    // scheduled, we call `finalize`, which sets the abort controller's aborted
    // flag.
    buf.merger.merge(0, 0, [], callback);
    buf.merger.finalize();
  });

  await promise;

  // Even though the merger is already finalized on the Rust side, the DB
  // connection is still open on the JS side. Finalizing `buf` closes it.
  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
