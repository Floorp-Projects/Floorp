const { SafeBrowsing } = ChromeUtils.importESModule(
  "resource://gre/modules/SafeBrowsing.sys.mjs"
);
const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

add_setup(async () => {
  // 'Cc["@mozilla.org/xre/app-info;1"]' for xpcshell has no nsIXULAppInfo
  // so that we have to update it to make nsURLFormatter.js happy.
  // (SafeBrowsing.init() will indirectly use nsURLFormatter.js)
  updateAppInfo();

  // This test should not actually try to create a connection to any real
  // endpoint. But a background request could try that while the test is in
  // progress before we've actually shut down networking, and would cause a
  // crash due to connecting to a non-local IP.
  Services.prefs.setCharPref(
    "browser.safebrowsing.provider.mozilla.updateURL",
    `http://localhost:4444/safebrowsing/update`
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(
      "browser.safebrowsing.provider.mozilla.updateURL"
    );
    Services.prefs.clearUserPref("browser.safebrowsing.provider.google.lists");
    Services.prefs.clearUserPref("browser.safebrowsing.provider.google4.lists");
  });
});

add_task(async function test() {
  SafeBrowsing.init();

  let origListV2 = Services.prefs.getCharPref(
    "browser.safebrowsing.provider.google.lists"
  );
  let origListV4 = Services.prefs.getCharPref(
    "browser.safebrowsing.provider.google4.lists"
  );

  // Ensure there's a list missing in both Safe Browsing V2 and V4.
  let trimmedListV2 = origListV2.replace("goog-malware-shavar,", "");
  Services.prefs.setCharPref(
    "browser.safebrowsing.provider.google.lists",
    trimmedListV2
  );
  let trimmedListV4 = origListV4.replace("goog-malware-proto,", "");
  Services.prefs.setCharPref(
    "browser.safebrowsing.provider.google4.lists",
    trimmedListV4
  );

  try {
    // Bug 1274685 - Unowned Safe Browsing tables break list updates
    //
    // If SafeBrowsing.registerTableWithURLs() doesn't check if
    // a provider is found before registering table, an exception
    // will be thrown while accessing a null object.
    //
    SafeBrowsing.registerTables();
    ok(true, "SafeBrowsing.registerTables() did not throw.");
  } catch (e) {
    ok(false, "Exception thrown due to " + e.toString());
  }
});
