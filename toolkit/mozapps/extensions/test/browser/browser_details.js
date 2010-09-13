/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests various aspects of the details view

const PREF_GETADDONS_GETSEARCHRESULTS = "extensions.getAddons.search.url";
const SEARCH_URL = TESTROOT + "browser_details.xml";

var gManagerWindow;
var gCategoryUtilities;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gVersion = Services.appinfo.version;
var gBlocklistURL = Services.urlFormatter.formatURLPref("extensions.blocklist.detailsURL");
var gPluginURL = Services.urlFormatter.formatURLPref("plugins.update.url");
var gDate = new Date(2010, 7, 1);

function open_details(aId, aType, aCallback) {
  gCategoryUtilities.openType(aType, function() {
    var list = gManagerWindow.document.getElementById("addon-list");
    var item = list.firstChild;
    while (item) {
      if ("mAddon" in item && item.mAddon.id == aId) {
        list.ensureElementIsVisible(item);
        EventUtils.synthesizeMouse(item, 2, 2, { clickCount: 2 }, gManagerWindow);
        wait_for_view_load(gManagerWindow, aCallback);
        return;
      }
      item = item.nextSibling;
    }
    ok(false, "Should have found the add-on in the list");
  });
}

function get(aId) {
  return gManagerWindow.document.getElementById(aId);
}

function test() {
  // Turn on searching for this test
  Services.prefs.setIntPref(PREF_SEARCH_MAXRESULTS, 15);
  Services.prefs.setCharPref(PREF_GETADDONS_GETSEARCHRESULTS, SEARCH_URL);

  waitForExplicitFinish();

  gProvider = new MockProvider();

  gProvider.createAddons([{
    id: "addon1@tests.mozilla.org",
    name: "Test add-on 1",
    version: "2.1",
    description: "Short description",
    fullDescription: "Longer description",
    type: "extension",
    iconURL: "chrome://foo/skin/icon.png",
    icon64URL: "chrome://foo/skin/icon64.png",
    contributionURL: "http://foo.com",
    contributionAmount: "$0.99",
    sourceURI: Services.io.newURI("http://example.com/foo", null, null),
    averageRating: 4,
    reviewCount: 5,
    reviewURL: "http://example.com/reviews",
    homepageURL: "http://example.com/addon1",
    applyBackgroundUpdates: true
  }, {
    id: "addon2@tests.mozilla.org",
    name: "Test add-on 2",
    version: "2.2",
    description: "Short description",
    creator: { name: "Mozilla", url: null },
    type: "extension",
    iconURL: "chrome://foo/skin/icon.png",
    updateDate: gDate,
    permissions: 0,
    screenshots: [{url: "http://example.com/screenshot"}],
  }, {
    id: "addon3@tests.mozilla.org",
    name: "Test add-on 3",
    description: "Short description",
    creator: { name: "Mozilla", url: "http://www.mozilla.org" },
    type: "extension",
    sourceURI: Services.io.newURI("http://example.com/foo", null, null),
    updateDate: gDate,
    reviewCount: 1,
    reviewURL: "http://example.com/reviews",
    applyBackgroundUpdates: false,
    isActive: false,
    isCompatible: false,
    appDisabled: true,
    permissions: AddonManager.PERM_CAN_ENABLE |
                 AddonManager.PERM_CAN_DISABLE |
                 AddonManager.PERM_CAN_UPGRADE,
    screenshots: [{
      url: "http://example.com/screenshot",
      thumbnailURL: "http://example.com/thumbnail"
    }],
  }, {
    id: "addon4@tests.mozilla.org",
    name: "Test add-on 4",
    _userDisabled: true,
    isActive: false,
    blocklistState: Ci.nsIBlocklistService.STATE_SOFTBLOCKED
  }, {
    id: "addon5@tests.mozilla.org",
    name: "Test add-on 5",
    isActive: false,
    blocklistState: Ci.nsIBlocklistService.STATE_BLOCKED,
    appDisabled: true
  }, {
    id: "addon6@tests.mozilla.org",
    name: "Test add-on 6",
    operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
  }, {
    id: "addon7@tests.mozilla.org",
    name: "Test add-on 7",
    _userDisabled: true,
    isActive: false
  }, {
    id: "addon8@tests.mozilla.org",
    name: "Test add-on 8",
    blocklistState: Ci.nsIBlocklistService.STATE_OUTDATED
  }]);

  open_manager(null, function(aWindow) {
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    run_next_test();
  });
}

