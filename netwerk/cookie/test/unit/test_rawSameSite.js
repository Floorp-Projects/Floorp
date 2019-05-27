const {NetUtil} = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

function run_test() {
  do_get_profile();

  let dbFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
  dbFile.append("cookies.sqlite");

  let storage = Services.storage;
  let properties = Cc["@mozilla.org/hash-property-bag;1"].
                     createInstance(Ci.nsIWritablePropertyBag);
  properties.setProperty("shared", true);
  let conn = storage.openDatabase(dbFile);

  conn.schemaVersion = 9;
  conn.executeSimpleSQL("DROP TABLE IF EXISTS moz_cookies");
  conn.executeSimpleSQL("CREATE TABLE moz_cookies (" +
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
      ")");

  // Allow all cookies if the pref service is available in this process.
  if (!inChildProcess()) {
    Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
    Services.prefs.setBoolPref("network.cookieSettings.unblocked_for_testing", true);
    Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", true);
    Services.prefs.setBoolPref("network.cookie.sameSite.noneRequiresSecure", true);
  }

  let cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);

  let uri = NetUtil.newURI("http://example.org/");

  let principal = Services.scriptSecurityManager.createCodebasePrincipal(uri, {});

  let channel = NetUtil.newChannel({
    uri,
    loadingPrincipal: principal,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_OTHER,
  });

  let tests = [
    { cookie: "foo=b;max-age=3600, c=d;path=/; sameSite=strict",
      success: true,
      sameSite: 2,
      rawSameSite: 2,
    },
    { cookie: "foo=b;max-age=3600, c=d;path=/; sameSite=lax",
      success: true,
      sameSite: 1,
      rawSameSite: 1,
    },
    { cookie: "foo=b;max-age=3600, c=d;path=/; sameSite=none",
      success: false,
      sameSite: 1,
      rawSameSite: 0,
    },
    { cookie: "foo=b;max-age=3600, c=d;path=/",
      success: true,
      sameSite: 1,
      rawSameSite: 0,
    },
  ];

  tests.forEach(test => {
    cs.setCookieStringFromHttp(uri, null, null, test.cookie, null, channel);

    let dbFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
    dbFile.append("cookies.sqlite");

    let storage = Services.storage;
    let properties = Cc["@mozilla.org/hash-property-bag;1"].
                     createInstance(Ci.nsIWritablePropertyBag);
    properties.setProperty("shared", true);
    let conn = storage.openDatabase(dbFile);

    Assert.equal(conn.schemaVersion, 10);

    let stmt = conn.createStatement("SELECT sameSite, rawSameSite FROM moz_cookies");

    let success = stmt.executeStep();
    Assert.equal(success, test.success);

    if (test.success) {
      let sameSite = stmt.getInt32(0);
      let rawSameSite = stmt.getInt32(1);
      stmt.finalize();

      Assert.equal(sameSite, test.sameSite);
      Assert.equal(rawSameSite, test.rawSameSite);
    }

    Services.cookies.removeAll();

    stmt.finalize();
    conn.close();
  });
}
