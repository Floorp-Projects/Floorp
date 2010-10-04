/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the discovery view loads properly

const PREF_DISCOVERURL = "extensions.webservice.discoverURL";
const MAIN_URL = "https://example.com/" + RELATIVE_DIR + "discovery.html";

var gManagerWindow;
var gCategoryUtilities;

var gLoadCompleteCallback = null;

var gProgressListener = {
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    // Only care about the network stop status events
    if (!(aStateFlags & (Ci.nsIWebProgressListener.STATE_IS_NETWORK)) ||
        !(aStateFlags & (Ci.nsIWebProgressListener.STATE_STOP)))
      return;

    if (gLoadCompleteCallback)
      executeSoon(gLoadCompleteCallback);
    gLoadCompleteCallback = null;
  },

  onLocationChange: function(aWebProgress, aRequest, aLocation) { },
  onSecurityChange: function(aWebProgress, aRequest, aState) { },
  onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress,
                             aMaxSelfProgress, aCurTotalProgress,
                             aMaxTotalProgress) { },
  onStatusChange: function(aWebProgress, aRequest, aStatus, aMessage) { },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),
};

function test() {
  // Switch to a known url
  Services.prefs.setCharPref(PREF_DISCOVERURL, MAIN_URL);

  waitForExplicitFinish();

  run_next_test();
}

function end_test() {
  finish();
}

function getURL(browser) {
  if (gManagerWindow.document.getElementById("discover-view").selectedPanel !=
      browser)
    return null;

  var url = browser.currentURI.spec;
  var pos = url.indexOf("#");
  if (pos != -1)
    return url.substring(0, pos);
  return url;
}

function isLoading() {
  return gManagerWindow.document.getElementById("discover-view").selectedPanel ==
         gManagerWindow.document.getElementById("discover-loading");
}

function isError() {
  return gManagerWindow.document.getElementById("discover-view").selectedPanel ==
         gManagerWindow.document.getElementById("discover-error");
}

function clickLink(aId, aCallback) {
  var browser = gManagerWindow.document.getElementById("discover-browser");
  browser.addProgressListener(gProgressListener);

  gLoadCompleteCallback = function() {
    browser.removeProgressListener(gProgressListener);
    aCallback();
  };

  var link = browser.contentDocument.getElementById(aId);
  EventUtils.synthesizeMouse(link, 2, 2, { }, browser.contentWindow);
}

// Tests that switching to the discovery view displays the right url
add_test(function() {
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    gCategoryUtilities.openType("discover", function() {
      var browser = gManagerWindow.document.getElementById("discover-browser");
      is(getURL(browser), MAIN_URL, "Should have loaded the right url");

      close_manager(gManagerWindow, run_next_test);
    });

    ok(isLoading(), "Should be loading at first");
  });
});

// Tests that loading the add-ons manager with the discovery view as the last
// selected view displays the right url
add_test(function() {
  open_manager(null, function(aWindow) {
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    is(gCategoryUtilities.selectedCategory, "discover", "Should have loaded the right view");

    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL, "Should have loaded the right url");

    close_manager(gManagerWindow, run_next_test);
  }, function(aWindow) {
    gManagerWindow = aWindow;
    ok(isLoading(), "Should be loading at first");
  });
});

// Tests that loading the add-ons manager with the discovery view as the initial
// view displays the right url
add_test(function() {
  open_manager(null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    gCategoryUtilities.openType("extension", function() {
      close_manager(gManagerWindow, function() {
        open_manager("addons://discover/", function(aWindow) {
          gCategoryUtilities = new CategoryUtilities(gManagerWindow);
          is(gCategoryUtilities.selectedCategory, "discover", "Should have loaded the right view");

          var browser = gManagerWindow.document.getElementById("discover-browser");
          is(getURL(browser), MAIN_URL, "Should have loaded the right url");

          close_manager(gManagerWindow, run_next_test);
        }, function(aWindow) {
          gManagerWindow = aWindow;
          ok(isLoading(), "Should be loading at first");
        });
      });
    });
  });
});

// Tests that navigating to an insecure page fails
add_test(function() {
  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL, "Should have loaded the right url");

    clickLink("link-http", function() {
      ok(isError(), "Should have shown the error page");

      gCategoryUtilities.openType("extension", function() {
        gCategoryUtilities.openType("discover", function() {
          is(getURL(browser), MAIN_URL, "Should have loaded the right url");

          close_manager(gManagerWindow, run_next_test);
        });
        ok(isLoading(), "Should start loading again");
      });
    });
  });
});

// Tests that navigating to a different domain fails
add_test(function() {
  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL, "Should have loaded the right url");

    clickLink("link-domain", function() {
      ok(isError(), "Should have shown the error page");

      gCategoryUtilities.openType("extension", function() {
        gCategoryUtilities.openType("discover", function() {
          is(getURL(browser), MAIN_URL, "Should have loaded the right url");

          close_manager(gManagerWindow, run_next_test);
        });
        ok(isLoading(), "Should start loading again");
      });
    });
  });
});

// Tests that navigating to a missing page fails
add_test(function() {
  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL, "Should have loaded the right url");

    clickLink("link-bad", function() {
      ok(isError(), "Should have shown the error page");

      gCategoryUtilities.openType("extension", function() {
        gCategoryUtilities.openType("discover", function() {
          is(getURL(browser), MAIN_URL, "Should have loaded the right url");

          close_manager(gManagerWindow, run_next_test);
        });
        ok(isLoading(), "Should start loading again");
      });
    });
  });
});

// Tests that navigating to a page on the same domain works
add_test(function() {
  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL, "Should have loaded the right url");

    clickLink("link-good", function() {
      is(getURL(browser), "https://example.com/" + RELATIVE_DIR + "releaseNotes.xhtml", "Should have loaded the right url");

      gCategoryUtilities.openType("extension", function() {
        gCategoryUtilities.openType("discover", function() {
          is(getURL(browser), "https://example.com/" + RELATIVE_DIR + "releaseNotes.xhtml", "Should have loaded the right url");

          close_manager(gManagerWindow, run_next_test);
        });
      });
    });
  });
});


// Tests repeated navigation to the same page followed by a navigation to a
// different domain
add_test(function() {
  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL, "Should have loaded the right url");

    var count = 10;
    function clickAgain(aCallback) {
      if (count-- == 0)
        aCallback();
      else
        clickLink("link-normal", clickAgain.bind(null, aCallback));
    }

    clickAgain(function() {
      is(getURL(browser), MAIN_URL, "Should have loaded the right url");

      clickLink("link-domain", function() {
        ok(isError(), "Should have shown the error page");

        gCategoryUtilities.openType("extension", function() {
          gCategoryUtilities.openType("discover", function() {
            is(getURL(browser), MAIN_URL, "Should have loaded the right url");

            close_manager(gManagerWindow, run_next_test);
          });
          ok(isLoading(), "Should start loading again");
        });
      });
    });
  });
});

// Loading an insecure main page should work if that is what the prefs say, should
// also be able to navigate to a https page and back again
add_test(function() {
  Services.prefs.setCharPref(PREF_DISCOVERURL, TESTROOT + "discovery.html");

  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), TESTROOT + "discovery.html", "Should have loaded the right url");

    clickLink("link-normal", function() {
      is(getURL(browser), MAIN_URL, "Should have loaded the right url");

      clickLink("link-http", function() {
        is(getURL(browser), TESTROOT + "discovery.html", "Should have loaded the right url");

        close_manager(gManagerWindow, run_next_test);
      });
    });
  });
});
