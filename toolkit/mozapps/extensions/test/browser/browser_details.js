/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests various aspects of the details view

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const PREF_AUTOUPDATE_DEFAULT = "extensions.update.autoUpdateDefault";

var gManagerWindow;
var gCategoryUtilities;
var gProvider;

var gApp = document.getElementById("bundle_brand").getString("brandShortName");
var gVersion = Services.appinfo.version;
var gDate = new Date(2010, 7, 1);
var infoURL = Services.urlFormatter.formatURLPref("app.support.baseURL") + "unsigned-addons";

async function open_details(aId, aType, aCallback) {
  requestLongerTimeout(2);

  await gCategoryUtilities.openType(aType);
  var list = gManagerWindow.document.getElementById("addon-list");
  var item = list.firstChild;
  while (item) {
    if ("mAddon" in item && item.mAddon.id == aId) {
      list.ensureElementIsVisible(item);
      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 1 }, gManagerWindow);
      EventUtils.synthesizeMouseAtCenter(item, { clickCount: 2 }, gManagerWindow);
      wait_for_view_load(gManagerWindow, aCallback);
      return;
    }
    item = item.nextSibling;
  }
  ok(false, "Should have found the add-on in the list");
}

function get(aId) {
  return gManagerWindow.document.getElementById(aId);
}

async function test() {
  requestLongerTimeout(2);

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
    sourceURI: Services.io.newURI("http://example.com/foo"),
    averageRating: 4,
    reviewCount: 5,
    reviewURL: "http://example.com/reviews",
    homepageURL: "http://example.com/addon1",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_ENABLE
  }, {
    id: "addon2@tests.mozilla.org",
    name: "Test add-on 2",
    version: "2.2",
    description: "Short description",
    creator: { name: "Mozilla", url: null },
    type: "extension",
    iconURL: "chrome://foo/skin/icon.png",
    contributionURL: "http://foo.com",
    contributionAmount: null,
    updateDate: gDate,
    permissions: 0,
    screenshots: [{
      url: "chrome://branding/content/about.png",
      width: 200,
      height: 150
    }],
  }, {
    id: "addon3@tests.mozilla.org",
    name: "Test add-on 3",
    description: "Short description",
    creator: { name: "Mozilla", url: "http://www.mozilla.org" },
    type: "extension",
    sourceURI: Services.io.newURI("http://example.com/foo"),
    updateDate: gDate,
    reviewCount: 1,
    reviewURL: "http://example.com/reviews",
    applyBackgroundUpdates: AddonManager.AUTOUPDATE_DISABLE,
    isActive: false,
    isCompatible: false,
    appDisabled: true,
    permissions: AddonManager.PERM_CAN_ENABLE |
                 AddonManager.PERM_CAN_DISABLE |
                 AddonManager.PERM_CAN_UPGRADE,
    screenshots: [{
      url: "http://example.com/screenshot",
      width: 400,
      height: 300,
      thumbnailURL: "chrome://branding/content/icon64.png",
      thumbnailWidth: 160,
      thumbnailHeight: 120
    }],
  }, {
    id: "addon5@tests.mozilla.org",
    blocklistURL: "http://example.com/addon5@tests.mozilla.org",
    name: "Test add-on 5",
    isActive: false,
    blocklistState: Ci.nsIBlocklistService.STATE_BLOCKED,
    appDisabled: true
  }, {
    id: "addon8@tests.mozilla.org",
    blocklistURL: "http://example.com/addon8@tests.mozilla.org",
    name: "Test add-on 8",
    blocklistState: Ci.nsIBlocklistService.STATE_OUTDATED
  }, {
    id: "addon9@tests.mozilla.org",
    name: "Test add-on 9",
    signedState: AddonManager.SIGNEDSTATE_MISSING,
  }, {
    id: "addon10@tests.mozilla.org",
    name: "Test add-on 10",
    signedState: AddonManager.SIGNEDSTATE_MISSING,
    isActive: false,
    appDisabled: true,
    isCompatible: false,
  }, {
    id: "addon11@tests.mozilla.org",
    name: "Test add-on 11",
    signedState: AddonManager.SIGNEDSTATE_PRELIMINARY,
    foreignInstall: true,
    isActive: false,
    appDisabled: true,
    isCompatible: false,
  }, {
    id: "addon12@tests.mozilla.org",
    name: "Test add-on 12",
    signedState: AddonManager.SIGNEDSTATE_SIGNED,
    foreignInstall: true,
  }]);

  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  run_next_test();
}

