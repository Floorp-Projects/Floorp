/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test cookie database migration from version 10 (prerelease Gecko 2.0) to the
// current version, presently 12.
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

  // Create a schema 10 database.
  let schema10db = new CookieDatabaseConnection(
    do_get_cookie_file(profile),
    10
  );

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

    schema10db.insertCookie(cookie);
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

    schema10db.insertCookie(cookie);
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

    try {
      schema10db.insertCookie(cookie);
    } catch (e) {}
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

    try {
      schema10db.insertCookie(cookie);
    } catch (e) {}
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

    try {
      schema10db.insertCookie(cookie);
    } catch (e) {}
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

    try {
      schema10db.insertCookie(cookie);
    } catch (e) {}
  }

  // Close it.
  schema10db.close();
  schema10db = null;

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
  Assert.equal(cookie.expiry, futureExpiry + 40);

  finish_test();
}
