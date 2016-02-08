/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  // initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let testURI = "https://www.mozilla.org/en-US/";
  let initialURL =
    "http://example.com/tests/toolkit/components/places/tests/browser/begin.html";
  let finalURL =
    "http://example.com/tests/toolkit/components/places/tests/browser/final.html";
  let observer = null;
  let enumerator = null;
  let currentObserver = null;
  let uri = null;

  function doTest(aIsPrivateMode, aWindow, aTestURI, aCallback) {
    observer = {
      observe: function(aSubject, aTopic, aData) {
        // The uri-visit-saved topic should only work when on normal mode.
        if (aTopic == "uri-visit-saved") {
          // Remove the observers set on per window private mode and normal
          // mode.
          enumerator = aWindow.Services.obs.enumerateObservers("uri-visit-saved");
          while (enumerator.hasMoreElements()) {
            currentObserver = enumerator.getNext();
            aWindow.Services.obs.removeObserver(currentObserver, "uri-visit-saved");
          }

          // The expected visit should be the finalURL because private mode
          // should not register a visit with the initialURL.
          uri = aSubject.QueryInterface(Ci.nsIURI);
          is(uri.spec, finalURL, "Check received expected visit");
        }
      }
    };

    aWindow.Services.obs.addObserver(observer, "uri-visit-saved", false);

    BrowserTestUtils.browserLoaded(aWindow.gBrowser.selectedBrowser).then(aCallback);
    aWindow.gBrowser.selectedBrowser.loadURI(aTestURI);
  }

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      windowsToClose.push(aWin);
      // execute should only be called when need, like when you are opening
      // web pages on the test. If calling executeSoon() is not necesary, then
      // call whenNewWindowLoaded() instead of testOnWindow() on your test.
      executeSoon(() => aCallback(aWin));
    });
  }

   // This function is called after calling finish() on the test.
  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
  });

  // test first when on private mode
  testOnWindow({private: true}, function(aWin) {
    doTest(true, aWin, initialURL, function() {
      // then test when not on private mode
      testOnWindow({}, function(aWin) {
        doTest(false, aWin, finalURL, function () {
          PlacesTestUtils.clearHistory().then(finish);
        });
      });
    });
  });
}
