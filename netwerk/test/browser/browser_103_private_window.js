/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);

registerCleanupFunction(function () {
  Services.prefs.clearUserPref("network.early-hints.enabled");
});

// Test steps:
// 1. Load early_hint_asset_html.sjs with a provided uuid.
// 2. In early_hint_asset_html.sjs, a 103 response with
//    a Link header<early_hint_asset.sjs> and the provided uuid is returned.
// 3. We use "http-on-opening-request" topic to observe whether the
//    early hinted request is created.
// 4. Finally, we check if the request has the correct `isPrivate` value.
async function test_early_hints_load_url(usePrivateWin) {
  let headers = new Headers();
  headers.append("X-Early-Hint-Count-Start", "");
  await fetch(
    "https://example.com/browser/netwerk/test/browser/early_hint_pixel_count.sjs",
    { headers }
  );

  // Open a browsing window.
  const win = await BrowserTestUtils.openNewBrowserWindow({
    private: usePrivateWin,
  });

  let id = Services.uuid.generateUUID().toString();
  let expectedUrl = `https://example.com/browser/netwerk/test/browser/early_hint_asset.sjs?as=fetch&uuid=${id}`;
  let observed = {};
  let observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(aSubject, aTopic, aData) {
      if (aTopic == "http-on-opening-request") {
        let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);
        if (channel.URI.spec === expectedUrl) {
          observed.actrualUrl = channel.URI.spec;
          let isPrivate = channel.QueryInterface(
            Ci.nsIPrivateBrowsingChannel
          ).isChannelPrivate;
          observed.isPrivate = isPrivate;
          Services.obs.removeObserver(observer, "http-on-opening-request");
        }
      }
    },
  };
  Services.obs.addObserver(observer, "http-on-opening-request");

  let requestUrl = `https://example.com/browser/netwerk/test/browser/early_hint_asset_html.sjs?as=fetch&hinted=1&uuid=${id}`;

  const browser = win.gBrowser.selectedTab.linkedBrowser;
  let loaded = BrowserTestUtils.browserLoaded(browser, false, requestUrl);
  BrowserTestUtils.startLoadingURIString(browser, requestUrl);
  await loaded;

  Assert.equal(observed.actrualUrl, expectedUrl);
  Assert.equal(observed.isPrivate, usePrivateWin);

  await BrowserTestUtils.closeWindow(win);
}

add_task(async function test_103_private_window() {
  await test_early_hints_load_url(true);
  await test_early_hints_load_url(false);
});
