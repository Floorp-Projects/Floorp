/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This file tests the Vacuum Manager and asyncVacuum().

const { VacuumParticipant } = ChromeUtils.importESModule(
  "resource://testing-common/VacuumParticipant.sys.mjs"
);

/**
 * Sends a fake idle-daily notification to the VACUUM Manager.
 */
function synthesize_idle_daily() {
  Cc["@mozilla.org/storage/vacuum;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "idle-daily", null);
}

/**
 * Returns a new nsIFile reference for a profile database.
 * @param filename for the database, excluded the .sqlite extension.
 */
function new_db_file(name = "testVacuum") {
  let file = Services.dirsvc.get("ProfD", Ci.nsIFile);
  file.append(name + ".sqlite");
  return file;
}

function reset_vacuum_date(name = "testVacuum") {
  let date = parseInt(Date.now() / 1000 - 31 * 86400);
  // Set last VACUUM to a date in the past.
  Services.prefs.setIntPref(`storage.vacuum.last.${name}.sqlite`, date);
  return date;
}

function get_vacuum_date(name = "testVacuum") {
  return Services.prefs.getIntPref(`storage.vacuum.last.${name}.sqlite`, 0);
}

add_setup(async function () {
  // turn on Cu.isInAutomation
  Services.prefs.setBoolPref(
    "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
    true
  );
});

add_task(async function test_common_vacuum() {
  let last_vacuum_date = reset_vacuum_date();
  info("Test that a VACUUM correctly happens and all notifications are fired.");
  let promiseTestVacuumBegin = TestUtils.topicObserved("test-begin-vacuum");
  let promiseTestVacuumEnd = TestUtils.topicObserved("test-end-vacuum-success");
  let promiseVacuumBegin = TestUtils.topicObserved("vacuum-begin");
  let promiseVacuumEnd = TestUtils.topicObserved("vacuum-end");

  let participant = new VacuumParticipant(
    Services.storage.openDatabase(new_db_file())
  );
  await participant.promiseRegistered();
  synthesize_idle_daily();
  // Wait for notifications.
  await Promise.all([
    promiseTestVacuumBegin,
    promiseTestVacuumEnd,
    promiseVacuumBegin,
    promiseVacuumEnd,
  ]);
  Assert.greater(get_vacuum_date(), last_vacuum_date);
  await participant.dispose();
});

add_task(async function test_skipped_if_recent_vacuum() {
  info("Test that a VACUUM is skipped if it was run recently.");
  Services.prefs.setIntPref(
    "storage.vacuum.last.testVacuum.sqlite",
    parseInt(Date.now() / 1000)
  );
  // Wait for VACUUM skipped notification.
  let promiseSkipped = TestUtils.topicObserved("vacuum-skip");

  let participant = new VacuumParticipant(
    Services.storage.openDatabase(new_db_file())
  );
  await participant.promiseRegistered();
  synthesize_idle_daily();

  // Check that VACUUM has been skipped.
  await promiseSkipped;

  await participant.dispose();
});

add_task(async function test_page_size_change() {
  info("Test that a VACUUM changes page_size");
  reset_vacuum_date();

  let conn = Services.storage.openDatabase(new_db_file());
  info("Check initial page size.");
  let stmt = conn.createStatement("PRAGMA page_size");
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.row.page_size, conn.defaultPageSize);
  stmt.finalize();
  await populateFreeList(conn);

  let participant = new VacuumParticipant(conn, { expectedPageSize: 1024 });
  await participant.promiseRegistered();
  let promiseVacuumEnd = TestUtils.topicObserved("test-end-vacuum-success");
  synthesize_idle_daily();
  await promiseVacuumEnd;

  info("Check that page size was updated.");
  stmt = conn.createStatement("PRAGMA page_size");
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.row.page_size, 1024);
  stmt.finalize();

  await participant.dispose();
});

