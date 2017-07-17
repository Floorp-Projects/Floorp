/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const initialURL =
  "http://example.com/tests/toolkit/components/places/tests/browser/begin.html";
const finalURL =
 "http://example.com/tests/toolkit/components/places/tests/browser/final.html";

var observer;
var visitSavedPromise;

add_task(async function setup() {
  visitSavedPromise = new Promise(resolve => {
    observer = {
      observe(subject, topic, data) {
        // The uri-visit-saved topic should only work when on normal mode.
        if (topic == "uri-visit-saved") {
          Services.obs.removeObserver(observer, "uri-visit-saved");

          // The expected visit should be the finalURL because private mode
          // should not register a visit with the initialURL.
          let uri = subject.QueryInterface(Ci.nsIURI);
          resolve(uri.spec);
        }
      }
    };
  });

  Services.obs.addObserver(observer, "uri-visit-saved");

  registerCleanupFunction(async function() {
    await PlacesTestUtils.clearHistory()
  });
});

// Note: The private window test must be the first one to run, since we'll listen
// to the first uri-visit-saved notification, and we expect this test to not
// fire any, so we'll just find the non-private window test notification.
add_task(async function test_private_browsing_window() {
  await testLoadInWindow({private: true}, initialURL);
});

add_task(async function test_normal_window() {
  await testLoadInWindow({private: false}, finalURL);

  let url = await visitSavedPromise;
  Assert.equal(url, finalURL, "Check received expected visit");
});

async function testLoadInWindow(options, url) {
  let win = await BrowserTestUtils.openNewBrowserWindow(options);

  registerCleanupFunction(async function() {
    await BrowserTestUtils.closeWindow(win);
  });

  let loadedPromise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  await BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, url);
  await loadedPromise;
}
