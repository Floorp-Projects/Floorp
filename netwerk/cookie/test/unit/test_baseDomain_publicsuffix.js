"use strict";

add_task(async () => {
  const HOST = "www.bbc.co.uk";
  Assert.equal(
    Services.eTLD.getBaseDomainFromHost(HOST),
    "bbc.co.uk",
    "Sanity check: HOST is an eTLD + 1 with subdomain"
  );

  const tests = [
    {
      // Correct baseDomain: eTLD + 1.
      baseDomain: "bbc.co.uk",
      name: "originally_bbc_co_uk",
    },
    {
      // Incorrect baseDomain: Part of public suffix list.
      baseDomain: "uk",
      name: "originally_uk",
    },
    {
      // Incorrect baseDomain: Part of public suffix list.
      baseDomain: "co.uk",
      name: "originally_co_uk",
    },
    {
      // Incorrect baseDomain: eTLD + 2.
      baseDomain: "www.bbc.co.uk",
      name: "originally_www_bbc_co_uk",
    },
  ];

  do_get_profile();

  let dbFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
  dbFile.append("cookies.sqlite");
  let conn = Services.storage.openDatabase(dbFile);

  conn.schemaVersion = 10;
  conn.executeSimpleSQL("DROP TABLE IF EXISTS moz_cookies");
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
      "inBrowserElement INTEGER DEFAULT 0, " +
      "sameSite INTEGER DEFAULT 0, " +
      "rawSameSite INTEGER DEFAULT 0, " +
      "CONSTRAINT moz_uniqueid UNIQUE (name, host, path, originAttributes)" +
      ")"
  );

  function addCookie(baseDomain, host, name) {
    conn.executeSimpleSQL(
      "INSERT INTO moz_cookies(" +
        "baseDomain, host, name, value, path, expiry, " +
        "lastAccessed, creationTime, isSecure, isHttpOnly) VALUES (" +
        `'${baseDomain}', '${host}', '${name}', 'thevalue', '/', ` +
        (Date.now() + 3600000) +
        "," +
        Date.now() +
        "," +
        Date.now() +
        ", 1, 1)"
    );
  }

  // Prepare the database.
  for (let { baseDomain, name } of tests) {
    addCookie(baseDomain, HOST, name);
  }
  // Domain cookies are not supported for IP addresses.
  addCookie("127.0.0.1", ".127.0.0.1", "invalid_host");
  conn.close();

  let cs = Services.cookies;

  // Count excludes the invalid_host cookie.
  Assert.equal(cs.cookies.length, tests.length, "Expected number of cookies");

  // Check whether the database has the expected value,
  // despite the incorrect baseDomain.
  for (let { name } of tests) {
    Assert.ok(
      cs.cookieExists(HOST, "/", name, {}),
      "Should find cookie with name: " + name
    );
  }

  Assert.equal(
    cs.cookieExists("127.0.0.1", "/", "invalid_host", {}),
    false,
    "Should ignore database row with invalid host name"
  );
});