function end_test() {
  close_manager(gManagerWindow, function() {
    finish();
  });
}

// Opens and tests the details view for add-on 1
add_test(function() {
  open_details("addon1@tests.mozilla.org", "extension", function() {
    is(get("detail-name").value, "Test add-on 1", "Name should be correct");
    is_element_visible(get("detail-version"), "Version should not be hidden");
    is(get("detail-version").value, "2.1", "Version should be correct");
    is(get("detail-icon").src, "chrome://foo/skin/icon64.png", "Icon should be correct");
    is_element_hidden(get("detail-creator"), "Creator should be hidden");
    is_element_hidden(get("detail-screenshot"), "Screenshot should be hidden");
    is(get("detail-desc").textContent, "Longer description", "Description should be correct");

    is_element_visible(get("detail-contributions"), "Contributions section should be visible");
    ok(get("detail-contrib-suggested").value, "$0.99");

    is_element_hidden(get("detail-dateUpdated"), "Update date should be hidden");

    is_element_visible(get("detail-rating-row"), "Rating row should not be hidden");
    is_element_visible(get("detail-rating"), "Rating should not be hidden");
    is(get("detail-rating").averageRating, 4, "Rating should be correct");
    is_element_visible(get("detail-reviews"), "Reviews should not be hidden");
    is(get("detail-reviews").href, "http://example.com/reviews", "Review URL should be correct");
    is(get("detail-reviews").value, "5 reviews", "Review text should be correct");

    is_element_visible(get("detail-homepage-row"), "Homepage should be visible");
    ok(get("detail-homepage").href, "http://example.com/addon1");
    is_element_hidden(get("detail-repository-row"), "Repository profile should not be visible");

    is_element_hidden(get("detail-size"), "Size should be hidden");

    is_element_hidden(get("detail-downloads"), "Downloads should be hidden");

    is_element_visible(get("detail-autoUpdate"), "Updates should not be hidden");
    ok(get("detail-autoUpdate").firstChild.selected, "Updates ahould be automatic");
    is_element_hidden(get("detail-findUpdates"), "Check for updates should be hidden");
    EventUtils.synthesizeMouse(get("detail-autoUpdate").lastChild, 2, 2, {}, gManagerWindow);
    ok(get("detail-autoUpdate").lastChild.selected, "Updates should be manual");
    is_element_visible(get("detail-findUpdates"), "Check for updates should be visible");
    EventUtils.synthesizeMouse(get("detail-autoUpdate").firstChild, 2, 2, {}, gManagerWindow);
    ok(get("detail-autoUpdate").firstChild.selected, "Updates should be automatic");
    is_element_hidden(get("detail-findUpdates"), "Check for updates should be hidden");

    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable"), "Enable button should be hidden");
    is_element_visible(get("detail-disable"), "Disable button should be visible");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    // Disable it
    EventUtils.synthesizeMouse(get("detail-disable"), 2, 2, {}, gManagerWindow);
    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_visible(get("detail-enable"), "Enable button should be visible");
    is_element_hidden(get("detail-disable"), "Disable button should be hidden");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_visible(get("detail-pending"), "Pending message should be visible");
    is(get("detail-pending").textContent, "Test add-on 1 will be disabled after you restart " + gApp + ".", "Pending message should be correct");

    // Reopen it
    open_details("addon1@tests.mozilla.org", "extension", function() {
      is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
      is_element_visible(get("detail-enable"), "Enable button should be visible");
      is_element_hidden(get("detail-disable"), "Disable button should be hidden");
      is_element_visible(get("detail-uninstall"), "Remove button should be visible");

      is_element_hidden(get("detail-warning"), "Warning message should be hidden");
      is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_visible(get("detail-pending"), "Pending message should be visible");
      is(get("detail-pending").textContent, "Test add-on 1 will be disabled after you restart " + gApp + ".", "Pending message should be correct");

      // Undo disabling
      EventUtils.synthesizeMouse(get("detail-undo"), 2, 2, {}, gManagerWindow);
      is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
      is_element_hidden(get("detail-enable"), "Enable button should be hidden");
      is_element_visible(get("detail-disable"), "Disable button should be visible");
      is_element_visible(get("detail-uninstall"), "Remove button should be visible");

      is_element_hidden(get("detail-warning"), "Warning message should be hidden");
      is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_hidden(get("detail-pending"), "Pending message should be hidden");

      run_next_test();
    });
  });
});

