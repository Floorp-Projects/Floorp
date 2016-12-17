/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

var bsp = Cu.import("resource://gre/modules/CrashManager.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/TelemetryEnvironment.jsm", this);

Cu.import("resource://testing-common/CrashManagerTest.jsm", this);
Cu.import("resource://testing-common/TelemetryArchiveTesting.jsm", this);

const DUMMY_DATE = new Date(Date.now() - 10 * 24 * 60 * 60 * 1000);
DUMMY_DATE.setMilliseconds(0);

const DUMMY_DATE_2 = new Date(Date.now() - 20 * 24 * 60 * 60 * 1000);
DUMMY_DATE_2.setMilliseconds(0);

function run_test() {
  do_get_profile();
  configureLogging();
  TelemetryArchiveTesting.setup();
  run_next_test();
}

add_task(function* test_constructor_ok() {
  let m = new CrashManager({
    pendingDumpsDir: "/foo",
    submittedDumpsDir: "/bar",
    eventsDirs: [],
    storeDir: "/baz",
  });
  Assert.ok(m, "CrashManager can be created.");
});

add_task(function* test_constructor_invalid() {
  Assert.throws(() => {
    new CrashManager({foo: true});
  });
});

add_task(function* test_get_manager() {
  let m = yield getManager();
  Assert.ok(m, "CrashManager obtained.");

  yield m.createDummyDump(true);
  yield m.createDummyDump(false);
});

// Unsubmitted dump files on disk are detected properly.
add_task(function* test_pending_dumps() {
  let m = yield getManager();
  let now = Date.now();
  let ids = [];
  const COUNT = 5;

  for (let i = 0; i < COUNT; i++) {
    ids.push(yield m.createDummyDump(false, new Date(now - i * 86400000)));
  }
  yield m.createIgnoredDumpFile("ignored", false);

  let entries = yield m.pendingDumps();
  Assert.equal(entries.length, COUNT, "proper number detected.");

  for (let entry of entries) {
    Assert.equal(typeof(entry), "object", "entry is an object");
    Assert.ok("id" in entry, "id in entry");
    Assert.ok("path" in entry, "path in entry");
    Assert.ok("date" in entry, "date in entry");
    Assert.notEqual(ids.indexOf(entry.id), -1, "ID is known");
  }

  for (let i = 0; i < COUNT; i++) {
    Assert.equal(entries[i].id, ids[COUNT - i - 1], "Entries sorted by mtime");
  }
});

// Submitted dump files on disk are detected properly.
add_task(function* test_submitted_dumps() {
  let m = yield getManager();
  let COUNT = 5;

  for (let i = 0; i < COUNT; i++) {
    yield m.createDummyDump(true);
  }
  yield m.createIgnoredDumpFile("ignored", true);

  let entries = yield m.submittedDumps();
  Assert.equal(entries.length, COUNT, "proper number detected.");

  let hrID = yield m.createDummyDump(true, new Date(), true);
  entries = yield m.submittedDumps();
  Assert.equal(entries.length, COUNT + 1, "hr- in filename detected.");

  let gotIDs = new Set(entries.map(e => e.id));
  Assert.ok(gotIDs.has(hrID));
});

// The store should expire after inactivity.
add_task(function* test_store_expires() {
  let m = yield getManager();

  Object.defineProperty(m, "STORE_EXPIRATION_MS", {
    value: 250,
  });

  let store = yield m._getStore();
  Assert.ok(store);
  Assert.equal(store, m._store);

  yield sleep(300);
  Assert.ok(!m._store, "Store has gone away.");
});

// Ensure discovery of unprocessed events files works.
add_task(function* test_unprocessed_events_files() {
  let m = yield getManager();
  yield m.createEventsFile("1", "test.1", new Date(), "foo", 0);
  yield m.createEventsFile("2", "test.1", new Date(), "bar", 0);
  yield m.createEventsFile("1", "test.1", new Date(), "baz", 1);

  let paths = yield m._getUnprocessedEventsFiles();
  Assert.equal(paths.length, 3);
});

// Ensure only 1 aggregateEventsFiles() is allowed at a time.
add_task(function* test_aggregate_events_locking() {
  let m = yield getManager();

  let p1 = m.aggregateEventsFiles();
  let p2 = m.aggregateEventsFiles();

  Assert.strictEqual(p1, p2, "Same promise should be returned.");
});

// Malformed events files should be deleted.
add_task(function* test_malformed_files_deleted() {
  let m = yield getManager();

  yield m.createEventsFile("1", "crash.main.1", new Date(), "foo\nbar");

  let count = yield m.aggregateEventsFiles();
  Assert.equal(count, 1);
  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 0);

  count = yield m.aggregateEventsFiles();
  Assert.equal(count, 0);
});

