Cu.import("resource://gre/modules/SafeBrowsing.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/AppInfo.jsm");

// 'Cc["@mozilla.org/xre/app-info;1"]' for xpcshell has no nsIXULAppInfo
// so that we have to update it to make nsURLFormatter.js happy.
// (SafeBrowsing.init() will indirectly use nsURLFormatter.js)
updateAppInfo();

function run_test() {
  SafeBrowsing.init();

  let origList = Services.prefs.getCharPref("browser.safebrowsing.provider.google.lists");

  // Remove 'goog-malware-shavar' from the original.
  let trimmedList = origList.replace('goog-malware-shavar,', '');
  Services.prefs.setCharPref("browser.safebrowsing.provider.google.lists", trimmedList);

  try {
    // Bug 1274685 - Unowned Safe Browsing tables break list updates
    //
    // If SafeBrowsing.registerTableWithURLs() doesn't check if
    // a provider is found before registering table, an exception
    // will be thrown while accessing a null object.
    //
    SafeBrowsing.registerTables();
  } catch (e) {
    ok(false, 'Exception thrown due to ' + e.toString());
  }

  Services.prefs.setCharPref("browser.safebrowsing.provider.google.lists", origList);
}