// Opens and tests the details view for add-on 2
add_test(function() {
  open_details("addon2@tests.mozilla.org", "extension", function() {
    is(get("detail-name").value, "Test add-on 2", "Name should be correct");
    is_element_visible(get("detail-version"), "Version should not be hidden");
    is(get("detail-version").value, "2.2", "Version should be correct");
    is(get("detail-icon").src, "chrome://foo/skin/icon.png", "Icon should be correct");

    is_element_visible(get("detail-creator"), "Creator should not be hidden");
    is_element_visible(get("detail-creator")._creatorName, "Creator name should not be hidden");
    is(get("detail-creator")._creatorName.value, "Mozilla", "Creator should be correct");
    is_element_hidden(get("detail-creator")._creatorLink, "Creator link should be hidden");

    is_element_visible(get("detail-screenshot"), "Screenshot should be visible");
    is(get("detail-screenshot").src, "http://example.com/screenshot", "Should be showing the full sized screenshot");
    is(get("detail-desc").textContent, "Short description", "Description should be correct");

    is_element_hidden(get("detail-contributions"), "Contributions section should be hidden");

    is_element_visible(get("detail-dateUpdated"), "Update date should not be hidden");
    is(Date.parse(get("detail-dateUpdated").value), gDate.getTime(), "Update date should be correct");

    is_element_hidden(get("detail-rating-row"), "Rating should be hidden");

    is_element_hidden(get("detail-homepage-row"), "Homepage should not be visible");
    is_element_hidden(get("detail-repository-row"), "Repository profile should not be visible");

    is_element_hidden(get("detail-size"), "Size should be hidden");

    is_element_hidden(get("detail-downloads"), "Downloads should be hidden");

    is_element_hidden(get("detail-updates-row"), "Updates should be hidden");

    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable"), "Enable button should be hidden");
    is_element_hidden(get("detail-disable"), "Disable button should be hidden");
    is_element_hidden(get("detail-uninstall"), "Remove button should be hidden");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    run_next_test();
  });
});

