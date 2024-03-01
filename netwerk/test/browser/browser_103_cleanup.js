/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.prefs.setBoolPref("network.early-hints.enabled", true);

add_task(async function test_103_cancel_parent_connect() {
  Services.prefs.setIntPref("network.early-hints.parent-connect-timeout", 1);

  let callback;
  let promise = new Promise(resolve => {
    callback = resolve;
  });
  let observed_cancel_reason = "";
  let observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(aSubject, aTopic) {
      aSubject = aSubject.QueryInterface(Ci.nsIRequest);
      if (
        aTopic == "http-on-stop-request" &&
        aSubject.name ==
          "https://example.com/browser/netwerk/test/browser/square.png"
      ) {
        observed_cancel_reason = aSubject.canceledReason;
        Services.obs.removeObserver(observer, "http-on-stop-request");
        callback();
      }
    },
  };
  Services.obs.addObserver(observer, "http-on-stop-request");

  // test that no crash or memory leak happens when cancelling before
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com/browser/netwerk/test/browser/103_preload.html",
      waitForLoad: true,
    },
    async function () {}
  );
  await promise;
  Assert.equal(observed_cancel_reason, "parent-connect-timeout");

  Services.prefs.clearUserPref("network.early-hints.parent-connect-timeout");
});