add_task(async function test_skipped_optout_vacuum() {
  info("Test that a VACUUM is skipped if the participant wants to opt-out.");
  reset_vacuum_date();

  let participant = new VacuumParticipant(
    Services.storage.openDatabase(new_db_file()),
    { grant: false }
  );
  await participant.promiseRegistered();
  // Wait for VACUUM skipped notification.
  let promiseSkipped = TestUtils.topicObserved("vacuum-skip");

  synthesize_idle_daily();

  // Check that VACUUM has been skipped.
  await promiseSkipped;

  await participant.dispose();
});

add_task(async function test_memory_database_crash() {
  info("Test that we don't crash trying to vacuum a memory database");
  reset_vacuum_date();

  let participant = new VacuumParticipant(
    Services.storage.openSpecialDatabase("memory")
  );
  await participant.promiseRegistered();
  // Wait for VACUUM skipped notification.
  let promiseSkipped = TestUtils.topicObserved("vacuum-skip");

  synthesize_idle_daily();

  // Check that VACUUM has been skipped.
  await promiseSkipped;

  await participant.dispose();
});

add_task(async function test_async_connection() {
  info("Test we can vacuum an async connection");
  reset_vacuum_date();

  let conn = await openAsyncDatabase(new_db_file());
  await populateFreeList(conn);
  let participant = new VacuumParticipant(conn);
  await participant.promiseRegistered();

  let promiseVacuumEnd = TestUtils.topicObserved("test-end-vacuum-success");
  synthesize_idle_daily();
  await promiseVacuumEnd;

  await participant.dispose();
});

add_task(async function test_change_to_incremental_vacuum() {
  info("Test we can change to incremental vacuum");
  reset_vacuum_date();

  let conn = Services.storage.openDatabase(new_db_file());
  info("Check initial vacuum.");
  let stmt = conn.createStatement("PRAGMA auto_vacuum");
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.row.auto_vacuum, 0);
  stmt.finalize();
  await populateFreeList(conn);

  let participant = new VacuumParticipant(conn, { useIncrementalVacuum: true });
  await participant.promiseRegistered();
  let promiseVacuumEnd = TestUtils.topicObserved("test-end-vacuum-success");
  synthesize_idle_daily();
  await promiseVacuumEnd;

  info("Check that auto_vacuum was updated.");
  stmt = conn.createStatement("PRAGMA auto_vacuum");
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.row.auto_vacuum, 2);
  stmt.finalize();

  await participant.dispose();
});

add_task(async function test_change_from_incremental_vacuum() {
  info("Test we can change from incremental vacuum");
  reset_vacuum_date();

  let conn = Services.storage.openDatabase(new_db_file());
  conn.executeSimpleSQL("PRAGMA auto_vacuum = 2");
  info("Check initial vacuum.");
  let stmt = conn.createStatement("PRAGMA auto_vacuum");
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.row.auto_vacuum, 2);
  stmt.finalize();
  await populateFreeList(conn);

  let participant = new VacuumParticipant(conn, {
    useIncrementalVacuum: false,
  });
  await participant.promiseRegistered();
  let promiseVacuumEnd = TestUtils.topicObserved("test-end-vacuum-success");
  synthesize_idle_daily();
  await promiseVacuumEnd;

  info("Check that auto_vacuum was updated.");
  stmt = conn.createStatement("PRAGMA auto_vacuum");
  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.row.auto_vacuum, 0);
  stmt.finalize();

  await participant.dispose();
});

add_task(async function test_attached_vacuum() {
  info("Test attached database is not a problem");
  reset_vacuum_date();

  let conn = Services.storage.openDatabase(new_db_file());
  let conn2 = Services.storage.openDatabase(new_db_file("attached"));

  info("Attach " + conn2.databaseFile.path);
  conn.executeSimpleSQL(
    `ATTACH DATABASE '${conn2.databaseFile.path}' AS attached`
  );
  await asyncClose(conn2);
  let stmt = conn.createStatement("PRAGMA database_list");
  let schemas = [];
  while (stmt.executeStep()) {
    schemas.push(stmt.row.name);
  }
  Assert.deepEqual(schemas, ["main", "attached"]);
  stmt.finalize();

  await populateFreeList(conn);
  await populateFreeList(conn, "attached");

  let participant = new VacuumParticipant(conn);
  await participant.promiseRegistered();
  let promiseVacuumEnd = TestUtils.topicObserved("test-end-vacuum-success");
  synthesize_idle_daily();
  await promiseVacuumEnd;

  await participant.dispose();
});

