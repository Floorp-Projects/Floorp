/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function count_IsPartitioned(bool) {
  var cookieCountIsPartitioned = 0;
  Services.cookies.cookies.forEach(cookie => {
    if (cookie.isPartitioned === bool) {
      cookieCountIsPartitioned += 1;
    }
  });
  return cookieCountIsPartitioned;
}

add_task(async function test_IsPartitioned() {
  let profile = do_get_profile();
  let dbFile = do_get_cookie_file(profile);
  Assert.ok(!dbFile.exists());

  let schema13db = new CookieDatabaseConnection(dbFile, 13);
  let now = Date.now() * 1000; // date in microseconds

  // add some non-partitioned cookies for key
  let nUnpartitioned = 5;
  let hostNonPartitioned = "cookie-host-non-partitioned.com";
  for (let i = 0; i < nUnpartitioned; i++) {
    let cookie = new Cookie(
      "cookie-name" + i,
      "cookie-value" + i,
      hostNonPartitioned,
      "/", // path
      now, // expiry
      now, // last accessed
      now, // creation time
      false, // session
      false, // secure
      false, // http-only
      false, // inBrowserElement
      {} // OA
    );
    schema13db.insertCookie(cookie);
  }

  // add some partitioned cookies
  let nPartitioned = 5;
  let hostPartitioned = "host-partitioned.com";
  for (let i = 0; i < nPartitioned; i++) {
    let cookie = new Cookie(
      "cookie-name" + i,
      "cookie-value" + i,
      hostPartitioned,
      "/", // path
      now, // expiry
      now, // last accessed
      now, // creation time
      false, // session
      false, // secure
      false, // http-only
      false, // inBrowserElement
      { partitionKey: "(https,example.com)" }
    );
    schema13db.insertCookie(cookie);
  }

  Assert.equal(do_count_cookies_in_db(schema13db.db), 10);

  // Startup the cookie service and check the cookie counts by OA
  let cookieCountNonPart =
    Services.cookies.countCookiesFromHost(hostNonPartitioned); // includes expired cookies
  Assert.equal(cookieCountNonPart, nUnpartitioned);
  let cookieCountPart = Services.cookies.getCookiesFromHost(hostPartitioned, {
    partitionKey: "(https,example.com)",
  }).length; // includes expired cookies
  Assert.equal(cookieCountPart, nPartitioned);

  // Startup the cookie service and check the cookie counts by isPartitioned (IsPartitioned())
  Assert.equal(count_IsPartitioned(false), nUnpartitioned);
  Assert.equal(count_IsPartitioned(true), nPartitioned);

  schema13db.close();
});
