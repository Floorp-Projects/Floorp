/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var { AsyncShutdown } = ChromeUtils.import(
  "resource://gre/modules/AsyncShutdown.jsm"
);

add_task(async function test_abort_merging() {
  let buf = await openMirror("abort_merging");

  let controller = new AbortController();
  controller.abort();
  await Assert.rejects(
    buf.apply({ signal: controller.signal }),
    ex => ex.name == "InterruptedError",
    "Should abort merge when signaled"
  );

  // Even though the merger is already finalized on the Rust side, the DB
  // connection is still open on the JS side. Finalizing `buf` closes it.
  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_blocker_state() {
  let barrier = new AsyncShutdown.Barrier("Test");
  let buf = await SyncedBookmarksMirror.open({
    path: "blocker_state_buf.sqlite",
    finalizeAt: barrier.client,
    recordTelemetryEvent(...args) {},
    recordStepTelemetry(...args) {},
    recordValidationTelemetry(...args) {},
  });
  await storeRecords(buf, [
    {
      id: "menu",
      parentid: "places",
      type: "folder",
      children: ["bookmarkAAAA"],
    },
    {
      id: "bookmarkAAAA",
      parentid: "menu",
      type: "bookmark",
      title: "A",
      bmkUri: "http://example.com/a",
    },
  ]);

  await buf.tryApply(buf.finalizeController.signal);
  await barrier.wait();

  let state = buf.progress.fetchState();
  let steps = state.steps;
  deepEqual(
    steps.map(s => s.step),
    [
      "fetchLocalTree",
      "fetchRemoteTree",
      "merge",
      "apply",
      "notifyObservers",
      "fetchLocalChangeRecords",
      "finalize",
    ],
    "Should report merge progress after waiting on blocker"
  );
  ok(
    buf.finalizeController.signal.aborted,
    "Should abort finalize signal on shutdown"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});
