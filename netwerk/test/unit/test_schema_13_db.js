/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test cookie database schema 13
"use strict";

add_task(async function test_schema_13_db() {
  // Set up a profile.
  let profile = do_get_profile();

  // Start the cookieservice, to force creation of a database.
  Services.cookies.sessionCookies;

  // Assert schema 13 cookie db was created
  Assert.ok(do_get_cookie_file(profile).exists());
  let dbConnection = Services.storage.openDatabase(do_get_cookie_file(profile));
  let version = dbConnection.schemaVersion;
  dbConnection.close();
  Assert.equal(version, 13);

  // Close the profile.
  await promise_close_profile();

  // Open CookieDatabaseConnection to manipulate DB without using services.
  let schema13db = new CookieDatabaseConnection(
    do_get_cookie_file(profile),
    13
  );

  let now = Date.now() * 1000;
  let futureExpiry = Math.round(now / 1e6 + 1000);

  let N = 20;
  // Populate db with N unexpired, unique, partially partitioned cookies.
  for (let i = 0; i < N; ++i) {
    let cookie = new Cookie(
      "test" + i,
      "Some data",
      "foo.com",
      "/",
      futureExpiry,
      now,
      now,
      false,
      false,
      false,
      false,
      {},
      Ci.nsICookie.SAMESITE_NONE,
      Ci.nsICookie.SAMESITE_NONE,
      Ci.nsICookie.SCHEME_UNSET,
      !!(i % 2) // isPartitioned
    );
    schema13db.insertCookie(cookie);
  }
  schema13db.close();
  schema13db = null;

  // Reload profile.
  await promise_load_profile();

  // Assert inserted cookies are in the db and correctly handled by services.
  Assert.equal(Services.cookies.countCookiesFromHost("foo.com"), N);

  // Open connection to manipulated db
  dbConnection = Services.storage.openDatabase(do_get_cookie_file(profile));
  // Check that schema is still correct after profile reload / db opening
  Assert.equal(dbConnection.schemaVersion, 13);

  // Count cookies with isPartitionedAttributeSet set to 1 (true)
  let stmt = dbConnection.createStatement(
    "SELECT COUNT(1) FROM moz_cookies WHERE isPartitionedAttributeSet = 1"
  );
  let success = stmt.executeStep();
  Assert.ok(success);

  // Assert the correct number of partitioned cookies was inserted / read
  let isPartitionedAttributeSetCount = stmt.getInt32(0);
  stmt.finalize();
  Assert.equal(isPartitionedAttributeSetCount, N / 2);

  // Cleanup
  Services.cookies.removeAll();
  stmt.finalize();
  dbConnection.close();
  do_close_profile();
});