// Unknown event types should be ignored.
add_task(function* test_aggregate_ignore_unknown_events() {
  let m = yield getManager();

  yield m.createEventsFile("1", "crash.main.2", DUMMY_DATE, "id1");
  yield m.createEventsFile("2", "foobar.1", new Date(), "dummy");

  let count = yield m.aggregateEventsFiles();
  Assert.equal(count, 2);

  count = yield m.aggregateEventsFiles();
  Assert.equal(count, 1);

  count = yield m.aggregateEventsFiles();
  Assert.equal(count, 1);
});

add_task(function* test_prune_old() {
  let m = yield getManager();
  let oldDate = new Date(Date.now() - 86400000);
  let newDate = new Date(Date.now() - 10000);
  yield m.createEventsFile("1", "crash.main.2", oldDate, "id1");
  yield m.addCrash(m.PROCESS_TYPE_PLUGIN, m.CRASH_TYPE_CRASH, "id2", newDate);

  yield m.aggregateEventsFiles();

  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 2);

  yield m.pruneOldCrashes(new Date(oldDate.getTime() + 10000));

  crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 1, "Old crash has been pruned.");

  let c = crashes[0];
  Assert.equal(c.id, "id2", "Proper crash was pruned.");

  // We can't test exact boundary conditions because dates from filesystem
  // don't have same guarantees as JS dates.
  yield m.pruneOldCrashes(new Date(newDate.getTime() + 5000));
  crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 0);
});

add_task(function* test_schedule_maintenance() {
  let m = yield getManager();
  yield m.createEventsFile("1", "crash.main.2", DUMMY_DATE, "id1");

  let oldDate = new Date(Date.now() - m.PURGE_OLDER_THAN_DAYS * 2 * 24 * 60 * 60 * 1000);
  yield m.createEventsFile("2", "crash.main.2", oldDate, "id2");

  yield m.scheduleMaintenance(25);
  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 1);
  Assert.equal(crashes[0].id, "id1");
});

const crashId = "3cb67eba-0dc7-6f78-6a569a0e-172287ec";
const productName = "Firefox";
const productId = "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}";
const stackTraces = "{\"status\":\"OK\"}";

add_task(function* test_main_crash_event_file() {
  let ac = new TelemetryArchiveTesting.Checker();
  yield ac.promiseInit();
  let theEnvironment = TelemetryEnvironment.currentEnvironment;
  const sessionId = "be66af2f-2ee5-4330-ae95-44462dfbdf0c";

  // To test proper escaping, add data to the environment with an embedded
  // double-quote
  theEnvironment.testValue = "MyValue\"";

  let m = yield getManager();
  const fileContent = crashId + "\n" +
    "ProductName=" + productName + "\n" +
    "ProductID=" + productId + "\n" +
    "TelemetryEnvironment=" + JSON.stringify(theEnvironment) + "\n" +
    "TelemetrySessionId=" + sessionId + "\n" +
    "StackTraces=" + stackTraces + "\n" +
    "ThisShouldNot=end-up-in-the-ping\n";

  yield m.createEventsFile(crashId, "crash.main.2", DUMMY_DATE, fileContent);
  let count = yield m.aggregateEventsFiles();
  Assert.equal(count, 1);

  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 1);
  Assert.equal(crashes[0].id, crashId);
  Assert.equal(crashes[0].type, "main-crash");
  Assert.equal(crashes[0].metadata.ProductName, productName);
  Assert.equal(crashes[0].metadata.ProductID, productId);
  Assert.ok(crashes[0].metadata.TelemetryEnvironment);
  Assert.equal(Object.getOwnPropertyNames(crashes[0].metadata).length, 6);
  Assert.equal(crashes[0].metadata.TelemetrySessionId, sessionId);
  Assert.ok(crashes[0].metadata.StackTraces);
  Assert.deepEqual(crashes[0].crashDate, DUMMY_DATE);

  let found = yield ac.promiseFindPing("crash", [
    [["payload", "hasCrashEnvironment"], true],
    [["payload", "metadata", "ProductName"], productName],
    [["payload", "metadata", "ProductID"], productId],
    [["payload", "crashId"], crashId],
    [["payload", "stackTraces", "status"], "OK"],
    [["payload", "sessionId"], sessionId],
  ]);
  Assert.ok(found, "Telemetry ping submitted for found crash");
  Assert.deepEqual(found.environment, theEnvironment,
                   "The saved environment should be present");
  Assert.equal(found.payload.metadata.ThisShouldNot, undefined,
               "Non-whitelisted fields should be filtered out");

  count = yield m.aggregateEventsFiles();
  Assert.equal(count, 0);
});

