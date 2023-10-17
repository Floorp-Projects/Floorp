/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Note that E2E test doesn't seem possible via setting a cookie
// from xpcshell (main process) since:
// 1. setCookieStringFromHttp requires a valid URL
// 2. setCookieStringFromDocument requires access to the document
// 3. Services.cookies.add() is just a backdoor that will bypass
// Similarly, even with a browser test, in order to call
// content.document.cookie we would need to SpecialPowers.spawn
// into a BrowserTestUtils tab which requires a valid URL

// not possible to make a valid url with non-ipv4 hostname ending in a number
add_task(async function test_url_failure() {
  let validUrl = Services.io.newURI("https://example.com/");
  Assert.equal(validUrl.host, "example.com");

  // ipv4 ending in number is fine
  let validUrl2 = Services.io.newURI("https://1.2.3.4");
  Assert.equal(validUrl2.host, "1.2.3.4");

  // non-ipv4 ending in number is not
  try {
    Assert.throws(
      () => {
        Services.io.newURI("https://example.0");
      },
      /NS_ERROR_MALFORMED_URI/,
      "non-ipv3 ending in number throws"
    );
  } catch {}
});

add_task(async function test_migrate_invalid_cookie() {
  let profile = do_get_profile();
  let dbFile = do_get_cookie_file(profile);
  Assert.ok(!dbFile.exists());

  let schema12db = new CookieDatabaseConnection(dbFile, 12);

  let now = Date.now() * 1000; // date in microseconds
  let futureExpiry = Math.round(now / 1e6 + 1000);

  let cookie1 = new Cookie(
    "cookie-name1",
    "cookie-value1",
    "cookie-host1.com",
    "/", // path
    futureExpiry,
    now,
    now,
    false,
    false,
    false
  );

  // non-ipv4 urls that have a hostname ending in a number are now invalid
  let badcookie = new Cookie(
    "cookie-name",
    "cookie-value",
    "cookie-host.0",
    "/", // path
    futureExpiry,
    now,
    now,
    false,
    false,
    false
  );

  let cookie2 = new Cookie(
    "cookie-name2",
    "cookie-value2",
    "cookie-host2.com",
    "/", // path
    futureExpiry,
    now,
    now,
    false,
    false,
    false
  );

  schema12db.insertCookie(cookie1);
  schema12db.insertCookie(badcookie);
  schema12db.insertCookie(cookie2);

  // check that 3 cookies were added to the db
  Assert.equal(do_count_cookies_in_db(schema12db.db), 3);
  Assert.equal(do_count_cookies_in_db(schema12db.db, "cookie-host1.com"), 1);
  Assert.equal(do_count_cookies_in_db(schema12db.db, "cookie-host2.com"), 1);
  Assert.equal(do_count_cookies_in_db(schema12db.db, "cookie-host.0"), 1);

  // start the cookie service, pull the data into memory
  // check that cookie1 and cookie2 were brought into memory
  // and that badcookie was not
  let cookie1Exists = false;
  let cookie2Exists = false;
  let badcookieExists = false;
  for (let cookie of Services.cookies.cookies) {
    if (cookie.host == "cookie-host1.com") {
      cookie1Exists = true;
    }
    if (cookie.host == "cookie-host2.com") {
      cookie2Exists = true;
    }
    if (cookie.host == "cookie-host.0") {
      badcookieExists = true;
    }
  }
  Assert.ok(cookie1Exists, "Cookie 1 was inadvertently removed");
  Assert.ok(cookie2Exists, "Cookie 2 was inadvertently removed");
  Assert.ok(!badcookieExists, "Bad cookie was not filtered by migration");
  Assert.equal(schema12db.db.schemaVersion, 12);

  // reload to make sure removal was written correctly
  await promise_close_profile();
  do_load_profile();

  // check that the db was also updated
  Assert.equal(do_count_cookies_in_db(schema12db.db), 2);
  Assert.equal(do_count_cookies_in_db(schema12db.db, "cookie-host1.com"), 1);
  Assert.equal(do_count_cookies_in_db(schema12db.db, "cookie-host2.com"), 1);
  Assert.equal(do_count_cookies_in_db(schema12db.db, "cookie-host.0"), 0);

  schema12db.close();
});
