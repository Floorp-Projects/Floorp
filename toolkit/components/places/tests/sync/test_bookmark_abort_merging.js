/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var { AsyncShutdown } = ChromeUtils.import(
  "resource://gre/modules/AsyncShutdown.jsm"
);

add_task(async function test_transaction_in_progress() {
  let buf = await openMirror("transaction_in_progress");

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

  // This transaction should block merging until the transaction is committed.
  info("Open transaction on Places connection");
  await buf.db.execute("BEGIN EXCLUSIVE");

  await Assert.rejects(
    buf.apply(),
    ex => ex.name == "MergeConflictError",
    "Should not merge when a transaction is in progress"
  );

  info("Commit open transaction");
  await buf.db.execute("COMMIT");

  info("Merging should succeed after committing");
  await buf.apply();

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

add_task(async function test_abort_store() {
  let buf = await openMirror("abort_store");

  let controller = new AbortController();
  controller.abort();
  await Assert.rejects(
    storeRecords(
      buf,
      [
        {
          id: "menu",
          parentid: "places",
          type: "folder",
          children: [],
        },
      ],
      { signal: controller.signal }
    ),
    ex => ex.name == "InterruptedError",
    "Should abort storing when signaled"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

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
  let names = [];
  for (let s of state.steps) {
    equal(typeof s.at, "number", `Should report timestamp for ${s.step}`);
    switch (s.step) {
      case "fetchLocalTree":
        greaterOrEqual(
          s.took,
          0,
          "Should report time taken to fetch local tree"
        );
        deepEqual(
          s.counts,
          [
            { name: "items", count: 6 },
            { name: "deletions", count: 0 },
          ],
          "Should report number of items in local tree"
        );
        break;

      case "fetchRemoteTree":
        greaterOrEqual(
          s.took,
          0,
          "Should report time taken to fetch remote tree"
        );
        deepEqual(
          s.counts,
          [
            { name: "items", count: 6 },
            { name: "deletions", count: 0 },
          ],
          "Should report number of items in remote tree"
        );
        break;

      case "merge":
        greaterOrEqual(s.took, 0, "Should report time taken to merge");
        deepEqual(
          s.counts,
          [{ name: "items", count: 6 }],
          "Should report merge stats"
        );
        break;

      case "apply":
        greaterOrEqual(s.took, 0, "Should report time taken to apply");
        ok(!("counts" in s), "Should not report counts for applying");
        break;

      case "notifyObservers":
        greaterOrEqual(
          s.took,
          0,
          "Should report time taken to notify observers"
        );
        ok(!("counts" in s), "Should not report counts for observers");
        break;

      case "fetchLocalChangeRecords":
        greaterOrEqual(
          s.took,
          0,
          "Should report time taken to fetch records for upload"
        );
        deepEqual(
          s.counts,
          [{ name: "items", count: 4 }],
          "Should report number of records to upload"
        );
        break;

      case "finalize":
        ok(!("took" in s), "Should not report time taken to finalize");
        ok(!("counts" in s), "Should not report counts for finalizing");
    }
    names.push(s.step);
  }
  deepEqual(
    names,
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
