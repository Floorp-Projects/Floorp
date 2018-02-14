/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function() {
  const TEST_URL = "http://mochi.test:8888/notFoundPage.html";

  // Used to verify errors are not marked as typed.
  PlacesUtils.history.markPageAsTyped(NetUtil.newURI(TEST_URL));

  // Create and add history observer.
  let visitedPromise = new Promise(resolve => {
    function listener(aEvents) {
      is(aEvents.length, 1, "Right number of visits notified");
      is(aEvents[0].type, "page-visited");
      let uri = NetUtil.newURI(aEvents[0].url);
      PlacesObservers.removeListener(["page-visited"], listener);
      info("Received 'page-visited': " + uri.spec);
      fieldForUrl(uri, "frecency", function(aFrecency) {
        is(aFrecency, 0, "Frecency should be 0");
        fieldForUrl(uri, "hidden", function(aHidden) {
          is(aHidden, 0, "Page should not be hidden");
          fieldForUrl(uri, "typed", function(aTyped) {
            is(aTyped, 0, "page should not be marked as typed");
            resolve();
          });
        });
      });
    }
    PlacesObservers.addListener(["page-visited"], listener);
  });

  let newTabPromise = BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  await Promise.all([visitedPromise, newTabPromise]);

  await PlacesUtils.history.clear();
  gBrowser.removeCurrentTab();
});
