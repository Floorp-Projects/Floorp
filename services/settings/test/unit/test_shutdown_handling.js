/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const { Database } = ChromeUtils.import(
  "resource://services-settings/Database.jsm"
);
const { RemoteSettingsWorker } = ChromeUtils.import(
  "resource://services-settings/RemoteSettingsWorker.jsm"
);
const { RemoteSettingsClient } = ChromeUtils.import(
  "resource://services-settings/RemoteSettingsClient.jsm"
);

add_task(async function test_shutdown_abort_after_start() {
  // Start a forever transaction:
  let counter = 0;
  let transactionStarted;
  let startedPromise = new Promise(r => {
    transactionStarted = r;
  });
  let promise = Database._executeIDB(
    "records",
    store => {
      // Signal we've started.
      transactionStarted();
      function makeRequest() {
        if (++counter > 1000) {
          Assert.ok(
            false,
            "We ran 1000 requests and didn't get aborted, what?"
          );
          return;
        }
        dump("Making request " + counter + "\n");
        const request = store
          .index("cid")
          .openCursor(IDBKeyRange.only("foopydoo/foo"));
        request.onsuccess = event => {
          makeRequest();
        };
      }
      makeRequest();
    },
    { mode: "readonly" }
  );

  // Wait for the transaction to start.
  await startedPromise;

  Database._shutdownHandler(); // should abort the readonly transaction.

  let rejection;
  await promise.catch(e => {
    rejection = e;
  });
  ok(rejection, "Promise should have rejected.");

  // Now clear the shutdown flag and rejection error:
  Database._cancelShutdown();
  rejection = null;
});

add_task(async function test_shutdown_immediate_abort() {
  // Now abort directly from the successful request.
  let promise = Database._executeIDB(
    "records",
    store => {
      let request = store
        .index("cid")
        .openCursor(IDBKeyRange.only("foopydoo/foo"));
      request.onsuccess = event => {
        // Abort immediately.
        Database._shutdownHandler();
        request = store
          .index("cid")
          .openCursor(IDBKeyRange.only("foopydoo/foo"));
        Assert.ok(false, "IndexedDB allowed opening a cursor after aborting?!");
      };
    },
    { mode: "readonly" }
  );

  let rejection;
  // Wait for the abort
  await promise.catch(e => {
    rejection = e;
  });
  ok(rejection, "Directly aborted promise should also have rejected.");
  // Now clear the shutdown flag and rejection error:
  Database._cancelShutdown();
});

add_task(async function test_shutdown_worker() {
  let client = new RemoteSettingsClient("language-dictionaries", {
    bucketNamePref: "services.settings.default_bucket",
  });
  const before = await client.get({ syncIfEmpty: false });
  Assert.equal(before.length, 0);

  let records = [{}];
  let importPromise = RemoteSettingsWorker._execute(
    "_test_only_import",
    ["main", "language-dictionaries", records],
    { mustComplete: true }
  );
  let stringifyPromise = RemoteSettingsWorker.canonicalStringify(
    [],
    [],
    Date.now()
  );
  // Change the idle time so we shut the worker down even though we can't
  // set gShutdown from outside of the worker management code.
  Services.prefs.setIntPref(
    "services.settings.worker_idle_max_milliseconds",
    1
  );
  RemoteSettingsWorker._abortCancelableRequests();
  await Assert.rejects(
    stringifyPromise,
    /Shutdown/,
    "Should have aborted the stringify request at shutdown."
  );
  await Assert.rejects(
    importPromise,
    /shutting down/,
    "Ensure imports get aborted during shutdown"
  );
  const after = await client.get({ syncIfEmpty: false });
  Assert.equal(after.length, 0);
  await TestUtils.waitForCondition(() => !RemoteSettingsWorker.worker);
  Assert.ok(
    !RemoteSettingsWorker.worker,
    "Worker should have been terminated."
  );
});
