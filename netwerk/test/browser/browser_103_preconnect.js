/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Services.prefs.setBoolPref("network.early-hints.enabled", true);
Services.prefs.setBoolPref("network.early-hints.preconnect.enabled", true);
Services.prefs.setBoolPref("network.http.debug-observations", true);
Services.prefs.setIntPref("network.early-hints.preconnect.max_connections", 10);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("network.early-hints.enabled");
  Services.prefs.clearUserPref("network.early-hints.preconnect.enabled");
  Services.prefs.clearUserPref("network.http.debug-observations");
  Services.prefs.clearUserPref(
    "network.early-hints.preconnect.max_connections"
  );
});

// Test steps:
// 1. Load early_hint_preconnect_html.sjs
// 2. In early_hint_preconnect_html.sjs, a 103 response with
//    "rel=preconnect" is returned.
// 3. We use "speculative-connect-request" topic to observe whether the
//    speculative connection is attempted.
// 4. Finally, we check if the observed URL is the same as the expected.
async function test_hint_preconnect(href, crossOrigin) {
  let requestUrl = `https://example.com/browser/netwerk/test/browser/early_hint_preconnect_html.sjs?href=${href}&crossOrigin=${crossOrigin}`;

  let observed = "";
  let observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(aSubject, aTopic, aData) {
      if (aTopic == "speculative-connect-request") {
        Services.obs.removeObserver(observer, "speculative-connect-request");
        observed = aData;
      }
    },
  };
  Services.obs.addObserver(observer, "speculative-connect-request");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: requestUrl,
      waitForLoad: true,
    },
    async function() {}
  );

  if (!crossOrigin) {
    crossOrigin = "anonymous";
  }

  Assert.equal(observed, `${href}/${crossOrigin}`);
}

add_task(async function test_103_preconnect() {
  await test_hint_preconnect("https://localhost", "use-credentials");
  await test_hint_preconnect("https://localhost", "");
  await test_hint_preconnect("https://localhost", "anonymous");
});
