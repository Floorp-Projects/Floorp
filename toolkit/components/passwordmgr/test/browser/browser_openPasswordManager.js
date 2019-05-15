const PREF_MANAGEMENT_URI = "signon.management.overrideURI";

function resetPrefs() {
  Services.prefs.clearUserPref(PREF_MANAGEMENT_URI);
}

registerCleanupFunction(resetPrefs);

add_task(async function test_noFilter() {
  LoginHelper.openPasswordManager(window, { entryPoint: "mainmenu" });
  let win = await waitForPasswordManagerDialog();
  ok(win, "Login dialog was opened");
  await BrowserTestUtils.closeWindow(win);
  await TestUtils.waitForCondition(() => {
    return Services.wm.getMostRecentWindow("Toolkit:PasswordManager") === null;
  }, "Waiting for the password manager dialog to close");
});

add_task(async function test_filter() {
  // Greek IDN for example.test
  let domain = "παράδειγμα.δοκιμή";
  LoginHelper.openPasswordManager(window, { filterString: domain, entryPoint: "mainmenu" });
  let win = await waitForPasswordManagerDialog();
  await TestUtils.waitForCondition(() => {
    return win.document.getElementById("filter").value == domain;
  }, "Waiting for the search string to filter logins");
  ok(win, "Login dialog was opened with a domain filter");
  await BrowserTestUtils.closeWindow(win);
  await TestUtils.waitForCondition(() => {
    return Services.wm.getMostRecentWindow("Toolkit:PasswordManager") === null;
  }, "Waiting for the password manager dialog to close");
});

add_task(async function test_management_overrideURI_noFilter() {
  await SpecialPowers.pushPrefEnv({"set": [
    ["signon.management.page.enabled", true],
  ]});

  Services.prefs.setStringPref(PREF_MANAGEMENT_URI, "about:logins?filter=%DOMAIN%");
  let tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:logins?filter=");
  LoginHelper.openPasswordManager(window, { entryPoint: "mainmenu" });
  let tab = await tabOpenPromise;
  ok(tab, "Got the new tab");
  BrowserTestUtils.removeTab(tab);
  resetPrefs();
});


add_task(async function test_management_overrideURI_filter() {
  Services.prefs.setStringPref(PREF_MANAGEMENT_URI, "about:logins?filter=%DOMAIN%");
  let tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser, "about:logins?filter=%CF%80%CE%B1%CF%81%CE%AC%CE%B4%CE%B5%CE%B9%CE%B3%CE%BC%CE%B1.%CE%B4%CE%BF%CE%BA%CE%B9%CE%BC%CE%AE");
  // Greek IDN for example.test
  LoginHelper.openPasswordManager(window, { filterString: "παράδειγμα.δοκιμή", entryPoint: "mainmenu" });
  let tab = await tabOpenPromise;
  ok(tab, "Got the new tab with a domain filter");
  BrowserTestUtils.removeTab(tab);
  resetPrefs();
});
