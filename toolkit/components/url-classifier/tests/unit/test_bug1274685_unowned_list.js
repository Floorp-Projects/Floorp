const { SafeBrowsing } = ChromeUtils.import(
  "resource://gre/modules/SafeBrowsing.jsm"
);
const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);

// 'Cc["@mozilla.org/xre/app-info;1"]' for xpcshell has no nsIXULAppInfo
// so that we have to update it to make nsURLFormatter.js happy.
// (SafeBrowsing.init() will indirectly use nsURLFormatter.js)
updateAppInfo();

function run_test() {
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
  } catch (e) {
    ok(false, "Exception thrown due to " + e.toString());
  }
  Services.prefs.clearUserPref("browser.safebrowsing.provider.google.lists");
  Services.prefs.clearUserPref("browser.safebrowsing.provider.google4.lists");
}
