/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the discovery view loads properly

const MAIN_URL = "https://example.com/" + RELATIVE_DIR + "discovery.html";

var gManagerWindow;
var gCategoryUtilities;
var gProvider;

var gLoadCompleteCallback = null;

var gProgressListener = {
  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    // Only care about the network stop status events
    if (!(aStateFlags & (Ci.nsIWebProgressListener.STATE_IS_NETWORK)) ||
        !(aStateFlags & (Ci.nsIWebProgressListener.STATE_STOP)))
      return;

    if (gLoadCompleteCallback)
      executeSoon(gLoadCompleteCallback);
    gLoadCompleteCallback = null;
  },

  onLocationChange() { },
  onSecurityChange() { },
  onProgressChange() { },
  onStatusChange() { },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),
};

function test() {
  // Switch to a known url
  Services.prefs.setCharPref(PREF_DISCOVERURL, MAIN_URL);
  // Temporarily enable caching
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

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

function testHash(aBrowser, aTestAddonVisible, aCallback) {
  var hash = getHash(aBrowser);
  isnot(hash, null, "There should be a hash");
  try {
    var data = JSON.parse(hash);
  } catch (e) {
    ok(false, "Hash should have been valid JSON: " + e);
    aCallback();
    return;
  }
  is(typeof data, "object", "Hash should be a JS object");

  // Ensure that at least the test add-ons are present
  if (aTestAddonVisible[0])
    ok("addon1@tests.mozilla.org" in data, "Test add-on 1 should be listed");
  else
    ok(!("addon1@tests.mozilla.org" in data), "Test add-on 1 should not be listed");
  if (aTestAddonVisible[1])
    ok("addon2@tests.mozilla.org" in data, "Test add-on 2 should be listed");
  else
    ok(!("addon2@tests.mozilla.org" in data), "Test add-on 2 should not be listed");
  if (aTestAddonVisible[2])
    ok("addon3@tests.mozilla.org" in data, "Test add-on 3 should be listed");
  else
    ok(!("addon3@tests.mozilla.org" in data), "Test add-on 3 should not be listed");

  // Test against all the add-ons the manager knows about since plugins and
  // app extensions may exist
  AddonManager.getAllAddons(function(aAddons) {
    for (let addon of aAddons) {
      if (!(addon.id in data)) {
        // Test add-ons will have shown an error if necessary above
        if (addon.id.substring(6) != "@tests.mozilla.org")
          ok(false, "Add-on " + addon.id + " was not included in the data");
        continue;
      }

      info("Testing data for add-on " + addon.id);
      var addonData = data[addon.id];
      is(addonData.name, addon.name, "Name should be correct");
      is(addonData.version, addon.version, "Version should be correct");
      is(addonData.type, addon.type, "Type should be correct");
      is(addonData.userDisabled, addon.userDisabled, "userDisabled should be correct");
      is(addonData.isBlocklisted, addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED, "blocklisted should be correct");
      is(addonData.isCompatible, addon.isCompatible, "isCompatible should be correct");
    }
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

  executeSoon(function() {
    ok(isLoading(), "Clicking a link should show the loading pane");
  });
}

// Tests that switching to the discovery view displays the right url
add_test(function() {
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    gCategoryUtilities.openType("discover", function() {
      var browser = gManagerWindow.document.getElementById("discover-browser");
      is(getURL(browser), MAIN_URL, "Should have loaded the right url");

      testHash(browser, [true, true, true], function() {
        close_manager(gManagerWindow, run_next_test);
      });
    });

    ok(isLoading(), "Should be loading at first");
  });
});

// Tests that loading the add-ons manager with the discovery view as the last
// selected view displays the right url
add_test(function() {
  // Hide one of the test add-ons
  Services.prefs.setBoolPref("extensions.addon2@tests.mozilla.org.getAddons.cache.enabled", false);
  Services.prefs.setBoolPref("extensions.addon3@tests.mozilla.org.getAddons.cache.enabled", true);

  open_manager(null, function(aWindow) {
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    is(gCategoryUtilities.selectedCategory, "discover", "Should have loaded the right view");

    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL, "Should have loaded the right url");

    testHash(browser, [true, false, true], function() {
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
  Services.prefs.clearUserPref("extensions.addon2@tests.mozilla.org.getAddons.cache.enabled");
  Services.prefs.setBoolPref("extensions.addon3@tests.mozilla.org.getAddons.cache.enabled", false);

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

          testHash(browser, [true, true, false], function() {
            Services.prefs.clearUserPref("extensions.addon3@tests.mozilla.org.getAddons.cache.enabled");
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
  Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);

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
          is(getURL(browser), MAIN_URL, "Should have loaded the right url");

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

// Test for Bug 703929 - Loading the discover view from a chrome XUL file fails when
// the add-on manager is reopened.
add_test(function() {
  const url = "chrome://mochitests/content/" + RELATIVE_DIR + "addon_about.xul";
  Services.prefs.setCharPref(PREF_DISCOVERURL, url);

  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), url, "Loading a chrome XUL file should work");

    restart_manager(gManagerWindow, "addons://discover/", function(aWindow) {
      gManagerWindow = aWindow;
      gCategoryUtilities = new CategoryUtilities(gManagerWindow);

      var browser = gManagerWindow.document.getElementById("discover-browser");
      is(getURL(browser), url, "Should be able to load the chrome XUL file a second time");

      close_manager(gManagerWindow, run_next_test);
    });
  });
});

// Bug 711693 - Send the compatibility mode when loading the Discovery pane
add_test(function() {
  info("Test '%COMPATIBILITY_MODE%' in the URL is correctly replaced by 'normal'");
  Services.prefs.setCharPref(PREF_DISCOVERURL, MAIN_URL + "?mode=%COMPATIBILITY_MODE%");
  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, false);

  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;
    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL + "?mode=normal", "Should have loaded the right url");
    close_manager(gManagerWindow, run_next_test);
  });
});

