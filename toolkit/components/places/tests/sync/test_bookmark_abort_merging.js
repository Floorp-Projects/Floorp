/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_abort_merging() {
  let buf = await openMirror("abort_merging");

  let promise = new Promise((resolve, reject) => {
    buf.merger.finalize();
    let callback = {
      handleResult() {
        reject(new Error("Shouldn't have merged after aborting"));
      },
      handleError(code, message) {
        equal(code, Cr.NS_ERROR_ABORT, "Should abort merge with result code");
        resolve();
      },
    };
    buf.merger.merge(0, 0, [], callback);
  });

  await promise;

  // Even though the merger is already finalized on the Rust side, the DB
  // connection is still open on the JS side. Finalizing `buf` closes it.
  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