add_task(function* test_main_crash_event_file_noenv() {
  let ac = new TelemetryArchiveTesting.Checker();
  yield ac.promiseInit();
  const fileContent = crashId + "\n" +
    "ProductName=" + productName + "\n" +
    "ProductID=" + productId + "\n";

  let m = yield getManager();
  yield m.createEventsFile(crashId, "crash.main.2", DUMMY_DATE, fileContent);
  let count = yield m.aggregateEventsFiles();
  Assert.equal(count, 1);

  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 1);
  Assert.equal(crashes[0].id, crashId);
  Assert.equal(crashes[0].type, "main-crash");
  Assert.deepEqual(crashes[0].metadata, {
    ProductName: productName,
    ProductID: productId
  });
  Assert.deepEqual(crashes[0].crashDate, DUMMY_DATE);

  let found = yield ac.promiseFindPing("crash", [
    [["payload", "hasCrashEnvironment"], false],
    [["payload", "metadata", "ProductName"], productName],
    [["payload", "metadata", "ProductID"], productId],
  ]);
  Assert.ok(found, "Telemetry ping submitted for found crash");
  Assert.ok(found.environment, "There is an environment");

  count = yield m.aggregateEventsFiles();
  Assert.equal(count, 0);
});

add_task(function* test_crash_submission_event_file() {
  let m = yield getManager();
  yield m.createEventsFile("1", "crash.main.2", DUMMY_DATE, "crash1");
  yield m.createEventsFile("1-submission", "crash.submission.1", DUMMY_DATE_2,
                           "crash1\nfalse\n");

  // The line below has been intentionally commented out to make sure that
  // the crash record is created when one does not exist.
  // yield m.createEventsFile("2", "crash.main.1", DUMMY_DATE, "crash2");
  yield m.createEventsFile("2-submission", "crash.submission.1", DUMMY_DATE_2,
                           "crash2\ntrue\nbp-2");
  let count = yield m.aggregateEventsFiles();
  Assert.equal(count, 3);

  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 2);

  let map = new Map(crashes.map(crash => [crash.id, crash]));

  let crash1 = map.get("crash1");
  Assert.ok(!!crash1);
  Assert.equal(crash1.remoteID, null);
  let crash2 = map.get("crash2");
  Assert.ok(!!crash2);
  Assert.equal(crash2.remoteID, "bp-2");

  Assert.equal(crash1.submissions.size, 1);
  let submission = crash1.submissions.values().next().value;
  Assert.equal(submission.result, m.SUBMISSION_RESULT_FAILED);
  Assert.equal(submission.requestDate.getTime(), DUMMY_DATE_2.getTime());
  Assert.equal(submission.responseDate.getTime(), DUMMY_DATE_2.getTime());

  Assert.equal(crash2.submissions.size, 1);
  submission = crash2.submissions.values().next().value;
  Assert.equal(submission.result, m.SUBMISSION_RESULT_OK);
  Assert.equal(submission.requestDate.getTime(), DUMMY_DATE_2.getTime());
  Assert.equal(submission.responseDate.getTime(), DUMMY_DATE_2.getTime());

  count = yield m.aggregateEventsFiles();
  Assert.equal(count, 0);
});

add_task(function* test_multiline_crash_id_rejected() {
  let m = yield getManager();
  yield m.createEventsFile("1", "crash.main.1", DUMMY_DATE, "id1\nid2");
  yield m.aggregateEventsFiles();
  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 0);
});

// Main process crashes should be remembered beyond the high water mark.
add_task(function* test_high_water_mark() {
  let m = yield getManager();

  let store = yield m._getStore();

  for (let i = 0; i < store.HIGH_WATER_DAILY_THRESHOLD + 1; i++) {
    yield m.createEventsFile("m" + i, "crash.main.2", DUMMY_DATE, "m" + i);
  }

  let count = yield m.aggregateEventsFiles();
  Assert.equal(count, bsp.CrashStore.prototype.HIGH_WATER_DAILY_THRESHOLD + 1);

  // Need to fetch again in case the first one was garbage collected.
  store = yield m._getStore();

  Assert.equal(store.crashesCount, store.HIGH_WATER_DAILY_THRESHOLD + 1);
});