add_test(function() {
  info("Test '%COMPATIBILITY_MODE%' in the URL is correctly replaced by 'strict'");
  Services.prefs.setBoolPref(PREF_STRICT_COMPAT, true);

  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;
    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL + "?mode=strict", "Should have loaded the right url");
    close_manager(gManagerWindow, run_next_test);
  });
});

add_test(function() {
  info("Test '%COMPATIBILITY_MODE%' in the URL is correctly replaced by 'ignore'");
  Services.prefs.setBoolPref(PREF_CHECK_COMPATIBILITY, false);

  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;
    var browser = gManagerWindow.document.getElementById("discover-browser");
    is(getURL(browser), MAIN_URL + "?mode=ignore", "Should have loaded the right url");
    close_manager(gManagerWindow, run_next_test);
  });
});

// Test for Bug 601442 - extensions.getAddons.showPane need to be update
// for the new addon manager.
function bug_601442_test_elements(visible) {
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    if (visible)
      ok(gCategoryUtilities.isTypeVisible("discover"), "Discover category should be visible");
    else
      ok(!gCategoryUtilities.isTypeVisible("discover"), "Discover category should not be visible");

    gManagerWindow.loadView("addons://list/dictionary");
    wait_for_view_load(gManagerWindow, function(aManager) {
      var button = aManager.document.getElementById("discover-button-install");
      if (visible)
        ok(!is_hidden(button), "Discover button should be visible!");
      else
        ok(is_hidden(button), "Discover button should not be visible!");

      close_manager(gManagerWindow, run_next_test);
    });
  });
}

add_test(function() {
  Services.prefs.setBoolPref(PREF_DISCOVER_ENABLED, false);
  Services.prefs.setBoolPref(PREF_XPI_ENABLED, true);
  bug_601442_test_elements(false);
});
add_test(function() {
  Services.prefs.setBoolPref(PREF_DISCOVER_ENABLED, true);
  Services.prefs.setBoolPref(PREF_XPI_ENABLED, false);
  bug_601442_test_elements(false);
});
add_test(function() {
  Services.prefs.setBoolPref(PREF_DISCOVER_ENABLED, false);
  Services.prefs.setBoolPref(PREF_XPI_ENABLED, false);
  bug_601442_test_elements(false);
});
add_test(function() {
  Services.prefs.setBoolPref(PREF_DISCOVER_ENABLED, true);
  Services.prefs.setBoolPref(PREF_XPI_ENABLED, true);
  bug_601442_test_elements(true);
});

// Test for Bug 1132971 - if extensions.getAddons.showPane is false,
// the extensions pane should show by default
add_test(function() {
  Services.prefs.clearUserPref(PREF_UI_LASTCATEGORY);
  Services.prefs.setBoolPref(PREF_DISCOVER_ENABLED, false);

  open_manager(null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);
    is(gCategoryUtilities.selectedCategory, "extension", "Should be showing the extension view");
    close_manager(gManagerWindow, run_next_test);
    Services.prefs.clearUserPref(PREF_DISCOVER_ENABLED);
  });
});

// Test for Bug 1219495 - should show placeholder content when offline
add_test(function() {
  // set a URL to cause an error
  Services.prefs.setCharPref(PREF_DISCOVERURL, "https://nocert.example.com/");

  open_manager("addons://discover/", function(aWindow) {
    gManagerWindow = aWindow;

    ok(isError(), "Should have shown the placeholder content");

    close_manager(gManagerWindow, run_next_test);
  });
});
