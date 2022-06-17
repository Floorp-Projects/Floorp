/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the various ways opening a cookie database can fail in an asynchronous
// (i.e. after synchronous initialization) manner, and that the database is
// renamed and recreated under each circumstance. These circumstances are, in no
// particular order:
//
// 1) A write operation failing after the database has been read in.
// 2) Asynchronous read failure due to a corrupt database.
// 3) Synchronous read failure due to a corrupt database, when reading:
//    a) a single base domain;
//    b) the entire database.
// 4) Asynchronous read failure, followed by another failure during INSERT but
//    before the database closes for rebuilding. (The additional error should be
//    ignored.)
// 5) Asynchronous read failure, followed by an INSERT failure during rebuild.
//    This should result in an abort of the database rebuild; the partially-
//    built database should be moved to 'cookies.sqlite.bak-rebuild'.

"use strict";

let profile;
let cookie;

add_task(async () => {
  // Set up a profile.
  profile = do_get_profile();
  Services.prefs.setBoolPref("dom.security.https_first", false);

  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  // Bug 1617611 - Fix all the tests broken by "cookies SameSite=Lax by default"
  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);

  // The server.
  const hosts = ["foo.com", "hither.com", "haithur.com", "bar.com"];
  for (let i = 0; i < 3000; ++i) {
    hosts.push(i + ".com");
  }
  CookieXPCShellUtils.createServer({ hosts });

  // Get the cookie file and the backup file.
  Assert.ok(!do_get_cookie_file(profile).exists());
  Assert.ok(!do_get_backup_file(profile).exists());

  // Create a cookie object for testing.
  let now = Date.now() * 1000;
  let futureExpiry = Math.round(now / 1e6 + 1000);
  cookie = new Cookie(
    "oh",
    "hai",
    "bar.com",
    "/",
    futureExpiry,
    now,
    now,
    false,
    false,
    false
  );

  await run_test_1();
  await run_test_2();
  await run_test_3();
  await run_test_4();
  await run_test_5();
  Services.prefs.clearUserPref("dom.security.https_first");
  Services.prefs.clearUserPref("network.cookie.sameSite.laxByDefault");
});

function do_get_backup_file(profile) {
  let file = profile.clone();
  file.append("cookies.sqlite.bak");
  return file;
}

function do_get_rebuild_backup_file(profile) {
  let file = profile.clone();
  file.append("cookies.sqlite.bak-rebuild");
  return file;
}

function do_corrupt_db(file) {
  // Sanity check: the database size should be larger than 320k, since we've
  // written about 460k of data. If it's not, let's make it obvious now.
  let size = file.fileSize;
  Assert.ok(size > 320e3);

  // Corrupt the database by writing bad data to the end of the file. We
  // assume that the important metadata -- table structure etc -- is stored
  // elsewhere, and that doing this will not cause synchronous failure when
  // initializing the database connection. This is totally empirical --
  // overwriting between 1k and 100k of live data seems to work. (Note that the
  // database file will be larger than the actual content requires, since the
  // cookie service uses a large growth increment. So we calculate the offset
  // based on the expected size of the content, not just the file size.)
  let ostream = Cc["@mozilla.org/network/file-output-stream;1"].createInstance(
    Ci.nsIFileOutputStream
  );
  ostream.init(file, 2, -1, 0);
  let sstream = ostream.QueryInterface(Ci.nsISeekableStream);
  let n = size - 320e3 + 20e3;
  sstream.seek(Ci.nsISeekableStream.NS_SEEK_SET, size - n);
  for (let i = 0; i < n; ++i) {
    ostream.write("a", 1);
  }
  ostream.flush();
  ostream.close();

  Assert.equal(file.clone().fileSize, size);
  return size;
}

