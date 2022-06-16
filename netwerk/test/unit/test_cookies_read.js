/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// test cookie database asynchronous read operation.

"use strict";

var CMAX = 1000; // # of cookies to create

add_task(async () => {
  // Set up a profile.
  let profile = do_get_profile();

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  // Bug 1617611 - Fix all the tests broken by "cookies SameSite=Lax by default"
  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);

  // Start the cookieservice, to force creation of a database.
  // Get the sessionCookies to join the initialization in cookie thread
  Services.cookies.sessionCookies;

  // Open a database connection now, after synchronous initialization has
  // completed. We may not be able to open one later once asynchronous writing
  // begins.
  Assert.ok(do_get_cookie_file(profile).exists());
  let db = new CookieDatabaseConnection(do_get_cookie_file(profile), 12);

  let uri = NetUtil.newURI("http://foo.com/");
  let channel = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });
  for (let i = 0; i < CMAX; ++i) {
    let uri = NetUtil.newURI("http://" + i + ".com/");
    Services.cookies.setCookieStringFromHttp(
      uri,
      "oh=hai; max-age=1000",
      channel
    );
  }

  Assert.equal(do_count_cookies(), CMAX);

  // Wait until all CMAX cookies have been written out to the database.
  while (do_count_cookies_in_db(db.db) < CMAX) {
    await new Promise(resolve => executeSoon(resolve));
  }

  // Check the WAL file size. We set it to 16 pages of 32k, which means it
  // should be around 500k.
  let file = db.db.databaseFile;
  Assert.ok(file.exists());
  Assert.ok(file.fileSize < 1e6);
  db.close();

  // fake a profile change
  await promise_close_profile();
  do_load_profile();

  // test a few random cookies
  Assert.equal(Services.cookies.countCookiesFromHost("999.com"), 1);
  Assert.equal(Services.cookies.countCookiesFromHost("abc.com"), 0);
  Assert.equal(Services.cookies.countCookiesFromHost("100.com"), 1);
  Assert.equal(Services.cookies.countCookiesFromHost("400.com"), 1);
  Assert.equal(Services.cookies.countCookiesFromHost("xyz.com"), 0);

  // force synchronous load of everything
  Assert.equal(do_count_cookies(), CMAX);

  // check that everything's precisely correct
  for (let i = 0; i < CMAX; ++i) {
    let host = i.toString() + ".com";
    Assert.equal(Services.cookies.countCookiesFromHost(host), 1);
  }

  // reload again, to make sure the additions were written correctly
  await promise_close_profile();
  do_load_profile();

  // remove some of the cookies, in both reverse and forward order
  for (let i = 100; i-- > 0; ) {
    let host = i.toString() + ".com";
    Services.cookies.remove(host, "oh", "/", {});
  }
  for (let i = CMAX - 100; i < CMAX; ++i) {
    let host = i.toString() + ".com";
    Services.cookies.remove(host, "oh", "/", {});
  }

  // check the count
  Assert.equal(do_count_cookies(), CMAX - 200);

  // reload again, to make sure the removals were written correctly
  await promise_close_profile();
  do_load_profile();

  // check the count
  Assert.equal(do_count_cookies(), CMAX - 200);

  // reload again, but wait for async read completion
  await promise_close_profile();
  await promise_load_profile();

  // check that everything's precisely correct
  Assert.equal(do_count_cookies(), CMAX - 200);
  for (let i = 100; i < CMAX - 100; ++i) {
    let host = i.toString() + ".com";
    Assert.equal(Services.cookies.countCookiesFromHost(host), 1);
  }

  Services.prefs.clearUserPref("network.cookie.sameSite.laxByDefault");
});
