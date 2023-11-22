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

  // add some non-partitioned cookies for key
  let cookieNum1 = 1;
  let hostNonPartitioned = "cookie-host-non-partitioned.com";
  for (let i = 0; i < cookieNum1; i++) {
    let cookie = new Cookie(
      "cookie-name" + i,
      "cookie-value" + i,
      hostNonPartitioned,
      "/", // path
      now, // expiry
      now, // needed to get the cookie by the db init
      now, // creation time
      false, // session
      false, // secure
      false, // http-only
      false, // inBrowserElement
      {} // OA
    );
    schema12db.insertCookie(cookie);
  }

  // add some non-partitioned cookies different key
  let cookieNum2 = 10;
  for (let i = 0; i < cookieNum2; i++) {
    let cookie = new Cookie(
      "cookie-name" + i,
      "cookie-value" + i,
      hostNonPartitioned,
      "/", // path
      now, // expiry
      now, // needed to get the cookie by the db init
      now, // creation time
      false, // session
      false, // secure
      false, // http-only
      false, // inBrowserElement
      { userContextId: 8 } // OA
    );
    schema12db.insertCookie(cookie);
  }

  // add some partitioned cookies
  let hostPartitioned = "host-partitioned.com";
  let cookieNum3 = 3;
  for (let i = 0; i < cookieNum3; i++) {
    let cookie = new Cookie(
      "cookie-name" + i,
      "cookie-value" + i,
      hostPartitioned,
      "/", // path
      now, // expiry
      now, // needed to get the cookie by the db init
      now, // creation time
      false, // session
      false, // secure
      false, // http-only
      false, // inBrowserElement
      { partitionKey: "(https,example.com)" }
    );
    schema12db.insertCookie(cookie);
  }

  // add some partitioned with different OA
  let cookieNum4 = 35;
  for (let i = 0; i < cookieNum4; i++) {
    let cookie = new Cookie(
      "cookie-name" + i,
      "cookie-value" + i,
      hostPartitioned,
      "/", // path
      now, // expiry
      now, // needed to get the cookie by the db init
      now, // creation time
      false, // session
      false, // secure
      false, // http-only
      false, // inBrowserElement
      { partitionKey: "(https,example.com)", userContextId: 7 }
    );
    schema12db.insertCookie(cookie);
  }

  let allCookieCount = cookieNum1 + cookieNum2 + cookieNum3 + cookieNum4;
  Assert.equal(do_count_cookies_in_db(schema12db.db), allCookieCount);

  // startup the cookie service and check the cookie counts
  let cookieCountNonPart =
    Services.cookies.countCookiesFromHost(hostNonPartitioned); // includes expired cookies
  Assert.equal(cookieCountNonPart, cookieNum1);
  let cookieCountNonPartOA = Services.cookies.getCookiesFromHost(
    hostNonPartitioned,
    { userContextId: 8 }
  ).length; // includes expired cookies
  Assert.equal(cookieCountNonPartOA, cookieNum2);
  let cookieCountPart = Services.cookies.getCookiesFromHost(hostPartitioned, {
    partitionKey: "(https,example.com)",
  }).length; // includes expired cookies
  Assert.equal(cookieCountPart, cookieNum3);
  let cookieCountPartOA = Services.cookies.getCookiesFromHost(hostPartitioned, {
    partitionKey: "(https,example.com)",
    userContextId: 7,
  }).length; // includes expired cookies
  Assert.equal(cookieCountPartOA, cookieNum4);

  // trigger the collection
  Services.obs.notifyObservers(null, "idle-daily");

  // check telem fired for all cookie count
  let cct = await Glean.networking.cookieCountTotal.testGetValue();
  Assert.equal(cct.sum, allCookieCount, "All cookies telem");

  // check telem for all un/partitioned counts
  let ccp = await Glean.networking.cookieCountPartitioned.testGetValue();
  Assert.equal(
    ccp.sum,
    cookieNum3 + cookieNum4,
    "All partitioned cookies telem"
  );

  let ccu = await Glean.networking.cookieCountUnpartitioned.testGetValue();
  Assert.equal(
    ccu.sum,
    cookieNum1 + cookieNum2,
    "All unpartitioned cookies telem"
  );

  // check telem for part by key (host+OA)
  // Note: With the decided histogram layout we see the buckets
  // (used for indexing the histogram's buckets)
  let histPartByKey =
    await Glean.networking.cookieCountPartByKey.testGetValue();
  Assert.equal(histPartByKey.values[2], 1, "Partitioned bucket 2 has 1 value");
  Assert.equal(
    histPartByKey.values[31],
    1,
    "Partitioned bucket 31 has 1 value"
  );
  Assert.equal(
    histPartByKey.sum,
    cookieNum3 + cookieNum4,
    "Partitioned bucket sums correctly"
  );

  // check telem for unpart by key (host+OA)
  let histUnpartByKey =
    await Glean.networking.cookieCountUnpartByKey.testGetValue();
  Assert.equal(
    histUnpartByKey.values[1],
    1,
    "Unpartitioned bucket 1 has 1 value"
  );
  Assert.equal(
    histUnpartByKey.values[8],
    1,
    "Unpartitioned bucket 8 has 1 value"
  );
  Assert.equal(
    histUnpartByKey.sum,
    cookieNum1 + cookieNum2,
    "Unpartitioned bucket sums correctly"
  );

  schema12db.close();
});