async function run_test_1() {
  // Load the profile and populate it.
  await CookieXPCShellUtils.setCookieToDocument(
    "http://foo.com/",
    "oh=hai; max-age=1000"
  );

  // Close the profile.
  await promise_close_profile();

  // Open a database connection now, before we load the profile and begin
  // asynchronous write operations.
  let db = new CookieDatabaseConnection(do_get_cookie_file(profile), 12);
  Assert.equal(do_count_cookies_in_db(db.db), 1);

  // Load the profile, and wait for async read completion...
  await promise_load_profile();

  // Insert a row.
  db.insertCookie(cookie);
  db.close();

  // Attempt to insert a cookie with the same (name, host, path) triplet.
  Services.cookies.add(
    cookie.host,
    cookie.path,
    cookie.name,
    "hallo",
    cookie.isSecure,
    cookie.isHttpOnly,
    cookie.isSession,
    cookie.expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );

  // Check that the cookie service accepted the new cookie.
  Assert.equal(Services.cookies.countCookiesFromHost(cookie.host), 1);

  let isRebuildingDone = false;
  let rebuildingObserve = function(subject, topic, data) {
    isRebuildingDone = true;
    Services.obs.removeObserver(rebuildingObserve, "cookie-db-rebuilding");
  };
  Services.obs.addObserver(rebuildingObserve, "cookie-db-rebuilding");

  // Crash test: we're going to rebuild the cookie database. Close all the db
  // connections in the main thread and initialize a new database file in the
  // cookie thread. Trigger some access of cookies to ensure we won't crash in
  // the chaos status.
  for (let i = 0; i < 10; ++i) {
    Assert.equal(Services.cookies.countCookiesFromHost(cookie.host), 1);
    await new Promise(resolve => executeSoon(resolve));
  }

  // Wait for the cookie service to rename the old database and rebuild if not yet.
  if (!isRebuildingDone) {
    Services.obs.removeObserver(rebuildingObserve, "cookie-db-rebuilding");
    await new _promise_observer("cookie-db-rebuilding");
  }

  await new Promise(resolve => executeSoon(resolve));

  // At this point, the cookies should still be in memory.
  Assert.equal(Services.cookies.countCookiesFromHost("foo.com"), 1);
  Assert.equal(Services.cookies.countCookiesFromHost(cookie.host), 1);
  Assert.equal(do_count_cookies(), 2);

  // Close the profile.
  await promise_close_profile();

  // Check that the original database was renamed, and that it contains the
  // original cookie.
  Assert.ok(do_get_backup_file(profile).exists());
  let backupdb = Services.storage.openDatabase(do_get_backup_file(profile));
  Assert.equal(do_count_cookies_in_db(backupdb, "foo.com"), 1);
  backupdb.close();

  // Load the profile, and check that it contains the new cookie.
  do_load_profile();

  Assert.equal(Services.cookies.countCookiesFromHost("foo.com"), 1);
  let cookies = Services.cookies.getCookiesFromHost(cookie.host, {});
  Assert.equal(cookies.length, 1);
  let dbcookie = cookies[0];
  Assert.equal(dbcookie.value, "hallo");

  // Close the profile.
  await promise_close_profile();

  // Clean up.
  do_get_cookie_file(profile).remove(false);
  do_get_backup_file(profile).remove(false);
  Assert.ok(!do_get_cookie_file(profile).exists());
  Assert.ok(!do_get_backup_file(profile).exists());
}

async function run_test_2() {
  // Load the profile and populate it.
  do_load_profile();

  Services.cookies.runInTransaction(_ => {
    let uri = NetUtil.newURI("http://foo.com/");
    const channel = NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    });

    for (let i = 0; i < 3000; ++i) {
      let uri = NetUtil.newURI("http://" + i + ".com/");
      Services.cookies.setCookieStringFromHttp(
        uri,
        "oh=hai; max-age=1000",
        channel
      );
    }
  });

  // Close the profile.
  await promise_close_profile();

  // Corrupt the database file.
  let size = do_corrupt_db(do_get_cookie_file(profile));

  // Load the profile.
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  Assert.ok(!do_get_backup_file(profile).exists());

  // Recreate a new database since it was corrupted
  Assert.equal(Services.cookies.countCookiesFromHost("0.com"), 0);
  Assert.equal(do_count_cookies(), 0);

  // Close the profile.
  await promise_close_profile();

  // Check that the original database was renamed.
  Assert.ok(do_get_backup_file(profile).exists());
  Assert.equal(do_get_backup_file(profile).fileSize, size);
  let db = Services.storage.openDatabase(do_get_cookie_file(profile));
  db.close();

  do_load_profile();
  Assert.equal(Services.cookies.countCookiesFromHost("0.com"), 0);
  Assert.equal(do_count_cookies(), 0);

  // Close the profile.
  await promise_close_profile();

  // Clean up.
  do_get_cookie_file(profile).remove(false);
  do_get_backup_file(profile).remove(false);
  Assert.ok(!do_get_cookie_file(profile).exists());
  Assert.ok(!do_get_backup_file(profile).exists());
}

