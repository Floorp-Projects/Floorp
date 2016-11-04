/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the normal operation of setAndFetchFaviconForPage.
function test() {
  // Initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let testURI = "https://www.mozilla.org/en-US/";
  let favIconLocation =
    "http://example.org/tests/toolkit/components/places/tests/browser/favicon-normal32.png";
  let favIconURI = NetUtil.newURI(favIconLocation);
  let favIconMimeType= "image/png";
  let pageURI;
  let favIconData;

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      windowsToClose.push(aWin);
      executeSoon(() => aCallback(aWin));
    });
  }

  // This function is called after calling finish() on the test.
  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
  });

  function getIconFile(aCallback) {
    NetUtil.asyncFetch({
      uri: favIconLocation,
      loadUsingSystemPrincipal: true,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE_FAVICON
    }, function(inputStream, status) {
        if (!Components.isSuccessCode(status)) {
          ok(false, "Could not get the icon file");
          // Handle error.
          return;
        }

        // Check the returned size versus the expected size.
        let size = inputStream.available();
        favIconData = NetUtil.readInputStreamToString(inputStream, size);
        is(size, favIconData.length, "Check correct icon size");
        // Check that the favicon loaded correctly before starting the actual tests.
        is(favIconData.length, 344, "Check correct icon length (344)");

        if (aCallback) {
          aCallback();
        } else {
          finish();
        }
      });
  }

  function testNormal(aWindow, aCallback) {
    pageURI = NetUtil.newURI("http://example.com/normal");
    waitForFaviconChanged(pageURI, favIconURI, aWindow,
      function testNormalCallback() {
        checkFaviconDataForPage(pageURI, favIconMimeType, favIconData, aWindow,
          aCallback);
      }
    );

    addVisits({uri: pageURI, transition: TRANSITION_TYPED}, aWindow,
      function () {
        aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, favIconURI,
          true, aWindow.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE, null,
          Services.scriptSecurityManager.getSystemPrincipal());
      }
    );
  }

  function testAboutURIBookmarked(aWindow, aCallback) {
    pageURI = NetUtil.newURI("about:testAboutURI_bookmarked");
    waitForFaviconChanged(pageURI, favIconURI, aWindow,
      function testAboutURIBookmarkedCallback() {
        checkFaviconDataForPage(pageURI, favIconMimeType, favIconData, aWindow,
          aCallback);
      }
    );

    aWindow.PlacesUtils.bookmarks.insertBookmark(
      aWindow.PlacesUtils.unfiledBookmarksFolderId, pageURI,
      aWindow.PlacesUtils.bookmarks.DEFAULT_INDEX, pageURI.spec);
    aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, favIconURI,
      true, aWindow.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE, null,
      Services.scriptSecurityManager.getSystemPrincipal());
  }

  function testPrivateBrowsingBookmarked(aWindow, aCallback) {
    pageURI = NetUtil.newURI("http://example.com/privateBrowsing_bookmarked");
    waitForFaviconChanged(pageURI, favIconURI, aWindow,
      function testPrivateBrowsingBookmarkedCallback() {
        checkFaviconDataForPage(pageURI, favIconMimeType, favIconData, aWindow,
          aCallback);
      }
    );

    aWindow.PlacesUtils.bookmarks.insertBookmark(
      aWindow.PlacesUtils.unfiledBookmarksFolderId, pageURI,
      aWindow.PlacesUtils.bookmarks.DEFAULT_INDEX, pageURI.spec);
    aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, favIconURI,
      true, aWindow.PlacesUtils.favicons.FAVICON_LOAD_PRIVATE, null,
      Services.scriptSecurityManager.getSystemPrincipal());
  }

  function testDisabledHistoryBookmarked(aWindow, aCallback) {
    pageURI = NetUtil.newURI("http://example.com/disabledHistory_bookmarked");
    waitForFaviconChanged(pageURI, favIconURI, aWindow,
      function testDisabledHistoryBookmarkedCallback() {
        checkFaviconDataForPage(pageURI, favIconMimeType, favIconData, aWindow,
          aCallback);
      }
    );

    // Disable history while changing the favicon.
    aWindow.Services.prefs.setBoolPref("places.history.enabled", false);

    aWindow.PlacesUtils.bookmarks.insertBookmark(
      aWindow.PlacesUtils.unfiledBookmarksFolderId, pageURI,
      aWindow.PlacesUtils.bookmarks.DEFAULT_INDEX, pageURI.spec);
    aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, favIconURI,
      true, aWindow.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE, null,
      Services.scriptSecurityManager.getSystemPrincipal());

    // The setAndFetchFaviconForPage function calls CanAddURI synchronously, thus
    // we can set the preference back to true immediately.  We don't clear the
    // preference because not all products enable Places by default.
    aWindow.Services.prefs.setBoolPref("places.history.enabled", true);
  }

  getIconFile(function () {
    testOnWindow({}, function(aWin) {
      testNormal(aWin, function () {
        testOnWindow({}, function(aWin2) {
          testAboutURIBookmarked(aWin2, function () {
            testOnWindow({private: true}, function(aWin3) {
              testPrivateBrowsingBookmarked(aWin3, function () {
                testOnWindow({}, function(aWin4) {
                  testDisabledHistoryBookmarked(aWin4, finish);
                });
              });
            });
          });
        });
      });
    });
  });
}
