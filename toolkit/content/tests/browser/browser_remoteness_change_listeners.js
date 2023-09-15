/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that adding progress listeners to a browser doesn't break things
 * when switching the remoteness of that browser.
 */
add_task(async function test_remoteness_switch_listeners() {
  await BrowserTestUtils.withNewTab("about:support", async function (browser) {
    let wpl;
    let navigated = new Promise(resolve => {
      wpl = {
        onLocationChange() {
          is(browser.currentURI.spec, "https://example.com/");
          if (browser.currentURI?.spec == "https://example.com/") {
            resolve();
          }
        },
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsISupportsWeakReference,
          Ci.nsIWebProgressListener2,
          Ci.nsIWebProgressListener,
        ]),
      };
      browser.addProgressListener(wpl);
    });

    let loaded = BrowserTestUtils.browserLoaded(
      browser,
      null,
      "https://example.com/"
    );
    BrowserTestUtils.startLoadingURIString(browser, "https://example.com/");
    await Promise.all([loaded, navigated]);
    browser.removeProgressListener(wpl);
  });
});