add_task(async function test_vacuum_fail() {
  info("Test a failed vacuum");
  reset_vacuum_date();

  let conn = Services.storage.openDatabase(new_db_file());
  // Cannot vacuum in a transaction.
  conn.beginTransaction();
  await populateFreeList(conn);

  let participant = new VacuumParticipant(conn);
  await participant.promiseRegistered();
  let promiseVacuumEnd = TestUtils.topicObserved("test-end-vacuum-failure");
  synthesize_idle_daily();
  await promiseVacuumEnd;

  conn.commitTransaction();
  await participant.dispose();
});

add_task(async function test_async_vacuum() {
  // Since previous tests already go through most cases, this only checks
  // the basics of directly calling asyncVacuum().
  info("Test synchronous connection");
  let conn = Services.storage.openDatabase(new_db_file());
  await populateFreeList(conn);
  let rv = await new Promise(resolve => {
    conn.asyncVacuum(status => {
      resolve(status);
    });
  });
  Assert.ok(Components.isSuccessCode(rv));
  await asyncClose(conn);

  info("Test asynchronous connection");
  conn = await openAsyncDatabase(new_db_file());
  await populateFreeList(conn);
  rv = await new Promise(resolve => {
    conn.asyncVacuum(status => {
      resolve(status);
    });
  });
  Assert.ok(Components.isSuccessCode(rv));
  await asyncClose(conn);
});

// Chunked growth is disabled on Android, so this test is pointless there.
add_task(
  { skip_if: () => AppConstants.platform == "android" },
  async function test_vacuum_growth() {
    // Tests vacuum doesn't nullify chunked growth.
    let conn = Services.storage.openDatabase(new_db_file("incremental"));
    conn.executeSimpleSQL("PRAGMA auto_vacuum = INCREMENTAL");
    conn.setGrowthIncrement(2 * conn.defaultPageSize, "");
    await populateFreeList(conn);
    let stmt = conn.createStatement("PRAGMA freelist_count");
    let count = 0;
    Assert.ok(stmt.executeStep());
    count = stmt.row.freelist_count;
    stmt.reset();
    Assert.greater(count, 2, "There's more than 2 page in freelist");

    let rv = await new Promise(resolve => {
      conn.asyncVacuum(status => {
        resolve(status);
      }, true);
    });
    Assert.ok(Components.isSuccessCode(rv));

    Assert.ok(stmt.executeStep());
    Assert.equal(
      stmt.row.freelist_count,
      2,
      "chunked growth space was preserved"
    );
    stmt.reset();

    // A full vacuuum should not be executed if there's less free pages than
    // chunked growth.
    rv = await new Promise(resolve => {
      conn.asyncVacuum(status => {
        resolve(status);
      });
    });
    Assert.ok(Components.isSuccessCode(rv));

    Assert.ok(stmt.executeStep());
    Assert.equal(
      stmt.row.freelist_count,
      2,
      "chunked growth space was preserved"
    );
    stmt.finalize();

    await asyncClose(conn);
  }
);

async function populateFreeList(conn, schema = "main") {
  await executeSimpleSQLAsync(conn, `CREATE TABLE ${schema}.test (id TEXT)`);
  await executeSimpleSQLAsync(
    conn,
    `INSERT INTO ${schema}.test
     VALUES ${Array.from({ length: 3000 }, () => Math.random()).map(
       v => "('" + v + "')"
     )}`
  );
  await executeSimpleSQLAsync(conn, `DROP TABLE ${schema}.test`);
}
