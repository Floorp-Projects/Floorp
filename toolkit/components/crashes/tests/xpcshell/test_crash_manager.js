/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/CrashManager.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

Cu.import("resource://testing-common/CrashManagerTest.jsm", this);

const DUMMY_DATE = new Date(1391043519000);

function run_test() {
  do_get_profile();
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

  run_next_test();
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
    Assert.equal(entries[i].id, ids[COUNT-i-1], "Entries sorted by mtime");
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

  let gotIDs = new Set([e.id for (e of entries)]);
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

  yield m.createEventsFile("1", "crash.main.1", DUMMY_DATE, "id1");
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
  yield m.createEventsFile("1", "crash.main.1", oldDate, "id1");
  yield m.createEventsFile("2", "crash.plugin.1", newDate, "id2");

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
  yield m.createEventsFile("1", "crash.main.1", DUMMY_DATE, "id1");

  let oldDate = new Date(Date.now() - m.PURGE_OLDER_THAN_DAYS * 2 * 24 * 60 * 60 * 1000);
  yield m.createEventsFile("2", "crash.main.1", oldDate, "id2");

  yield m.scheduleMaintenance(25);
  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 1);
  Assert.equal(crashes[0].id, "id1");
});

add_task(function* test_main_crash_event_file() {
  let m = yield getManager();
  yield m.createEventsFile("1", "crash.main.1", DUMMY_DATE, "id1");
  let count = yield m.aggregateEventsFiles();
  Assert.equal(count, 1);

  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 1);
  Assert.equal(crashes[0].id, "id1");
  Assert.equal(crashes[0].type, "main-crash");
  Assert.deepEqual(crashes[0].crashDate, DUMMY_DATE);

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

add_task(function* test_plugin_crash_event_file() {
  let m = yield getManager();
  yield m.createEventsFile("1", "crash.plugin.1", DUMMY_DATE, "id1");
  let count = yield m.aggregateEventsFiles();
  Assert.equal(count, 1);

  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 1);
  Assert.equal(crashes[0].id, "id1");
  Assert.equal(crashes[0].type, "plugin-crash");
  Assert.deepEqual(crashes[0].crashDate, DUMMY_DATE);

  count = yield m.aggregateEventsFiles();
  Assert.equal(count, 0);
});

add_task(function* test_plugin_hang_event_file() {
  let m = yield getManager();
  yield m.createEventsFile("1", "hang.plugin.1", DUMMY_DATE, "id1");
  let count = yield m.aggregateEventsFiles();
  Assert.equal(count, 1);

  let crashes = yield m.getCrashes();
  Assert.equal(crashes.length, 1);
  Assert.equal(crashes[0].id, "id1");
  Assert.equal(crashes[0].type, "plugin-hang");
  Assert.deepEqual(crashes[0].crashDate, DUMMY_DATE);

  count = yield m.aggregateEventsFiles();
  Assert.equal(count, 0);
});