async function end_test() {
  await close_manager(gManagerWindow);
  finish();
}

// Opens and tests the details view for add-on 2
add_test(function() {
  open_details("addon2@tests.mozilla.org", "extension", function() {
    is(get("detail-name").textContent, "Test add-on 2", "Name should be correct");
    is_element_visible(get("detail-version"), "Version should not be hidden");
    is(get("detail-version").value, "2.2", "Version should be correct");
    is(get("detail-icon").src, "chrome://foo/skin/icon.png", "Icon should be correct");

    is_element_visible(get("detail-creator"), "Creator should not be hidden");
    is_element_visible(get("detail-creator")._creatorName, "Creator name should not be hidden");
    is(get("detail-creator")._creatorName.value, "Mozilla", "Creator should be correct");
    is_element_hidden(get("detail-creator")._creatorLink, "Creator link should be hidden");

    is_element_visible(get("detail-screenshot-box"), "Screenshot should be visible");
    is(get("detail-screenshot").src, "chrome://branding/content/about.png", "Should be showing the full sized screenshot");
    is(get("detail-screenshot").width, 200, "Screenshot dimensions should be set");
    is(get("detail-screenshot").height, 150, "Screenshot dimensions should be set");
    is(get("detail-screenshot").hasAttribute("loading"), true, "Screenshot should have loading attribute");
    is(get("detail-desc").textContent, "Short description", "Description should be correct");
    is_element_hidden(get("detail-fulldesc"), "Full description should be hidden");

    is_element_visible(get("detail-contributions"), "Contributions section should be visible");

    is_element_visible(get("detail-dateUpdated"), "Update date should not be hidden");
    is(get("detail-dateUpdated").value, formatDate(gDate), "Update date should be correct");

    is_element_hidden(get("detail-rating-row"), "Rating should be hidden");

    is_element_hidden(get("detail-homepage-row"), "Homepage should not be visible");
    is_element_hidden(get("detail-repository-row"), "Repository profile should not be visible");

    is_element_hidden(get("detail-size"), "Size should be hidden");

    is_element_hidden(get("detail-updates-row"), "Updates should be hidden");

    is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
    is_element_hidden(get("detail-disable-btn"), "Disable button should be hidden");
    is_element_hidden(get("detail-uninstall-btn"), "Remove button should be hidden");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    get("detail-screenshot").addEventListener("load", function() {
      is(this.hasAttribute("loading"), false, "Screenshot should not have loading attribute");
      run_next_test();
    }, {once: true});
  });
});

