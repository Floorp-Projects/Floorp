/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  const pageURI =
   "http://example.org/tests/toolkit/components/places/tests/browser/favicon.html";
  let windowsToClose = [];

  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
  });

  function testOnWindow(aIsPrivate, aCallback) {
    whenNewWindowLoaded({private: aIsPrivate}, function(aWin) {
      windowsToClose.push(aWin);
      executeSoon(() => aCallback(aWin));
    });
  };

  function waitForTabLoad(aWin, aCallback) {
    aWin.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      aWin.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
      aCallback();
    }, true);
    aWin.gBrowser.selectedBrowser.loadURI(pageURI);
  }

  testOnWindow(true, function(win) {
    waitForTabLoad(win, function() {
      PlacesUtils.favicons.getFaviconURLForPage(NetUtil.newURI(pageURI),
        function(uri, dataLen, data, mimeType) {
          is(uri, null, "No result should be found");
          finish();
        }
      );
    });
  });
}