async function run_test_3() {
  // Set the maximum cookies per base domain limit to a large value, so that
  // corrupting the database is easier.
  Services.prefs.setIntPref("network.cookie.maxPerHost", 3000);

  // Load the profile and populate it.
  do_load_profile();
  Services.cookies.runInTransaction(_ => {
    let uri = NetUtil.newURI("http://hither.com/");
    let channel = NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    });
    for (let i = 0; i < 10; ++i) {
      Services.cookies.setCookieStringFromHttp(
        uri,
        "oh" + i + "=hai; max-age=1000",
        channel
      );
    }
    uri = NetUtil.newURI("http://haithur.com/");
    channel = NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    });
    for (let i = 10; i < 3000; ++i) {
      Services.cookies.setCookieStringFromHttp(
        uri,
        "oh" + i + "=hai; max-age=1000",
        channel
      );
    }
  });

  // Close the profile.
  await promise_close_profile();

  // Corrupt the database file.
  let size = do_corrupt_db(do_get_cookie_file(profile));

  // Load the profile.
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  Assert.ok(!do_get_backup_file(profile).exists());

  // Recreate a new database since it was corrupted
  Assert.equal(Services.cookies.countCookiesFromHost("hither.com"), 0);
  Assert.equal(Services.cookies.countCookiesFromHost("haithur.com"), 0);

  // Close the profile.
  await promise_close_profile();

  let db = Services.storage.openDatabase(do_get_cookie_file(profile));
  Assert.equal(do_count_cookies_in_db(db, "hither.com"), 0);
  Assert.equal(do_count_cookies_in_db(db), 0);
  db.close();

  // Check that the original database was renamed.
  Assert.ok(do_get_backup_file(profile).exists());
  Assert.equal(do_get_backup_file(profile).fileSize, size);

  // Rename it back, and try loading the entire database synchronously.
  do_get_backup_file(profile).moveTo(null, "cookies.sqlite");
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  Assert.ok(!do_get_backup_file(profile).exists());

  // Synchronously read in everything.
  Assert.equal(do_count_cookies(), 0);

  // Close the profile.
  await promise_close_profile();

  db = Services.storage.openDatabase(do_get_cookie_file(profile));
  Assert.equal(do_count_cookies_in_db(db), 0);
  db.close();

  // Check that the original database was renamed.
  Assert.ok(do_get_backup_file(profile).exists());
  Assert.equal(do_get_backup_file(profile).fileSize, size);

  // Clean up.
  do_get_cookie_file(profile).remove(false);
  do_get_backup_file(profile).remove(false);
  Assert.ok(!do_get_cookie_file(profile).exists());
  Assert.ok(!do_get_backup_file(profile).exists());
}