// Opens and tests the details view for add-on 3
add_test(function() {
  open_details("addon3@tests.mozilla.org", "extension", function() {
    is(get("detail-name").textContent, "Test add-on 3", "Name should be correct");
    is_element_hidden(get("detail-version"), "Version should be hidden");
    is(get("detail-icon").src, "", "Icon should be correct");

    is_element_visible(get("detail-creator"), "Creator should not be hidden");
    is_element_hidden(get("detail-creator")._creatorName, "Creator name should be hidden");
    is_element_visible(get("detail-creator")._creatorLink, "Creator link should not be hidden");
    is(get("detail-creator")._creatorLink.value, "Mozilla", "Creator link should be correct");
    is(get("detail-creator")._creatorLink.href, "http://www.mozilla.org", "Creator link href should be correct");

    is_element_visible(get("detail-screenshot-box"), "Screenshot should be visible");
    is(get("detail-screenshot").src, "chrome://branding/content/icon64.png", "Should be showing the thumbnail");
    is(get("detail-screenshot").width, 160, "Screenshot dimensions should be set");
    is(get("detail-screenshot").height, 120, "Screenshot dimensions should be set");
    is(get("detail-screenshot").hasAttribute("loading"), true, "Screenshot should have loading attribute");

    is_element_hidden(get("detail-contributions"), "Contributions section should be hidden");

    is_element_visible(get("detail-updates-row"), "Updates should not be hidden");
    is_element_visible(get("detail-dateUpdated"), "Update date should not be hidden");
    is(get("detail-dateUpdated").value, formatDate(gDate), "Update date should be correct");

    is_element_visible(get("detail-rating-row"), "Rating row should not be hidden");
    is_element_hidden(get("detail-rating"), "Rating should be hidden");
    is_element_visible(get("detail-reviews"), "Reviews should not be hidden");
    is(get("detail-reviews").href, "http://example.com/reviews", "Review URL should be correct");
    is(get("detail-reviews").value, "1 review", "Review text should be correct");

    is_element_hidden(get("detail-size"), "Size should be hidden");

    is_element_visible(get("detail-autoUpdate"), "Updates should not be hidden");
    ok(get("detail-autoUpdate").lastChild.selected, "Updates should be manual");
    is_element_visible(get("detail-findUpdates-btn"), "Check for updates should be visible");
    EventUtils.synthesizeMouseAtCenter(get("detail-autoUpdate").childNodes[1], {}, gManagerWindow);
    ok(get("detail-autoUpdate").childNodes[1].selected, "Updates should be automatic");
    is_element_hidden(get("detail-findUpdates-btn"), "Check for updates should be hidden");
    EventUtils.synthesizeMouseAtCenter(get("detail-autoUpdate").lastChild, {}, gManagerWindow);
    ok(get("detail-autoUpdate").lastChild.selected, "Updates should be manual");
    is_element_visible(get("detail-findUpdates-btn"), "Check for updates should be visible");

    info("Setting " + PREF_AUTOUPDATE_DEFAULT + " to true");
    Services.prefs.setBoolPref(PREF_AUTOUPDATE_DEFAULT, true);
    EventUtils.synthesizeMouseAtCenter(get("detail-autoUpdate").firstChild, {}, gManagerWindow);
    ok(get("detail-autoUpdate").firstChild.selected, "Updates should be default");
    is_element_hidden(get("detail-findUpdates-btn"), "Check for updates should be hidden");

    info("Setting " + PREF_AUTOUPDATE_DEFAULT + " to false");
    Services.prefs.setBoolPref(PREF_AUTOUPDATE_DEFAULT, false);
    ok(get("detail-autoUpdate").firstChild.selected, "Updates should be default");
    is_element_visible(get("detail-findUpdates-btn"), "Check for updates should be visible");
    EventUtils.synthesizeMouseAtCenter(get("detail-autoUpdate").childNodes[1], {}, gManagerWindow);
    ok(get("detail-autoUpdate").childNodes[1].selected, "Updates should be automatic");
    is_element_hidden(get("detail-findUpdates-btn"), "Check for updates should be hidden");
    EventUtils.synthesizeMouseAtCenter(get("detail-autoUpdate").firstChild, {}, gManagerWindow);
    ok(get("detail-autoUpdate").firstChild.selected, "Updates should be default");
    is_element_visible(get("detail-findUpdates-btn"), "Check for updates should be visible");
    Services.prefs.clearUserPref(PREF_AUTOUPDATE_DEFAULT);

    is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
    is_element_hidden(get("detail-disable-btn"), "Disable button should be hidden");
    is_element_hidden(get("detail-uninstall-btn"), "Remove button should be hidden");

    is_element_visible(get("detail-warning"), "Warning message should be visible");
    is(get("detail-warning").textContent, "Test add-on 3 is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    get("detail-screenshot").addEventListener("load", function() {
      is(this.hasAttribute("loading"), false, "Screenshot should not have loading attribute");
      run_next_test();
    }, {once: true});
  });
});

