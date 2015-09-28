/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file tests setAndFetchFaviconForPage when it is called with invalid
 * arguments, and when no favicon is stored for the given arguments.
 */
function test() {
  // Initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let favIcon16Location =
    "http://example.org/tests/toolkit/components/places/tests/browser/favicon-normal16.png";
  let favIcon32Location =
    "http://example.org/tests/toolkit/components/places/tests/browser/favicon-normal32.png";
  let favIcon16URI = NetUtil.newURI(favIcon16Location);
  let favIcon32URI = NetUtil.newURI(favIcon32Location);
  let lastPageURI = NetUtil.newURI("http://example.com/verification");
  // This error icon must stay in sync with FAVICON_ERRORPAGE_URL in
  // nsIFaviconService.idl, aboutCertError.xhtml and netError.xhtml.
  let favIconErrorPageURI =
    NetUtil.newURI("chrome://global/skin/icons/warning-16.png");
  let favIconsResultCount = 0;
  let pageURI;

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      windowsToClose.push(aWin);
      executeSoon(() => aCallback(aWin));
    });
  };

  // This function is called after calling finish() on the test.
  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
  });

  function checkFavIconsDBCount(aCallback) {
    let stmt = DBConn().createAsyncStatement("SELECT url FROM moz_favicons");
    stmt.executeAsync({
      handleResult: function final_handleResult(aResultSet) {
        for (let row; (row = aResultSet.getNextRow()); ) {
          favIconsResultCount++;
        }
      },
      handleError: function final_handleError(aError) {
        throw("Unexpected error (" + aError.result + "): " + aError.message);
      },
      handleCompletion: function final_handleCompletion(aReason) {
        //begin testing
        info("Previous records in moz_favicons: " + favIconsResultCount);
        if (aCallback) {
          aCallback();
        }
      }
    });
    stmt.finalize();
  }

  function testNullPageURI(aWindow, aCallback) {
    try {
      aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(null, favIcon16URI,
        true, aWindow.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);
      throw("Exception expected because aPageURI is null.");
    } catch (ex) {
      // We expected an exception.
      ok(true, "Exception expected because aPageURI is null");
    }

    if (aCallback) {
      aCallback();
    }
  }

  function testNullFavIconURI(aWindow, aCallback) {
    try {
      aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(
        NetUtil.newURI("http://example.com/null_faviconURI"), null, true,
          aWindow.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);
      throw("Exception expected because aFaviconURI is null.");
    } catch (ex) {
      // We expected an exception.
      ok(true, "Exception expected because aFaviconURI is null.");
    }

    if (aCallback) {
      aCallback();
    }
  }

  function testAboutURI(aWindow, aCallback) {
    aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(
      NetUtil.newURI("about:testAboutURI"), favIcon16URI, true,
        aWindow.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);

    if (aCallback) {
      aCallback();
    }
  }

  function testPrivateBrowsingNonBookmarkedURI(aWindow, aCallback) {
    let pageURI = NetUtil.newURI("http://example.com/privateBrowsing");
    addVisits({ uri: pageURI, transitionType: TRANSITION_TYPED }, aWindow,
      function () {
        aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI,
          favIcon16URI, true, aWindow.PlacesUtils.favicons.FAVICON_LOAD_PRIVATE);

        if (aCallback) {
          aCallback();
        }
    });
  }

  function testDisabledHistory(aWindow, aCallback) {
    let pageURI = NetUtil.newURI("http://example.com/disabledHistory");
    addVisits({ uri: pageURI, transition: TRANSITION_TYPED }, aWindow,
      function () {
        aWindow.Services.prefs.setBoolPref("places.history.enabled", false);

        aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI,
          favIcon16URI, true,
            aWindow.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);

        // The setAndFetchFaviconForPage function calls CanAddURI synchronously, thus
        // we can set the preference back to true immediately . We don't clear the
        // preference because not all products enable Places by default.
        aWindow.Services.prefs.setBoolPref("places.history.enabled", true);

        if (aCallback) {
          aCallback();
        }
    });
  }

  function testErrorIcon(aWindow, aCallback) {
    let pageURI = NetUtil.newURI("http://example.com/errorIcon");
    let places = [{ uri: pageURI, transition: TRANSITION_TYPED }];
    addVisits({ uri: pageURI, transition: TRANSITION_TYPED }, aWindow,
      function () {
        aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI,
          favIconErrorPageURI, true,
            aWindow.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);

      if (aCallback) {
        aCallback();
      }
    });
  }

  function testNonExistingPage(aWindow, aCallback) {
    aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(
      NetUtil.newURI("http://example.com/nonexistingPage"), favIcon16URI, true,
        aWindow.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);

    if (aCallback) {
      aCallback();
    }
  }

  function testFinalVerification(aWindow, aCallback) {
    // Only the last test should raise the onPageChanged notification,
    // executing the waitForFaviconChanged callback.
    waitForFaviconChanged(lastPageURI, favIcon32URI, aWindow,
      function final_callback() {
        // Check that only one record corresponding to the last favicon is present.
        let resultCount = 0;
        let stmt = DBConn().createAsyncStatement("SELECT url FROM moz_favicons");
        stmt.executeAsync({
          handleResult: function final_handleResult(aResultSet) {

            // If the moz_favicons DB had been previously loaded (before our
            // test began), we should focus only in the URI we are testing and
            // skip the URIs not related to our test.
            if (favIconsResultCount > 0) {
              for (let row; (row = aResultSet.getNextRow()); ) {
                if (favIcon32URI.spec === row.getResultByIndex(0)) {
                  is(favIcon32URI.spec, row.getResultByIndex(0),
                    "Check equal favicons");
                  resultCount++;
                }
              }
            } else {
              for (let row; (row = aResultSet.getNextRow()); ) {
                is(favIcon32URI.spec, row.getResultByIndex(0),
                  "Check equal favicons");
                resultCount++;
              }
            }
          },
          handleError: function final_handleError(aError) {
            throw("Unexpected error (" + aError.result + "): " + aError.message);
          },
          handleCompletion: function final_handleCompletion(aReason) {
            is(Ci.mozIStorageStatementCallback.REASON_FINISHED, aReason,
              "Check reasons are equal");
            is(1, resultCount, "Check result count");
            if (aCallback) {
              aCallback();
            }
          }
        });
        stmt.finalize();
    });

    // This is the only test that should cause the waitForFaviconChanged
    // callback to be invoked.  In turn, the callback will invoke
    // finish() causing the tests to finish.
    addVisits({ uri: lastPageURI, transition: TRANSITION_TYPED }, aWindow,
      function () {
        aWindow.PlacesUtils.favicons.setAndFetchFaviconForPage(lastPageURI,
          favIcon32URI, true,
            aWindow.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE);
    });
  }

  checkFavIconsDBCount(function () {
    testOnWindow({}, function(aWin) {
      testNullPageURI(aWin, function () {
        testOnWindow({}, function(aWin) {
          testNullFavIconURI(aWin, function() {
            testOnWindow({}, function(aWin) {
              testAboutURI(aWin, function() {
                testOnWindow({private: true}, function(aWin) {
                  testPrivateBrowsingNonBookmarkedURI(aWin, function () {
                    testOnWindow({}, function(aWin) {
                      testDisabledHistory(aWin, function () {
                        testOnWindow({}, function(aWin) {
                          testErrorIcon(aWin, function() {
                            testOnWindow({}, function(aWin) {
                              testNonExistingPage(aWin, function() {
                                testOnWindow({}, function(aWin) {
                                  testFinalVerification(aWin, function() {
                                    finish();
                                  });
                                });
                              });
                            });
                          });
                        });
                      });
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  });
}
