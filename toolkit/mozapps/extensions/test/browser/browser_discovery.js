/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the discovery view loads properly

const PREF_DISCOVERURL = "extensions.webservice.discoverURL";
const PREF_BACKGROUND_UPDATE = "extensions.update.enabled";

var gManagerWindow;
var gCategoryUtilities;
var gProvider;

function test() {
  // Switch to a known url
  Services.prefs.setCharPref(PREF_DISCOVERURL, TESTROOT + "releaseNotes.xhtml");

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
  var url = aBrowser.currentURI.spec;
  var pos = url.indexOf("#");
  if (pos != -1)
    return url.substring(0, pos);
  return url;
}

function getHash(aBrowser) {
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

// Tests that switching to the discovery view displays the right url
add_test(function() {
  open_manager("addons://list/extension", function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    gCategoryUtilities.openType("discover", function() {
      var browser = gManagerWindow.document.getElementById("discover-browser");
      is(getURL(browser), TESTROOT + "releaseNotes.xhtml", "Should have loaded the right url");

      testHash(browser, function() {
        close_manager(gManagerWindow, run_next_test);
      });
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
    is(getURL(browser), TESTROOT + "releaseNotes.xhtml", "Should have loaded the right url");

    testHash(browser, function() {
      close_manager(gManagerWindow, run_next_test);
    });
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
          is(getURL(browser), TESTROOT + "releaseNotes.xhtml", "Should have loaded the right url");

          testHash(browser, function() {
            close_manager(gManagerWindow, run_next_test);
          });
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
      is(getURL(browser), TESTROOT + "releaseNotes.xhtml", "Should have loaded the right url");

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
    is(getURL(browser), TESTROOT + "releaseNotes.xhtml", "Should have loaded the right url");

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
          is(getURL(browser), TESTROOT + "releaseNotes.xhtml", "Should have loaded the right url");

          is(getHash(browser), null, "Hash should not have been passed");
          close_manager(gManagerWindow, run_next_test);
        });
      });
    });
  });
});
