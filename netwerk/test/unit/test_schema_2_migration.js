/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test cookie database migration from version 2 (Gecko 1.9.3) to the current
// version, presently 4 (Gecko 2.0).
"use strict";

var test_generator = do_run_test();

function run_test() {
  do_test_pending();
  test_generator.next();
}

function finish_test() {
  executeSoon(function() {
    test_generator.return();
    do_test_finished();
  });
}

function* do_run_test() {
  // Set up a profile.
  let profile = do_get_profile();

  // Start the cookieservice, to force creation of a database.
  // Get the sessionCookies to join the initialization in cookie thread
  Services.cookiemgr.sessionCookies;

  // Close the profile.
  do_close_profile(test_generator);
  yield;

  // Remove the cookie file in order to create another database file.
  do_get_cookie_file(profile).remove(false);

  // Create a schema 2 database.
  let schema2db = new CookieDatabaseConnection(do_get_cookie_file(profile), 2);

  let now = Date.now() * 1000;
  let futureExpiry = Math.round(now / 1e6 + 1000);
  let pastExpiry = Math.round(now / 1e6 - 1000);

  // Populate it, with:
  // 1) Unexpired, unique cookies.
  for (let i = 0; i < 20; ++i) {
    let cookie = new Cookie(
      "oh" + i,
      "hai",
      "foo.com",
      "/",
      futureExpiry,
      now,
      now + i,
      false,
      false,
      false
    );

    schema2db.insertCookie(cookie);
  }

  // 2) Expired, unique cookies.
  for (let i = 20; i < 40; ++i) {
    let cookie = new Cookie(
      "oh" + i,
      "hai",
      "bar.com",
      "/",
      pastExpiry,
      now,
      now + i,
      false,
      false,
      false
    );

    schema2db.insertCookie(cookie);
  }

  // 3) Many copies of the same cookie, some of which have expired and
  // some of which have not.
  for (let i = 40; i < 45; ++i) {
    let cookie = new Cookie(
      "oh",
      "hai",
      "baz.com",
      "/",
      futureExpiry + i,
      now,
      now + i,
      false,
      false,
      false
    );

    schema2db.insertCookie(cookie);
  }
  for (let i = 45; i < 50; ++i) {
    let cookie = new Cookie(
      "oh",
      "hai",
      "baz.com",
      "/",
      pastExpiry - i,
      now,
      now + i,
      false,
      false,
      false
    );

    schema2db.insertCookie(cookie);
  }
  for (let i = 50; i < 55; ++i) {
    let cookie = new Cookie(
      "oh",
      "hai",
      "baz.com",
      "/",
      futureExpiry - i,
      now,
      now + i,
      false,
      false,
      false
    );

    schema2db.insertCookie(cookie);
  }
  for (let i = 55; i < 60; ++i) {
    let cookie = new Cookie(
      "oh",
      "hai",
      "baz.com",
      "/",
      pastExpiry + i,
      now,
      now + i,
      false,
      false,
      false
    );

    schema2db.insertCookie(cookie);
  }

  // Close it.
  schema2db.close();
  schema2db = null;

  // Load the database, forcing migration to the current schema version. Then
  // test the expected set of cookies:
  do_load_profile();

  // 1) All unexpired, unique cookies exist.
  Assert.equal(Services.cookiemgr.countCookiesFromHost("foo.com"), 20);

  // 2) All expired, unique cookies exist.
  Assert.equal(Services.cookiemgr.countCookiesFromHost("bar.com"), 20);

  // 3) Only one cookie remains, and it's the one with the highest expiration
  // time.
  Assert.equal(Services.cookiemgr.countCookiesFromHost("baz.com"), 1);
  let cookies = Services.cookiemgr.getCookiesFromHost("baz.com", {});
  let cookie = cookies[0];
  Assert.equal(cookie.expiry, futureExpiry + 44);

  do_close_profile(test_generator);
  yield;

  // Open the database so we can execute some more schema 2 statements on it.
  schema2db = new CookieDatabaseConnection(do_get_cookie_file(profile), 2);

  // Populate it with more cookies.
  for (let i = 60; i < 80; ++i) {
    let cookie = new Cookie(
      "oh" + i,
      "hai",
      "foo.com",
      "/",
      futureExpiry,
      now,
      now + i,
      false,
      false,
      false
    );

    schema2db.insertCookie(cookie);
  }
  for (let i = 80; i < 100; ++i) {
    let cookie = new Cookie(
      "oh" + i,
      "hai",
      "cat.com",
      "/",
      futureExpiry,
      now,
      now + i,
      false,
      false,
      false
    );

    schema2db.insertCookie(cookie);
  }

  // Attempt to add a cookie with the same (name, host, path) values as another
  // cookie. This should succeed since we have a REPLACE clause for conflict on
  // the unique index.
  cookie = new Cookie(
    "oh",
    "hai",
    "baz.com",
    "/",
    futureExpiry,
    now,
    now + 100,
    false,
    false,
    false
  );

  schema2db.insertCookie(cookie);

  // Check that there is, indeed, a singular cookie for baz.com.
  Assert.equal(do_count_cookies_in_db(schema2db.db, "baz.com"), 1);

  // Close it.
  schema2db.close();
  schema2db = null;

  // Back up the database, so we can test both asynchronous and synchronous
  // loading separately.
  let file = do_get_cookie_file(profile);
  let copy = profile.clone();
  copy.append("cookies.sqlite.copy");
  file.copyTo(null, copy.leafName);

  // Load the database asynchronously, forcing a purge of the newly-added
  // cookies. (Their baseDomain column will be NULL.)
  do_load_profile(test_generator);
  yield;

  // Test the expected set of cookies.
  Assert.equal(Services.cookiemgr.countCookiesFromHost("foo.com"), 40);
  Assert.equal(Services.cookiemgr.countCookiesFromHost("bar.com"), 20);
  Assert.equal(Services.cookiemgr.countCookiesFromHost("baz.com"), 1);
  Assert.equal(Services.cookiemgr.countCookiesFromHost("cat.com"), 20);

  do_close_profile(test_generator);
  yield;

  // Copy the database back.
  file.remove(false);
  copy.copyTo(null, file.leafName);

  // Load the database host-at-a-time.
  do_load_profile();

  // Test the expected set of cookies.
  Assert.equal(Services.cookiemgr.countCookiesFromHost("foo.com"), 40);
  Assert.equal(Services.cookiemgr.countCookiesFromHost("bar.com"), 20);
  Assert.equal(Services.cookiemgr.countCookiesFromHost("baz.com"), 1);
  Assert.equal(Services.cookiemgr.countCookiesFromHost("cat.com"), 20);

  do_close_profile(test_generator);
  yield;

  // Open the database and prove that they were deleted.
  schema2db = new CookieDatabaseConnection(do_get_cookie_file(profile), 2);
  Assert.equal(do_count_cookies_in_db(schema2db.db), 81);
  Assert.equal(do_count_cookies_in_db(schema2db.db, "foo.com"), 40);
  Assert.equal(do_count_cookies_in_db(schema2db.db, "bar.com"), 20);
  schema2db.close();

  // Copy the database back.
  file.remove(false);
  copy.copyTo(null, file.leafName);

  // Load the database synchronously, in its entirety.
  do_load_profile();
  Assert.equal(do_count_cookies(), 81);

  // Test the expected set of cookies.
  Assert.equal(Services.cookiemgr.countCookiesFromHost("foo.com"), 40);
  Assert.equal(Services.cookiemgr.countCookiesFromHost("bar.com"), 20);
  Assert.equal(Services.cookiemgr.countCookiesFromHost("baz.com"), 1);
  Assert.equal(Services.cookiemgr.countCookiesFromHost("cat.com"), 20);

  do_close_profile(test_generator);
  yield;

  // Open the database and prove that they were deleted.
  schema2db = new CookieDatabaseConnection(do_get_cookie_file(profile), 2);
  Assert.equal(do_count_cookies_in_db(schema2db.db), 81);
  Assert.equal(do_count_cookies_in_db(schema2db.db, "foo.com"), 40);
  Assert.equal(do_count_cookies_in_db(schema2db.db, "bar.com"), 20);
  schema2db.close();

  finish_test();
}
