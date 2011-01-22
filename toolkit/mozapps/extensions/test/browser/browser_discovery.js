/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the discovery view loads properly

const PREF_BACKGROUND_UPDATE = "extensions.update.enabled";
const PREF_DISCOVERURL = "extensions.webservice.discoverURL";
const MAIN_URL = "https://example.com/" + RELATIVE_DIR + "discovery.html";

var gManagerWindow;
var gCategoryUtilities;
var gProvider;

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

  onLocationChange: function() { },
  onSecurityChange: function() { },
  onProgressChange: function() { },
  onStatusChange: function() { },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),
};

function test() {
  // Switch to a known url
  Services.prefs.setCharPref(PREF_DISCOVERURL, MAIN_URL);

  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "Test add-on 1",
    type: "extension",
    version: "2.2",
    isCompatible: false,
    blocklistState: Ci.nsIBlocklistService.STATE_SOFTBLOCKED,
    userDisabled: false
  }, {
    id: "addon2@tests.mozilla.org",
    name: "Test add-on 2",
    type: "plugin",
    version: "3.1.5",
    isCompatible: true,
    blocklistState: Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
    userDisabled: false
  }, {
    id: "addon3@tests.mozilla.org",
    name: "Test add-on 3",
    type: "theme",
    version: "1.2b1",
    isCompatible: false,
    blocklistState: Ci.nsIBlocklistService.STATE_BLOCKED,
    userDisabled: true
  }]);

  run_next_test();
}

function end_test() {
  finish();
}

function getURL(aBrowser) {
  if (gManagerWindow.document.getElementById("discover-view").selectedPanel !=
      aBrowser)
    return null;

  var url = aBrowser.currentURI.spec;
  var pos = url.indexOf("#");
  if (pos != -1)
    return url.substring(0, pos);
  return url;
}

function getHash(aBrowser) {
  if (gManagerWindow.document.getElementById("discover-view").selectedPanel !=
      aBrowser)
    return null;

  var url = aBrowser.currentURI.spec;
  var pos = url.indexOf("#");
  if (pos != -1)
    return decodeURIComponent(url.substring(pos + 1));
  return null;
}

function testHash(aBrowser, aCallback) {
  var hash = getHash(aBrowser);
  isnot(hash, null, "There should be a hash");
  try {
    var data = JSON.parse(hash);
  }
  catch (e) {
    ok(false, "Hash should have been valid JSON: " + e);
    aCallback();
    return;
  }
  is(typeof data, "object", "Hash should be a JS object");

  // Ensure that at least the test add-ons are present
  ok("addon1@tests.mozilla.org" in data, "Test add-on 1 should be listed");
  ok("addon2@tests.mozilla.org" in data, "Test add-on 2 should be listed");
  ok("addon3@tests.mozilla.org" in data, "Test add-on 3 should be listed");

  // Test against all the add-ons the manager knows about since plugins and
  // app extensions may exist
  AddonManager.getAllAddons(function(aAddons) {
    aAddons.forEach(function(aAddon) {
      info("Testing data for add-on " + aAddon.id);
      if (!aAddon.id in data) {
        ok(false, "Add-on was not included in the data");
        return;
      }
      var addonData = data[aAddon.id];
      is(addonData.name, aAddon.name, "Name should be correct");
      is(addonData.version, aAddon.version, "Version should be correct");
      is(addonData.type, aAddon.type, "Type should be correct");
      is(addonData.userDisabled, aAddon.userDisabled, "userDisabled should be correct");
      is(addonData.isBlocklisted, aAddon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED, "blocklisted should be correct");
      is(addonData.isCompatible, aAddon.isCompatible, "isCompatible should be correct");
    });

    aCallback();
  });
}

function isLoading() {
  var loading = gManagerWindow.document.getElementById("discover-view").selectedPanel ==
                gManagerWindow.document.getElementById("discover-loading");
  if (loading) {
    is_element_visible(gManagerWindow.document.querySelector("#discover-loading .loading"),
                       "Loading message should be visible when its panel is the selected panel");
  }
  return loading;
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
  EventUtils.sendMouseEvent({type: "click"}, link);
}

// Tests that switching to the discovery view displays the right url
add_test(function() {
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    gCategoryUtilities.openType("discover", function() {
      var browser = gManagerWindow.document.getElementById("discover-browser");
      is(getURL(browser), MAIN_URL, "Should have loaded the right url");

      testHash(browser, function() {
        close_manager(gManagerWindow, run_next_test);
      });
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

    testHash(browser, function() {
      close_manager(gManagerWindow, run_next_test);
    });
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

          testHash(browser, function() {
            close_manager(gManagerWindow, run_next_test);
          });
        }, function(aWindow) {
          gManagerWindow = aWindow;
          ok(isLoading(), "Should be loading at first");
        });
      });
    });
  });
});

// Tests that switching to the discovery view displays the right url
add_test(function() {
  Services.prefs.setBoolPref(PREF_BACKGROUND_UPDATE, false);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(PREF_BACKGROUND_UPDATE);
  });

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    gCategoryUtilities.openType("discover", function() {
      var browser = gManagerWindow.document.getElementById("discover-browser");
      is(getURL(browser), MAIN_URL, "Should have loaded the right url");

      is(getHash(browser), null, "Hash should not have been passed");
      close_manager(gManagerWindow, run_next_test);
    });
  });
});

// Tests that loading the add-ons manager with the discovery view as the last
// selected view displays the right url
add_test(function() {
  open_manager(null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    is(gCategoryUtilities.selectedCategory, "discover", "Should have loaded the right view");

    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL, "Should have loaded the right url");

    is(getHash(browser), null, "Hash should not have been passed");
    close_manager(gManagerWindow, run_next_test);
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
          gManagerWindow = aWindow;
          gCategoryUtilities = new CategoryUtilities(gManagerWindow);
          is(gCategoryUtilities.selectedCategory, "discover", "Should have loaded the right view");

          var browser = gManagerWindow.document.getElementById("discover-browser");
          is(getURL(browser), MAIN_URL, "Should have loaded the right url");

          is(getHash(browser), null, "Hash should not have been passed");
          close_manager(gManagerWindow, run_next_test);
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

// Stopping the initial load should display the error page and then correctly
// reload when switching away and back again
add_test(function() {
  Services.prefs.setCharPref(PREF_DISCOVERURL, MAIN_URL);

  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    var browser = gManagerWindow.document.getElementById("discover-browser");

    EventUtils.synthesizeMouse(gCategoryUtilities.get("discover"), 2, 2, { }, gManagerWindow);

    wait_for_view_load(gManagerWindow, function() {
      ok(isError(), "Should have shown the error page");

      gCategoryUtilities.openType("extension", function() {
        EventUtils.synthesizeMouse(gCategoryUtilities.get("discover"), 2, 2, { }, gManagerWindow);

        wait_for_view_load(gManagerWindow, function() {
          ok(isError(), "Should have shown the error page");

          gCategoryUtilities.openType("extension", function() {
            gCategoryUtilities.openType("discover", function() {
              is(getURL(browser), MAIN_URL, "Should have loaded the right url");

              close_manager(gManagerWindow, run_next_test);
            });
          });
        });

        ok(isLoading(), "Should be loading");
        // This will stop the real page load
        browser.stop();
      });
    });

    ok(isLoading(), "Should be loading");
    // This will actually stop the about:blank load
    browser.stop();
  });
});
