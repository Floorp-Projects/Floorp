/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

/* globals TestUtils */

let {ExtensionTestCommon} = Components.utils.import("resource://testing-common/ExtensionTestCommon.jsm", {});

Components.utils.import("resource://testing-common/ContentTask.jsm", {});

var gAddon;
var gOtherAddon;
var gManagerWindow;
var gCategoryUtilities;

function installAddon(details) {
  let id = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator)
                                              .generateUUID().number;
  if (!details.manifest) {
    details.manifest = {};
  }
  details.manifest.applications = {gecko: {id}};
  let xpi = ExtensionTestCommon.generateXPI(details);

  return AddonManager.installTemporaryAddon(xpi).then(addon => {
    SimpleTest.registerCleanupFunction(function() {
      addon.uninstall();

      Services.obs.notifyObservers(xpi, "flush-cache-entry");
      xpi.remove(false);
    });

    return addon;
  });
}

add_task(async function() {
  gAddon = await installAddon({
    manifest: {
      "options_ui": {
        "page": "options.html",
      }
    },

    files: {
      "options.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="UTF-8">
            <style type="text/css">
              body > p {
                height: 300px;
                margin: 0;
              }
              body.bigger > p {
                height: 600px;
              }
            </style>
          </head>
          <body>
            <p>The quick mauve fox jumps over the opalescent dog.</p>
          </body>
        </html>`,
    },
  });

  // Create another add-on with no inline options, to verify that detail
  // view switches work correctly.
  gOtherAddon = await installAddon({});

  gManagerWindow = await open_manager("addons://list/extension");
  gCategoryUtilities = new CategoryUtilities(gManagerWindow);
});


async function openDetailsBrowser(addonId) {
  var addon = get_addon_element(gManagerWindow, addonId);

  is(addon.mAddon.optionsType, AddonManager.OPTIONS_TYPE_INLINE_BROWSER,
     "Options should be inline browser type");

  addon.parentNode.ensureElementIsVisible(addon);

  var button = gManagerWindow.document.getAnonymousElementByAttribute(addon, "anonid", "preferences-btn");

  is_element_visible(button, "Preferences button should be visible");

  EventUtils.synthesizeMouseAtCenter(button, { clickCount: 1 }, gManagerWindow);

  await TestUtils.topicObserved(AddonManager.OPTIONS_NOTIFICATION_DISPLAYED,
                                (subject, data) => data == addonId);

  is(gManagerWindow.gViewController.currentViewId,
     `addons://detail/${encodeURIComponent(addonId)}/preferences`,
     "Current view should scroll to preferences");

  var browser = gManagerWindow.document.querySelector(
    "#detail-grid > rows > stack > .inline-options-browser");
  var rows = browser.parentNode.parentNode;

  let url = await ContentTask.spawn(browser, {}, () => content.location.href);

  ok(browser, "Grid should have a browser descendant");
  is(browser.localName, "browser", "Grid should have a browser descendant");
  is(url, addon.mAddon.optionsURL, "Browser has the expected options URL loaded");

  is(browser.clientWidth, browser.parentNode.clientWidth,
     "Browser should be the same width as its direct parent");
  is(browser.clientWidth, rows.clientWidth,
     "Browser should be the same width as its rows ancestor");

  button = gManagerWindow.document.getElementById("detail-prefs-btn");
  is_element_hidden(button, "Preferences button should not be visible");

  return browser;
}


add_task(async function test_inline_browser_addon() {
  let browser = await openDetailsBrowser(gAddon.id);

  function checkHeights(expected) {
    let {clientHeight} = browser;
    return ContentTask.spawn(browser, {expected, clientHeight}, ({expected, clientHeight}) => {
      let {body} = content.document;

      is(body.clientHeight, expected, `Document body should be ${expected}px tall`);
      is(body.clientHeight, body.scrollHeight,
         "Document body should be tall enough to fit its contents");

      let heightDiff = clientHeight - expected;
      ok(heightDiff >= 0 && heightDiff < 50,
         `Browser should be slightly taller than the document body (${clientHeight} vs. ${expected})`);
    });
  }

  // Delay long enough to avoid hitting our resize rate limit.
  let delay = () => new Promise(resolve => setTimeout(resolve, 300));

  await delay();

  await checkHeights(300);

  info("Increase the document height, and expect the browser to grow correspondingly");
  await ContentTask.spawn(browser, null, () => {
    content.document.body.classList.toggle("bigger");
  });

  await delay();

  await checkHeights(600);

  info("Decrease the document height, and expect the browser to shrink correspondingly");
  await ContentTask.spawn(browser, null, () => {
    content.document.body.classList.toggle("bigger");
  });

  await delay();

  await checkHeights(300);

  await new Promise(resolve =>
    gCategoryUtilities.openType("extension", resolve));

  browser = gManagerWindow.document.querySelector(
    ".inline-options-browser");

  is(browser, null, "Options browser should be removed from the document");
});


// Test that loading an add-on with no inline browser works as expected
// after having viewed our main test add-on.
add_task(async function test_plain_addon() {
  var addon = get_addon_element(gManagerWindow, gOtherAddon.id);

  is(addon.mAddon.optionsType, null, "Add-on should have no options");

  addon.parentNode.ensureElementIsVisible(addon);

  await EventUtils.synthesizeMouseAtCenter(addon, { clickCount: 1 }, gManagerWindow);

  EventUtils.synthesizeMouseAtCenter(addon, { clickCount: 2 }, gManagerWindow);

  await BrowserTestUtils.waitForEvent(gManagerWindow, "ViewChanged");

  is(gManagerWindow.gViewController.currentViewId,
     `addons://detail/${encodeURIComponent(gOtherAddon.id)}`,
     "Detail view should be open");

  var browser = gManagerWindow.document.querySelector(
    "#detail-grid > rows > .inline-options-browser");

  is(browser, null, "Detail view should have no inline browser");

  await new Promise(resolve =>
    gCategoryUtilities.openType("extension", resolve));
});


// Test that loading the original add-on details successfully creates a
// browser.
add_task(async function test_inline_browser_addon_again() {
  let browser = await openDetailsBrowser(gAddon.id);

  await new Promise(resolve =>
    gCategoryUtilities.openType("extension", resolve));

  browser = gManagerWindow.document.querySelector(
    ".inline-options-browser");

  is(browser, null, "Options browser should be removed from the document");
});

add_task(async function() {
  await close_manager(gManagerWindow);

  gManagerWindow = null;
  gCategoryUtilities = null;
});