// Opens and tests the details view for add-on 3
add_test(function() {
  open_details("addon3@tests.mozilla.org", "extension", function() {
    is(get("detail-name").value, "Test add-on 3", "Name should be correct");
    is_element_hidden(get("detail-version"), "Version should be hidden");
    is(get("detail-icon").src, "", "Icon should be correct");

    is_element_visible(get("detail-creator"), "Creator should not be hidden");
    is_element_hidden(get("detail-creator")._creatorName, "Creator name should be hidden");
    is_element_visible(get("detail-creator")._creatorLink, "Creator link should not be hidden");
    is(get("detail-creator")._creatorLink.value, "Mozilla", "Creator link should be correct");
    is(get("detail-creator")._creatorLink.href, "http://www.mozilla.org", "Creator link href should be correct");

    is_element_visible(get("detail-screenshot"), "Screenshot should be visible");
    is(get("detail-screenshot").src, "http://example.com/thumbnail", "Should be showing the thumbnail");

    is_element_hidden(get("detail-contributions"), "Contributions section should be hidden");

    is_element_visible(get("detail-dateUpdated"), "Update date should not be hidden");
    is(Date.parse(get("detail-dateUpdated").value), gDate.getTime(), "Update date should be correct");

    is_element_visible(get("detail-rating-row"), "Rating row should not be hidden");
    is_element_hidden(get("detail-rating"), "Rating should be hidden");
    is_element_visible(get("detail-reviews"), "Reviews should not be hidden");
    is(get("detail-reviews").href, "http://example.com/reviews", "Review URL should be correct");
    is(get("detail-reviews").value, "1 review", "Review text should be correct");

    is_element_hidden(get("detail-size"), "Size should be hidden");

    is_element_hidden(get("detail-downloads"), "Downloads should be hidden");

    is_element_visible(get("detail-autoUpdate"), "Updates should not be hidden");
    ok(get("detail-autoUpdate").lastChild.selected, "Updates ahould be manual");
    is_element_visible(get("detail-findUpdates"), "Check for updates should be visible");
    EventUtils.synthesizeMouse(get("detail-autoUpdate").firstChild, 2, 2, {}, gManagerWindow);
    ok(get("detail-autoUpdate").firstChild.selected, "Updates ahould be automatic");
    is_element_hidden(get("detail-findUpdates"), "Check for updates should be hidden");
    EventUtils.synthesizeMouse(get("detail-autoUpdate").lastChild, 2, 2, {}, gManagerWindow);
    ok(get("detail-autoUpdate").lastChild.selected, "Updates ahould be manual");
    is_element_visible(get("detail-findUpdates"), "Check for updates should be visible");

    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable"), "Enable button should be hidden");
    is_element_hidden(get("detail-disable"), "Disable button should be hidden");
    is_element_hidden(get("detail-uninstall"), "Remove button should be hidden");

    is_element_visible(get("detail-warning"), "Warning message should be visible");
    is(get("detail-warning").textContent, "Test add-on 3 is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    run_next_test();
  });
});

// Opens and tests the details view for add-on 4
add_test(function() {
  open_details("addon4@tests.mozilla.org", "extension", function() {
    is(get("detail-name").value, "Test add-on 4", "Name should be correct");

    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_visible(get("detail-enable"), "Enable button should be visible");
    is_element_hidden(get("detail-disable"), "Disable button should be hidden");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_visible(get("detail-warning"), "Warning message should be visible");
    is(get("detail-warning").textContent, "Test add-on 4 is known to cause security or stability issues.", "Warning message should be correct");
    is_element_visible(get("detail-warning-link"), "Warning link should be visible");
    is(get("detail-warning-link").value, "More Information", "Warning link text should be correct");
    is(get("detail-warning-link").href, gBlocklistURL, "Warning link should be correct");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    // Enable it
    EventUtils.synthesizeMouse(get("detail-enable"), 2, 2, {}, gManagerWindow);
    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable"), "Enable button should be hidden");
    is_element_visible(get("detail-disable"), "Disable button should be visible");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_visible(get("detail-pending"), "Pending message should be visible");
    is(get("detail-pending").textContent, "Test add-on 4 will be enabled after you restart " + gApp + ".", "Pending message should be correct");

    // Reopen it
    open_details("addon4@tests.mozilla.org", "extension", function() {
      is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
      is_element_hidden(get("detail-enable"), "Enable button should be hidden");
      is_element_visible(get("detail-disable"), "Disable button should be visible");
      is_element_visible(get("detail-uninstall"), "Remove button should be visible");

      is_element_hidden(get("detail-warning"), "Warning message should be hidden");
      is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_visible(get("detail-pending"), "Pending message should be visible");
      is(get("detail-pending").textContent, "Test add-on 4 will be enabled after you restart " + gApp + ".", "Pending message should be correct");

      // Undo enabling
      EventUtils.synthesizeMouse(get("detail-undo"), 2, 2, {}, gManagerWindow);
      is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
      is_element_visible(get("detail-enable"), "Enable button should be visible");
      is_element_hidden(get("detail-disable"), "Disable button should be hidden");
      is_element_visible(get("detail-uninstall"), "Remove button should be visible");

      is_element_visible(get("detail-warning"), "Warning message should be visible");
      is(get("detail-warning").textContent, "Test add-on 4 is known to cause security or stability issues.", "Warning message should be correct");
      is_element_visible(get("detail-warning-link"), "Warning link should be visible");
      is(get("detail-warning-link").value, "More Information", "Warning link text should be correct");
      is(get("detail-warning-link").href, gBlocklistURL, "Warning link should be correct");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_hidden(get("detail-pending"), "Pending message should be hidden");

      run_next_test();
    });
  });
});

// Opens and tests the details view for add-on 5
add_test(function() {
  open_details("addon5@tests.mozilla.org", "extension", function() {
    is(get("detail-name").value, "Test add-on 5", "Name should be correct");

    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable"), "Enable button should be hidden");
    is_element_hidden(get("detail-disable"), "Disable button should be hidden");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_visible(get("detail-error"), "Error message should be visible");
    is(get("detail-error").textContent, "Test add-on 5 has been disabled due to security or stability issues.", "Error message should be correct");
    is_element_visible(get("detail-error-link"), "Error link should be visible");
    is(get("detail-error-link").value, "More Information", "Error link text should be correct");
    is(get("detail-error-link").href, gBlocklistURL, "Error link should be correct");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    run_next_test();
  });
});

// Opens and tests the details view for add-on 6
add_test(function() {
  open_details("addon6@tests.mozilla.org", "extension", function() {
    is(get("detail-name").value, "Test add-on 6", "Name should be correct");

    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable"), "Enable button should be hidden");
    is_element_visible(get("detail-disable"), "Disable button should be visible");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    // Disable it
    EventUtils.synthesizeMouse(get("detail-disable"), 2, 2, {}, gManagerWindow);
    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_visible(get("detail-enable"), "Enable button should be visible");
    is_element_hidden(get("detail-disable"), "Disable button should be hidden");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    // Reopen it
    open_details("addon6@tests.mozilla.org", "extension", function() {
      is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
      is_element_visible(get("detail-enable"), "Enable button should be visible");
      is_element_hidden(get("detail-disable"), "Disable button should be hidden");
      is_element_visible(get("detail-uninstall"), "Remove button should be visible");

      is_element_hidden(get("detail-warning"), "Warning message should be hidden");
      is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_hidden(get("detail-pending"), "Pending message should be visible");

      // Enable it
      EventUtils.synthesizeMouse(get("detail-enable"), 2, 2, {}, gManagerWindow);
      is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
      is_element_hidden(get("detail-enable"), "Enable button should be hidden");
      is_element_visible(get("detail-disable"), "Disable button should be visible");
      is_element_visible(get("detail-uninstall"), "Remove button should be visible");

      is_element_hidden(get("detail-warning"), "Warning message should be hidden");
      is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_hidden(get("detail-pending"), "Pending message should be hidden");

      run_next_test();
    });
  });
});

// Opens and tests the details view for add-on 7
add_test(function() {
  open_details("addon7@tests.mozilla.org", "extension", function() {
    is(get("detail-name").value, "Test add-on 7", "Name should be correct");

    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_visible(get("detail-enable"), "Enable button should be visible");
    is_element_hidden(get("detail-disable"), "Disable button should be hidden");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    // Enable it
    EventUtils.synthesizeMouse(get("detail-enable"), 2, 2, {}, gManagerWindow);
    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable"), "Enable button should be hidden");
    is_element_visible(get("detail-disable"), "Disable button should be visible");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_visible(get("detail-pending"), "Pending message should be visible");
    is(get("detail-pending").textContent, "Test add-on 7 will be enabled after you restart " + gApp + ".", "Pending message should be correct");

    // Reopen it
    open_details("addon7@tests.mozilla.org", "extension", function() {
      is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
      is_element_hidden(get("detail-enable"), "Enable button should be hidden");
      is_element_visible(get("detail-disable"), "Disable button should be visible");
      is_element_visible(get("detail-uninstall"), "Remove button should be visible");

      is_element_hidden(get("detail-warning"), "Warning message should be hidden");
      is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_visible(get("detail-pending"), "Pending message should be visible");
      is(get("detail-pending").textContent, "Test add-on 7 will be enabled after you restart " + gApp + ".", "Pending message should be correct");

      // Undo enabling
      EventUtils.synthesizeMouse(get("detail-undo"), 2, 2, {}, gManagerWindow);
      is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
      is_element_visible(get("detail-enable"), "Enable button should be visible");
      is_element_hidden(get("detail-disable"), "Disable button should be hidden");
      is_element_visible(get("detail-uninstall"), "Remove button should be visible");

      is_element_hidden(get("detail-warning"), "Warning message should be hidden");
      is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_hidden(get("detail-pending"), "Pending message should be hidden");

      run_next_test();
    });
  });
});

// Opens and tests the details view for add-on 8
add_test(function() {
  open_details("addon8@tests.mozilla.org", "extension", function() {
    is(get("detail-name").value, "Test add-on 8", "Name should be correct");

    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable"), "Enable button should be hidden");
    is_element_visible(get("detail-disable"), "Disable button should be visible");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_visible(get("detail-warning"), "Warning message should be visible");
    is(get("detail-warning").textContent, "An important update is available for Test add-on 8.", "Warning message should be correct");
    is_element_visible(get("detail-warning-link"), "Warning link should be visible");
    is(get("detail-warning-link").value, "Update Now", "Warning link text should be correct");
    is(get("detail-warning-link").href, gPluginURL, "Warning link should be correct");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    // Disable it
    EventUtils.synthesizeMouse(get("detail-disable"), 2, 2, {}, gManagerWindow);
    is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
    is_element_visible(get("detail-enable"), "Enable button should be visible");
    is_element_hidden(get("detail-disable"), "Disable button should be hidden");
    is_element_visible(get("detail-uninstall"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_visible(get("detail-pending"), "Pending message should be visible");
    is(get("detail-pending").textContent, "Test add-on 8 will be disabled after you restart " + gApp + ".", "Pending message should be correct");

    // Reopen it
    open_details("addon8@tests.mozilla.org", "extension", function() {
      is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
      is_element_visible(get("detail-enable"), "Enable button should be visible");
      is_element_hidden(get("detail-disable"), "Disable button should be hidden");
      is_element_visible(get("detail-uninstall"), "Remove button should be visible");

      is_element_hidden(get("detail-warning"), "Warning message should be hidden");
      is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_visible(get("detail-pending"), "Pending message should be visible");
      is(get("detail-pending").textContent, "Test add-on 8 will be disabled after you restart " + gApp + ".", "Pending message should be correct");

      // Undo disabling
      EventUtils.synthesizeMouse(get("detail-undo"), 2, 2, {}, gManagerWindow);
      is_element_hidden(get("detail-prefs"), "Preferences button should be hidden");
      is_element_hidden(get("detail-enable"), "Enable button should be hidden");
      is_element_visible(get("detail-disable"), "Disable button should be visible");
      is_element_visible(get("detail-uninstall"), "Remove button should be visible");

      is_element_visible(get("detail-warning"), "Warning message should be visible");
      is(get("detail-warning").textContent, "An important update is available for Test add-on 8.", "Warning message should be correct");
      is_element_visible(get("detail-warning-link"), "Warning link should be visible");
      is(get("detail-warning-link").value, "Update Now", "Warning link text should be correct");
      is(get("detail-warning-link").href, gPluginURL, "Warning link should be correct");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_hidden(get("detail-pending"), "Pending message should be hidden");

      run_next_test();
    });
  });
});