async function run_test_4() {
  // Load the profile and populate it.
  do_load_profile();
  Services.cookies.runInTransaction(_ => {
    let uri = NetUtil.newURI("http://foo.com/");
    let channel = NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    });
    for (let i = 0; i < 3000; ++i) {
      let uri = NetUtil.newURI("http://" + i + ".com/");
      Services.cookies.setCookieStringFromHttp(
        uri,
        "oh=hai; max-age=1000",
        channel
      );
    }
  });

  // Close the profile.
  await promise_close_profile();

  // Corrupt the database file.
  let size = do_corrupt_db(do_get_cookie_file(profile));

  // Load the profile.
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  Assert.ok(!do_get_backup_file(profile).exists());

  // Recreate a new database since it was corrupted
  Assert.equal(Services.cookies.countCookiesFromHost("0.com"), 0);

  // Queue up an INSERT for the same base domain. This should also go into
  // memory and be written out during database rebuild.
  await CookieXPCShellUtils.setCookieToDocument(
    "http://0.com/",
    "oh2=hai; max-age=1000"
  );

  // At this point, the cookies should still be in memory.
  Assert.equal(Services.cookies.countCookiesFromHost("0.com"), 1);
  Assert.equal(do_count_cookies(), 1);

  // Close the profile.
  await promise_close_profile();

  // Check that the original database was renamed.
  Assert.ok(do_get_backup_file(profile).exists());
  Assert.equal(do_get_backup_file(profile).fileSize, size);

  // Load the profile, and check that it contains the new cookie.
  do_load_profile();
  Assert.equal(Services.cookies.countCookiesFromHost("0.com"), 1);
  Assert.equal(do_count_cookies(), 1);

  // Close the profile.
  await promise_close_profile();

  // Clean up.
  do_get_cookie_file(profile).remove(false);
  do_get_backup_file(profile).remove(false);
  Assert.ok(!do_get_cookie_file(profile).exists());
  Assert.ok(!do_get_backup_file(profile).exists());
}

async function run_test_5() {
  // Load the profile and populate it.
  do_load_profile();
  Services.cookies.runInTransaction(_ => {
    let uri = NetUtil.newURI("http://bar.com/");
    const channel = NetUtil.newChannel({
      uri,
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    });
    Services.cookies.setCookieStringFromHttp(
      uri,
      "oh=hai; path=/; max-age=1000",
      channel
    );
    for (let i = 0; i < 3000; ++i) {
      let uri = NetUtil.newURI("http://" + i + ".com/");
      Services.cookies.setCookieStringFromHttp(
        uri,
        "oh=hai; max-age=1000",
        channel
      );
    }
  });

  // Close the profile.
  await promise_close_profile();

  // Corrupt the database file.
  let size = do_corrupt_db(do_get_cookie_file(profile));

  // Load the profile.
  do_load_profile();

  // At this point, the database connection should be open. Ensure that it
  // succeeded.
  Assert.ok(!do_get_backup_file(profile).exists());

  // Recreate a new database since it was corrupted
  Assert.equal(Services.cookies.countCookiesFromHost("bar.com"), 0);
  Assert.equal(Services.cookies.countCookiesFromHost("0.com"), 0);
  Assert.equal(do_count_cookies(), 0);
  Assert.ok(do_get_backup_file(profile).exists());
  Assert.equal(do_get_backup_file(profile).fileSize, size);
  Assert.ok(!do_get_rebuild_backup_file(profile).exists());

  // Open a database connection, and write a row that will trigger a constraint
  // violation.
  let db = new CookieDatabaseConnection(do_get_cookie_file(profile), 12);
  db.insertCookie(cookie);
  Assert.equal(do_count_cookies_in_db(db.db, "bar.com"), 1);
  Assert.equal(do_count_cookies_in_db(db.db), 1);
  db.close();

  // Check that the original backup and the database itself are gone.
  Assert.ok(do_get_backup_file(profile).exists());
  Assert.equal(do_get_backup_file(profile).fileSize, size);

  Assert.equal(Services.cookies.countCookiesFromHost("bar.com"), 0);
  Assert.equal(Services.cookies.countCookiesFromHost("0.com"), 0);
  Assert.equal(do_count_cookies(), 0);

  // Close the profile. We do not need to wait for completion, because the
  // database has already been closed. Ensure the cookie file is unlocked.
  await promise_close_profile();

  // Clean up.
  do_get_cookie_file(profile).remove(false);
  do_get_backup_file(profile).remove(false);
  Assert.ok(!do_get_cookie_file(profile).exists());
  Assert.ok(!do_get_backup_file(profile).exists());
}
