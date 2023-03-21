"use strict";

const { Sqlite } = ChromeUtils.importESModule(
  "resource://gre/modules/Sqlite.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

/**
 * Sends a fake idle-daily notification to the VACUUM Manager.
 */
function synthesize_idle_daily() {
  Cc["@mozilla.org/storage/vacuum;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "idle-daily", null);
}

function unregister_vacuum_participants() {
  // First unregister other participants.
  for (let { data: entry } of Services.catMan.enumerateCategory(
    "vacuum-participant"
  )) {
    Services.catMan.deleteCategoryEntry("vacuum-participant", entry, false);
  }
}

function reset_vacuum_date(dbname) {
  let date = parseInt(Date.now() / 1000 - 31 * 86400);
  // Set last VACUUM to a date in the past.
  Services.prefs.setIntPref(`storage.vacuum.last.${dbname}`, date);
  return date;
}

function get_vacuum_date(dbname) {
  return Services.prefs.getIntPref(`storage.vacuum.last.${dbname}`, 0);
}

async function get_freelist_count(conn) {
  return (await conn.execute("PRAGMA freelist_count"))[0].getResultByIndex(0);
}

async function get_auto_vacuum(conn) {
  return (await conn.execute("PRAGMA auto_vacuum"))[0].getResultByIndex(0);
}

async function test_vacuum(options = {}) {
  unregister_vacuum_participants();
  const dbName = "testVacuum.sqlite";
  const dbFile = PathUtils.join(PathUtils.profileDir, dbName);
  let lastVacuumDate = reset_vacuum_date(dbName);
  let conn = await Sqlite.openConnection(
    Object.assign(
      {
        path: dbFile,
        vacuumOnIdle: true,
      },
      options
    )
  );
  // Ensure the category manager is up-to-date.
  await TestUtils.waitForTick();

  Assert.equal(
    await get_auto_vacuum(conn),
    options.incrementalVacuum ? 2 : 0,
    "Check auto_vacuum"
  );

  // Generate some freelist page.
  await conn.execute("CREATE TABLE test (id INTEGER)");
  await conn.execute("DROP TABLE test");
  Assert.greater(await get_freelist_count(conn), 0, "Check freelist_count");

  let promiseVacuumEnd = TestUtils.topicObserved(
    "vacuum-end",
    (_, d) => d == dbName
  );
  synthesize_idle_daily();
  info("Await vacuum end");
  await promiseVacuumEnd;

  Assert.greater(get_vacuum_date(dbName), lastVacuumDate);

  Assert.equal(await get_freelist_count(conn), 0, "Check freelist_count");

  await conn.close();
  await IOUtils.remove(dbFile);
}

add_task(async function test_vacuumOnIdle() {
  info("Test full vacuum");
  await test_vacuum();
  info("Test incremental vacuum");
  await test_vacuum({ incrementalVacuum: true });
});