// Opens and tests the details view for add-on 5
add_test(function() {
  open_details("addon5@tests.mozilla.org", "extension", async function() {
    await TestUtils.waitForCondition(() => !is_hidden(get("detail-error-link")));
    is(get("detail-name").textContent, "Test add-on 5", "Name should be correct");

    is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
    is_element_hidden(get("detail-disable-btn"), "Disable button should be hidden");
    is_element_visible(get("detail-uninstall-btn"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_visible(get("detail-error"), "Error message should be visible");
    is(get("detail-error").textContent, "Test add-on 5 has been disabled due to security or stability issues.", "Error message should be correct");
    is_element_visible(get("detail-error-link"), "Error link should be visible");
    is(get("detail-error-link").value, "More Information", "Error link text should be correct");
    is(get("detail-error-link").href, "http://example.com/addon5@tests.mozilla.org", "Error link should be correct");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    run_next_test();
  });
});

// These tests are only appropriate when signing can be turned off
if (!AppConstants.MOZ_REQUIRE_SIGNING) {
  // Opens and tests the details view for add-on 9
  add_test(function() {
    open_details("addon9@tests.mozilla.org", "extension", function() {
      is(get("detail-name").textContent, "Test add-on 9", "Name should be correct");

      is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
      is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
      is_element_visible(get("detail-disable-btn"), "Disable button should be visible");
      is_element_visible(get("detail-uninstall-btn"), "Remove button should be visible");

      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_visible(get("detail-warning"), "Error message should be visible");
      is(get("detail-warning").textContent, "Test add-on 9 could not be verified for use in " + gApp + ". Proceed with caution.", "Warning message should be correct");
      is_element_visible(get("detail-warning-link"), "Warning link should be visible");
      is(get("detail-warning-link").value, "More Information", "Warning link text should be correct");
      is(get("detail-warning-link").href, infoURL, "Warning link should be correct");
      is_element_hidden(get("detail-pending"), "Pending message should be hidden");

      run_next_test();
    });
  });
}

// Opens and tests the details view for add-on 9 with signing required
add_test(async function() {
  await close_manager(gManagerWindow);
  Services.prefs.setBoolPref("xpinstall.signatures.required", true);
  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  open_details("addon9@tests.mozilla.org", "extension", async function() {
    await TestUtils.waitForCondition(() => !is_hidden(get("detail-error-link")));
    is(get("detail-name").textContent, "Test add-on 9", "Name should be correct");

    is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
    is_element_visible(get("detail-disable-btn"), "Disable button should be visible");
    is_element_visible(get("detail-uninstall-btn"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_visible(get("detail-error"), "Error message should be visible");
    is(get("detail-error").textContent, "Test add-on 9 could not be verified for use in " + gApp + " and has been disabled.", "Error message should be correct");
    is_element_visible(get("detail-error-link"), "Error link should be visible");
    is(get("detail-error-link").value, "More Information", "Error link text should be correct");
    is(get("detail-error-link").href, infoURL, "Error link should be correct");

    await close_manager(gManagerWindow);
    Services.prefs.setBoolPref("xpinstall.signatures.required", false);
    aWindow = await open_manager(null);
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    run_next_test();
  });
});

// These tests are only appropriate when signing can be turned off
if (!AppConstants.REQUIRE_SIGNING) {
  // Opens and tests the details view for add-on 10
  add_test(function() {
    open_details("addon10@tests.mozilla.org", "extension", function() {
      is(get("detail-name").textContent, "Test add-on 10", "Name should be correct");

      is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
      is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
      is_element_hidden(get("detail-disable-btn"), "Disable button should be hidden");
      is_element_visible(get("detail-uninstall-btn"), "Remove button should be visible");

      is_element_visible(get("detail-warning"), "Warning message should be visible");
      is(get("detail-warning").textContent, "Test add-on 10 is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");
      is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
      is_element_hidden(get("detail-error"), "Error message should be hidden");
      is_element_hidden(get("detail-error-link"), "Error link should be hidden");
      is_element_hidden(get("detail-pending"), "Pending message should be hidden");

      run_next_test();
    });
  });
}

// Opens and tests the details view for add-on 10 with signing required
add_test(async function() {
  await close_manager(gManagerWindow);
  Services.prefs.setBoolPref("xpinstall.signatures.required", true);
  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  open_details("addon10@tests.mozilla.org", "extension", async function() {
    await TestUtils.waitForCondition(() => !is_hidden(get("detail-error-link")));
    is(get("detail-name").textContent, "Test add-on 10", "Name should be correct");

    is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
    is_element_hidden(get("detail-disable-btn"), "Disable button should be hidden");
    is_element_visible(get("detail-uninstall-btn"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_visible(get("detail-error"), "Error message should be visible");
    is(get("detail-error").textContent, "Test add-on 10 could not be verified for use in " + gApp + " and has been disabled.", "Error message should be correct");
    is_element_visible(get("detail-error-link"), "Error link should be visible");
    is(get("detail-error-link").value, "More Information", "Error link text should be correct");
    is(get("detail-error-link").href, infoURL, "Error link should be correct");

    await close_manager(gManagerWindow);
    Services.prefs.setBoolPref("xpinstall.signatures.required", false);
    aWindow = await open_manager(null);
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    run_next_test();
  });
});

// Opens and tests the details view for add-on 11
add_test(function() {
  open_details("addon11@tests.mozilla.org", "extension", function() {
    is(get("detail-name").textContent, "Test add-on 11", "Name should be correct");

    is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
    is_element_hidden(get("detail-disable-btn"), "Disable button should be hidden");
    is_element_visible(get("detail-uninstall-btn"), "Remove button should be visible");

    is_element_visible(get("detail-warning"), "Warning message should be visible");
    is(get("detail-warning").textContent, "Test add-on 11 is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    run_next_test();
  });
});

// Opens and tests the details view for add-on 11 with signing required
add_test(async function() {
  await close_manager(gManagerWindow);
  Services.prefs.setBoolPref("xpinstall.signatures.required", true);
  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  open_details("addon11@tests.mozilla.org", "extension", async function() {
    is(get("detail-name").textContent, "Test add-on 11", "Name should be correct");

    is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
    is_element_hidden(get("detail-disable-btn"), "Disable button should be hidden");
    is_element_visible(get("detail-uninstall-btn"), "Remove button should be visible");

    is_element_visible(get("detail-warning"), "Warning message should be visible");
    is(get("detail-warning").textContent, "Test add-on 11 is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");

    await close_manager(gManagerWindow);
    Services.prefs.setBoolPref("xpinstall.signatures.required", false);
    aWindow = await open_manager(null);
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    run_next_test();
  });
});

// Opens and tests the details view for add-on 12
add_test(function() {
  open_details("addon12@tests.mozilla.org", "extension", function() {
    is(get("detail-name").textContent, "Test add-on 12", "Name should be correct");

    is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
    is_element_visible(get("detail-disable-btn"), "Disable button should be visible");
    is_element_visible(get("detail-uninstall-btn"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    run_next_test();
  });
});

// Opens and tests the details view for add-on 12 with signing required
add_test(async function() {
  await close_manager(gManagerWindow);
  Services.prefs.setBoolPref("xpinstall.signatures.required", true);
  let aWindow = await open_manager(null);
  gManagerWindow = aWindow;
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);

  open_details("addon12@tests.mozilla.org", "extension", async function() {
    is(get("detail-name").textContent, "Test add-on 12", "Name should be correct");

    is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
    is_element_visible(get("detail-disable-btn"), "Disable button should be visible");
    is_element_visible(get("detail-uninstall-btn"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-error-link"), "Error link should be hidden");

    await close_manager(gManagerWindow);
    Services.prefs.setBoolPref("xpinstall.signatures.required", false);
    aWindow = await open_manager(null);
    gManagerWindow = aWindow;
    gCategoryUtilities = new CategoryUtilities(gManagerWindow);

    run_next_test();
  });
});

// Tests that upgrades with onExternalInstall apply immediately
add_test(function() {
  open_details("addon1@tests.mozilla.org", "extension", function() {
    gProvider.createAddons([{
      id: "addon1@tests.mozilla.org",
      name: "Test add-on replacement",
      version: "2.5",
      description: "Short description replacement",
      fullDescription: "Longer description replacement",
      type: "extension",
      iconURL: "chrome://foo/skin/icon.png",
      icon64URL: "chrome://foo/skin/icon264.png",
      sourceURI: Services.io.newURI("http://example.com/foo"),
      averageRating: 2,
      optionsURL: "chrome://foo/content/options.xul",
      applyBackgroundUpdates: AddonManager.AUTOUPDATE_ENABLE,
      operationsRequiringRestart: AddonManager.OP_NEEDS_RESTART_NONE
    }]);

    is(get("detail-name").textContent, "Test add-on replacement", "Name should be correct");
    is_element_visible(get("detail-version"), "Version should not be hidden");
    is(get("detail-version").value, "2.5", "Version should be correct");
    is(get("detail-icon").src, "chrome://foo/skin/icon264.png", "Icon should be correct");
    is_element_hidden(get("detail-creator"), "Creator should be hidden");
    is_element_hidden(get("detail-screenshot-box"), "Screenshot should be hidden");
    is(get("detail-desc").textContent, "Short description replacement", "Description should be correct");
    is(get("detail-fulldesc").textContent, "Longer description replacement", "Full description should be correct");

    is_element_hidden(get("detail-contributions"), "Contributions section should be hidden");

    is_element_hidden(get("detail-dateUpdated"), "Update date should be hidden");

    is_element_visible(get("detail-rating-row"), "Rating row should not be hidden");
    is_element_visible(get("detail-rating"), "Rating should not be hidden");
    is(get("detail-rating").averageRating, 2, "Rating should be correct");
    is_element_hidden(get("detail-reviews"), "Reviews should be hidden");

    is_element_hidden(get("detail-homepage-row"), "Homepage should be hidden");

    is_element_hidden(get("detail-size"), "Size should be hidden");

    is_element_hidden(get("detail-prefs-btn"), "Preferences button should be hidden");
    is_element_hidden(get("detail-enable-btn"), "Enable button should be hidden");
    is_element_visible(get("detail-disable-btn"), "Disable button should be visible");
    is_element_visible(get("detail-uninstall-btn"), "Remove button should be visible");

    is_element_hidden(get("detail-warning"), "Warning message should be hidden");
    is_element_hidden(get("detail-warning-link"), "Warning link should be hidden");
    is_element_hidden(get("detail-error"), "Error message should be hidden");
    is_element_hidden(get("detail-pending"), "Pending message should be hidden");

    run_next_test();
  });
});

// Check that onPropertyChanges for appDisabled updates the UI
add_test(async function() {
  info("Checking that onPropertyChanges for appDisabled updates the UI");

  let aAddon = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  aAddon.userDisabled = true;
  aAddon.isCompatible = true;
  aAddon.appDisabled = false;

  open_details("addon1@tests.mozilla.org", "extension", function() {
    is(get("detail-view").getAttribute("active"), "false", "Addon should not be marked as active");
    is_element_hidden(get("detail-warning"), "Warning message should not be visible");

    info("Making addon incompatible and appDisabled");
    aAddon.isCompatible = false;
    aAddon.appDisabled = true;

    is(get("detail-view").getAttribute("active"), "false", "Addon should not be marked as active");
    is_element_visible(get("detail-warning"), "Warning message should be visible");
    is(get("detail-warning").textContent, "Test add-on replacement is incompatible with " + gApp + " " + gVersion + ".", "Warning message should be correct");

    run_next_test();
  });
});
