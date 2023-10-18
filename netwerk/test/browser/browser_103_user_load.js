/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// simulate user initiated loads by entering the URL in the URL-bar code based on
// https://searchfox.org/mozilla-central/rev/5644fae86d5122519a0e34ee03117c88c6ed9b47/browser/components/urlbar/tests/browser/browser_enter.js

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

const {
  request_count_checking,
  test_hint_preload_internal,
  test_hint_preload,
} = ChromeUtils.importESModule(
  "resource://testing-common/early_hint_preload_test_helper.sys.mjs"
);

const START_VALUE =
  "https://example.com/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=style&hinted=1";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quickactions", false]],
  });
});

Services.prefs.setBoolPref("network.early-hints.enabled", true);

// bug 1780822
add_task(async function user_initiated_load() {
  // reset the count
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  info("Simple user initiated load");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  // under normal test conditions using the systemPrincipal as loadingPrincipal
  // doesn't elicit a crash, changing the behavior for this test:
  // https://searchfox.org/mozilla-central/rev/5644fae86d5122519a0e34ee03117c88c6ed9b47/dom/security/nsContentSecurityManager.cpp#1149-1150
  Services.prefs.setBoolPref(
    "security.disallow_non_local_systemprincipal_in_tests",
    true
  );

  gURLBar.value = START_VALUE;
  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter");
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  // reset the config option
  Services.prefs.clearUserPref(
    "security.disallow_non_local_systemprincipal_in_tests"
  );

  // Check url bar and selected tab.
  is(
    gURLBar.value,
    UrlbarTestUtils.trimURL(START_VALUE),
    "Urlbar should preserve the value on return keypress"
  );
  is(gBrowser.selectedTab, tab, "New URL was loaded in the current tab");

  let gotRequestCount = await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs"
  ).then(response => response.json());
  let expectedRequestCount = { hinted: 1, normal: 0 };

  await request_count_checking(
    "test_preload_user_initiated",
    gotRequestCount,
    expectedRequestCount
  );

  // Cleanup.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
