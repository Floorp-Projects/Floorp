/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

add_task(async function test_purge_counting() {
  let profile = do_get_profile();
  let dbFile = do_get_cookie_file(profile);
  Assert.ok(!dbFile.exists());

  let schema12db = new CookieDatabaseConnection(dbFile, 12);

  let now = Date.now() * 1000; // date in microseconds
  let pastExpiry = Math.round(now / 1e6 - 1000);
  let futureExpiry = Math.round(now / 1e6 + 1000);

  let manyHosts = 50;
  let manyCookies = 140;
  let totalCookies = manyHosts * manyCookies;

  // add many expired cookies for each host
  for (let hostNum = 0; hostNum < manyHosts; hostNum++) {
    let host = "cookie-host" + hostNum + ".com";
    for (let i = 0; i < manyCookies; i++) {
      let cookie = new Cookie(
        "cookie-name" + i,
        "cookie-value" + i,
        host,
        "/", // path
        pastExpiry,
        pastExpiry, // needed to get the cookie by the db init
        now,
        false,
        false,
        false
      );
      schema12db.insertCookie(cookie);
    }
  }

  let validCookies = Services.cookies.cookies.length; // includes expired cookies
  Assert.equal(validCookies, totalCookies);

  // add a valid cookie - this triggers the purge
  Services.cookies.add(
    "cookie-host0.com", // any host
    "/", // path
    "cookie-name-x",
    "cookie-value-x",
    false, // secure
    true, // http-only
    true, // isSession
    futureExpiry,
    {}, // OA
    Ci.nsICookie.SAMESITE_NONE, // SameSite
    Ci.nsICookie.SCHEME_HTTPS
  );

  // check that we purge all the expired cookies and not the unexpired
  validCookies = Services.cookies.cookies.length;
  Assert.equal(validCookies, 1);

  // check that the telemetry fired
  let cpm = await Glean.networking.cookiePurgeMax.testGetValue();
  Assert.equal(cpm.sum, totalCookies, "Purge the expected number of cookies");

  schema12db.close();
});
