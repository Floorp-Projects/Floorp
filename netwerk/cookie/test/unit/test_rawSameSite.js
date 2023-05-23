const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

add_task(async _ => {
  do_get_profile();

  let dbFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
  dbFile.append("cookies.sqlite");

  let storage = Services.storage;
  let properties = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
    Ci.nsIWritablePropertyBag
  );
  properties.setProperty("shared", true);
  let conn = storage.openDatabase(dbFile);

  conn.schemaVersion = 9;
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
      "CONSTRAINT moz_uniqueid UNIQUE (name, host, path, originAttributes)" +
      ")"
  );
  conn.close();

  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess()) {
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
    Services.prefs.setBoolPref(
      "network.cookieJarSettings.unblocked_for_testing",
      true
    );
    Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", true);
    Services.prefs.setBoolPref(
      "network.cookie.sameSite.noneRequiresSecure",
      true
    );
  }

  let uri = NetUtil.newURI("http://example.org/");

  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  let channel = NetUtil.newChannel({
    uri,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });

  let tests = [
    {
      cookie: "foo=b;max-age=3600, c=d;path=/; sameSite=strict",
      sameSite: 2,
      rawSameSite: 2,
    },
    {
      cookie: "foo=b;max-age=3600, c=d;path=/; sameSite=lax",
      sameSite: 1,
      rawSameSite: 1,
    },
    { cookie: "foo=b;max-age=3600, c=d;path=/", sameSite: 1, rawSameSite: 0 },
  ];

  for (let i = 0; i < tests.length; ++i) {
    let test = tests[i];

    let promise = new Promise(resolve => {
      function observer(subject, topic, data) {
        Services.obs.removeObserver(observer, "cookie-saved-on-disk");
        resolve();
      }

      Services.obs.addObserver(observer, "cookie-saved-on-disk");
    });

    Services.cookies.setCookieStringFromHttp(uri, test.cookie, channel);

    await promise;

    conn = storage.openDatabase(dbFile);
    Assert.equal(conn.schemaVersion, 12);

    let stmt = conn.createStatement(
      "SELECT sameSite, rawSameSite FROM moz_cookies"
    );

    let success = stmt.executeStep();
    Assert.ok(success);

    let sameSite = stmt.getInt32(0);
    let rawSameSite = stmt.getInt32(1);
    stmt.finalize();

    Assert.equal(sameSite, test.sameSite);
    Assert.equal(rawSameSite, test.rawSameSite);

    Services.cookies.removeAll();

    stmt.finalize();
    conn.close();
  }
});
