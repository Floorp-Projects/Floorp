/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const USEC_PER_SEC = 1000 * 1000;
const ONE_DAY = 60 * 60 * 24 * USEC_PER_SEC;
const ONE_YEAR = ONE_DAY * 365;
const LAST_ACCESSED_DIFF = 10 * ONE_YEAR;
const CREATION_DIFF = 100 * ONE_YEAR;

function initDB(conn, now) {
  // Write the schema v7 to the database.
  conn.schemaVersion = 7;
  conn.executeSimpleSQL(
    "CREATE TABLE moz_cookies (" +
      "id INTEGER PRIMARY KEY, " +
      "baseDomain TEXT, " +
      "originAttributes TEXT NOT NULL DEFAULT '', " +
      "name TEXT, " +
      "value TEXT, " +
      "host TEXT, " +
      "path TEXT, " +
      "expiry INTEGER, " +
      "lastAccessed INTEGER, " +
      "creationTime INTEGER, " +
      "isSecure INTEGER, " +
      "isHttpOnly INTEGER, " +
      "appId INTEGER DEFAULT 0, " +
      "inBrowserElement INTEGER DEFAULT 0, " +
      "CONSTRAINT moz_uniqueid UNIQUE (name, host, path, originAttributes)" +
      ")"
  );
  conn.executeSimpleSQL(
    "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain, " +
      "originAttributes)"
  );

  conn.executeSimpleSQL("PRAGMA synchronous = OFF");
  conn.executeSimpleSQL("PRAGMA journal_mode = WAL");
  conn.executeSimpleSQL("PRAGMA wal_autocheckpoint = 16");

  conn.executeSimpleSQL(
    `INSERT INTO moz_cookies(baseDomain, host, name, value, path, expiry, lastAccessed, creationTime, isSecure, isHttpOnly) 
    VALUES ('foo.com', '.foo.com', 'foo', 'bar=baz', '/',
    ${now + ONE_DAY}, ${now + LAST_ACCESSED_DIFF} , ${
      now + CREATION_DIFF
    } , 1, 1)`
  );
}

add_task(async function test_timestamp_fixup() {
  let now = Date.now() * 1000; // date in microseconds
  Services.prefs.setBoolPref("network.cookie.fixup_on_db_load", true);
  do_get_profile();
  let dbFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
  dbFile.append("cookies.sqlite");
  let conn = Services.storage.openDatabase(dbFile);
  initDB(conn, now);

  if (AppConstants.platform != "android") {
    Services.fog.initializeFOG();
  }
  Services.fog.testResetFOG();

  // Now start the cookie service, and then check the fields in the table.
  // Get sessionCookies to wait for the initialization in cookie thread
  Assert.lessOrEqual(
    Math.floor(Services.cookies.cookies[0].creationTime / 1000),
    now
  );
  Assert.equal(conn.schemaVersion, 13);

  Assert.equal(
    await Glean.networking.cookieTimestampFixedCount.creationTime.testGetValue(),
    1,
    "One fixup of creation time"
  );
  Assert.equal(
    await Glean.networking.cookieTimestampFixedCount.lastAccessed.testGetValue(),
    1,
    "One fixup of lastAccessed"
  );
  {
    let { values } =
      await Glean.networking.cookieCreationFixupDiff.testGetValue();
    info(JSON.stringify(values));
    let keys = Object.keys(values).splice(-2, 2);
    Assert.equal(keys.length, 2, "There should be two entries in telemetry");
    Assert.equal(values[keys[0]], 1, "First entry should have value 1");
    Assert.equal(values[keys[1]], 0, "Second entry should have value 0");
    const creationDiffInSeconds = CREATION_DIFF / USEC_PER_SEC;
    Assert.lessOrEqual(
      parseInt(keys[0]),
      creationDiffInSeconds,
      "The bucket should be smaller than time diff"
    );
    Assert.lessOrEqual(
      creationDiffInSeconds,
      parseInt(keys[1]),
      "The next bucket should be larger than time diff"
    );
  }

  {
    let { values } =
      await Glean.networking.cookieAccessFixupDiff.testGetValue();
    info(JSON.stringify(values));
    let keys = Object.keys(values).splice(-2, 2);
    Assert.equal(keys.length, 2, "There should be two entries in telemetry");
    Assert.equal(values[keys[0]], 1, "First entry should have value 1");
    Assert.equal(values[keys[1]], 0, "Second entry should have value 0");
    info(now);
    const lastAccessedDiffInSeconds = LAST_ACCESSED_DIFF / USEC_PER_SEC;
    Assert.lessOrEqual(
      parseInt(keys[0]),
      lastAccessedDiffInSeconds,
      "The bucket should be smaller than time diff"
    );
    Assert.lessOrEqual(
      lastAccessedDiffInSeconds,
      parseInt(keys[1]),
      "The next bucket should be larger than time diff"
    );
  }

  conn.close();
});