add_task(function* test_addCrash() {
  let m = yield getManager();

  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 0);

  yield m.addCrash(m.PROCESS_TYPE_MAIN, m.CRASH_TYPE_CRASH,
                   "main-crash", DUMMY_DATE);
  yield m.addCrash(m.PROCESS_TYPE_MAIN, m.CRASH_TYPE_HANG,
                   "main-hang", DUMMY_DATE);
  yield m.addCrash(m.PROCESS_TYPE_CONTENT, m.CRASH_TYPE_CRASH,
                   "content-crash", DUMMY_DATE);
  yield m.addCrash(m.PROCESS_TYPE_CONTENT, m.CRASH_TYPE_HANG,
                   "content-hang", DUMMY_DATE);
  yield m.addCrash(m.PROCESS_TYPE_PLUGIN, m.CRASH_TYPE_CRASH,
                   "plugin-crash", DUMMY_DATE);
  yield m.addCrash(m.PROCESS_TYPE_PLUGIN, m.CRASH_TYPE_HANG,
                   "plugin-hang", DUMMY_DATE);
  yield m.addCrash(m.PROCESS_TYPE_GMPLUGIN, m.CRASH_TYPE_CRASH,
                   "gmplugin-crash", DUMMY_DATE);
  yield m.addCrash(m.PROCESS_TYPE_GPU, m.CRASH_TYPE_CRASH,
                   "gpu-crash", DUMMY_DATE);

  yield m.addCrash(m.PROCESS_TYPE_MAIN, m.CRASH_TYPE_CRASH,
                   "changing-item", DUMMY_DATE);
  yield m.addCrash(m.PROCESS_TYPE_CONTENT, m.CRASH_TYPE_HANG,
                   "changing-item", DUMMY_DATE_2);

  crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 9);

  let map = new Map(crashes.map(crash => [crash.id, crash]));

  let crash = map.get("main-crash");
  Assert.ok(!!crash);
  Assert.equal(crash.crashDate, DUMMY_DATE);
  Assert.equal(crash.type, m.PROCESS_TYPE_MAIN + "-" + m.CRASH_TYPE_CRASH);
  Assert.ok(crash.isOfType(m.PROCESS_TYPE_MAIN, m.CRASH_TYPE_CRASH));

  crash = map.get("main-hang");
  Assert.ok(!!crash);
  Assert.equal(crash.crashDate, DUMMY_DATE);
  Assert.equal(crash.type, m.PROCESS_TYPE_MAIN + "-" + m.CRASH_TYPE_HANG);
  Assert.ok(crash.isOfType(m.PROCESS_TYPE_MAIN, m.CRASH_TYPE_HANG));

  crash = map.get("content-crash");
  Assert.ok(!!crash);
  Assert.equal(crash.crashDate, DUMMY_DATE);
  Assert.equal(crash.type, m.PROCESS_TYPE_CONTENT + "-" + m.CRASH_TYPE_CRASH);
  Assert.ok(crash.isOfType(m.PROCESS_TYPE_CONTENT, m.CRASH_TYPE_CRASH));

  crash = map.get("content-hang");
  Assert.ok(!!crash);
  Assert.equal(crash.crashDate, DUMMY_DATE);
  Assert.equal(crash.type, m.PROCESS_TYPE_CONTENT + "-" + m.CRASH_TYPE_HANG);
  Assert.ok(crash.isOfType(m.PROCESS_TYPE_CONTENT, m.CRASH_TYPE_HANG));

  crash = map.get("plugin-crash");
  Assert.ok(!!crash);
  Assert.equal(crash.crashDate, DUMMY_DATE);
  Assert.equal(crash.type, m.PROCESS_TYPE_PLUGIN + "-" + m.CRASH_TYPE_CRASH);
  Assert.ok(crash.isOfType(m.PROCESS_TYPE_PLUGIN, m.CRASH_TYPE_CRASH));

  crash = map.get("plugin-hang");
  Assert.ok(!!crash);
  Assert.equal(crash.crashDate, DUMMY_DATE);
  Assert.equal(crash.type, m.PROCESS_TYPE_PLUGIN + "-" + m.CRASH_TYPE_HANG);
  Assert.ok(crash.isOfType(m.PROCESS_TYPE_PLUGIN, m.CRASH_TYPE_HANG));

  crash = map.get("gmplugin-crash");
  Assert.ok(!!crash);
  Assert.equal(crash.crashDate, DUMMY_DATE);
  Assert.equal(crash.type, m.PROCESS_TYPE_GMPLUGIN + "-" + m.CRASH_TYPE_CRASH);
  Assert.ok(crash.isOfType(m.PROCESS_TYPE_GMPLUGIN, m.CRASH_TYPE_CRASH));

  crash = map.get("gpu-crash");
  Assert.ok(!!crash);
  Assert.equal(crash.crashDate, DUMMY_DATE);
  Assert.equal(crash.type, m.PROCESS_TYPE_GPU + "-" + m.CRASH_TYPE_CRASH);
  Assert.ok(crash.isOfType(m.PROCESS_TYPE_GPU, m.CRASH_TYPE_CRASH));

  crash = map.get("changing-item");
  Assert.ok(!!crash);
  Assert.equal(crash.crashDate, DUMMY_DATE_2);
  Assert.equal(crash.type, m.PROCESS_TYPE_CONTENT + "-" + m.CRASH_TYPE_HANG);
  Assert.ok(crash.isOfType(m.PROCESS_TYPE_CONTENT, m.CRASH_TYPE_HANG));
});

add_task(function* test_content_crash_ping() {
  let ac = new TelemetryArchiveTesting.Checker();
  yield ac.promiseInit();

  let m = yield getManager();
  let id = yield m.createDummyDump();
  yield m.addCrash(m.PROCESS_TYPE_CONTENT, m.CRASH_TYPE_CRASH, id, DUMMY_DATE, {
    StackTraces: stackTraces,
    ThisShouldNot: "end-up-in-the-ping"
  });
  yield m._pingPromise;

  let found = yield ac.promiseFindPing("crash", [
    [["payload", "crashId"], id],
    [["payload", "processType"], m.PROCESS_TYPE_CONTENT],
    [["payload", "stackTraces", "status"], "OK"],
  ]);
  Assert.ok(found, "Telemetry ping submitted for content crash");
  Assert.equal(found.payload.metadata.ThisShouldNot, undefined,
               "Non-whitelisted fields should be filtered out");
});

add_task(function* test_generateSubmissionID() {
  let m = yield getManager();

  const SUBMISSION_ID_REGEX =
    /^(sub-[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})$/i;
  let id = m.generateSubmissionID();
  Assert.ok(SUBMISSION_ID_REGEX.test(id));
});

add_task(function* test_addSubmissionAttemptAndResult() {
  let m = yield getManager();

  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 0);

  yield m.addCrash(m.PROCESS_TYPE_MAIN, m.CRASH_TYPE_CRASH,
                   "main-crash", DUMMY_DATE);
  yield m.addSubmissionAttempt("main-crash", "submission", DUMMY_DATE);
  yield m.addSubmissionResult("main-crash", "submission", DUMMY_DATE_2,
                              m.SUBMISSION_RESULT_OK);

  crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 1);

  let submissions = crashes[0].submissions;
  Assert.ok(!!submissions);

  let submission = submissions.get("submission");
  Assert.ok(!!submission);
  Assert.equal(submission.requestDate.getTime(), DUMMY_DATE.getTime());
  Assert.equal(submission.responseDate.getTime(), DUMMY_DATE_2.getTime());
  Assert.equal(submission.result, m.SUBMISSION_RESULT_OK);
});

add_task(function* test_addSubmissionAttemptEarlyCall() {
  let m = yield getManager();

  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 0);

  let p = m.ensureCrashIsPresent("main-crash").then(() => {
    return m.addSubmissionAttempt("main-crash", "submission", DUMMY_DATE);
  }).then(() => {
    return m.addSubmissionResult("main-crash", "submission", DUMMY_DATE_2,
                                 m.SUBMISSION_RESULT_OK);
  });

  yield m.addCrash(m.PROCESS_TYPE_MAIN, m.CRASH_TYPE_CRASH,
                   "main-crash", DUMMY_DATE);

  crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 1);

  yield p;
  let submissions = crashes[0].submissions;
  Assert.ok(!!submissions);

  let submission = submissions.get("submission");
  Assert.ok(!!submission);
  Assert.equal(submission.requestDate.getTime(), DUMMY_DATE.getTime());
  Assert.equal(submission.responseDate.getTime(), DUMMY_DATE_2.getTime());
  Assert.equal(submission.result, m.SUBMISSION_RESULT_OK);
});

add_task(function* test_setCrashClassifications() {
  let m = yield getManager();

  yield m.addCrash(m.PROCESS_TYPE_MAIN, m.CRASH_TYPE_CRASH,
                   "main-crash", DUMMY_DATE);
  yield m.setCrashClassifications("main-crash", ["a"]);
  let classifications = (yield m.getCrashes())[0].classifications;
  Assert.ok(classifications.indexOf("a") != -1);
});

add_task(function* test_setRemoteCrashID() {
  let m = yield getManager();

  yield m.addCrash(m.PROCESS_TYPE_MAIN, m.CRASH_TYPE_CRASH,
                   "main-crash", DUMMY_DATE);
  yield m.setRemoteCrashID("main-crash", "bp-1");
  Assert.equal((yield m.getCrashes())[0].remoteID, "bp-1");
});
