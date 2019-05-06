/* eslint max-len: ["error", 80] */
"use strict";

const {
  AddonTestUtils,
} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

const {
  ExtensionUtils: {
    getUniqueId,
    promiseEvent,
    promiseObserved,
  },
}  = ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

// The response to the discovery API, as documented at:
// https://addons-server.readthedocs.io/en/latest/topics/api/discovery.html
//
// The test is designed to easily verify whether the discopane works with the
// latest AMO API, by replacing API_RESPONSE_FILE's content with latest AMO API
// response, e.g. from https://addons.allizom.org/api/v4/discovery/?lang=en-US
// The response must contain at least one theme, and one extension.
const API_RESPONSE_FILE = RELATIVE_DIR + "discovery/api_response.json";

const AMO_TEST_HOST = "addons.example.com";

const ArrayBufferInputStream =
  Components.Constructor("@mozilla.org/io/arraybuffer-input-stream;1",
                         "nsIArrayBufferInputStream", "setData");

AddonTestUtils.initMochitest(this);

// `result` is an element in the `results` array from AMO's discovery API,
// stored in API_RESPONSE_FILE.
function getTestExpectationFromApiResult(result) {
  return {
    typeIsTheme: result.addon.type === "statictheme",
    addonName: result.addon.name,
    authorName: result.addon.authors[0].name,
    editorialHead: result.heading_text,
    editorialBody: result.description_text,
  };
}

/**
 * An internal server to support testing against the AMO API.
 *
 * // Start serving at http://example.com
 * let amoServer = new MockAPIServer("example.com");
 *
 * // Define the responses to be mocked:
 * amoServer.setResponseToFile("file", "path/to/real/file.txt"); // or nsIFile.
 * amoServer.setResponseToText(".txt", "actual content of file");
 *
 * // Suspend and resume responses from the server:
 * amoServer.blockNextResponses();
 * amoServer.unblockResponses();
 *
 * // Check request counters.
 * Assert.deepEqual(amoServer.requestCounters, {file: 1, ".txt": 1})
 *
 * // Unregister the server, so that a new MockAPIServer can be constructed at
 * // the next test.
 * await amoServer.unregister();
 */
class MockAPIServer {
  constructor(host) {
    this._resources = new Map();
    this.requestCounters = {};

    if (!MockAPIServer._servers) {
      MockAPIServer._servers = new Map();
    }
    if (!MockAPIServer._servers.has(host)) {
      MockAPIServer._servers.set(host,
        AddonTestUtils.createHttpServer({hosts: [host]}));
    }
    this.server = MockAPIServer._servers.get(host);
    this.server.registerPrefixHandler("/", this);
  }

  unregister() {
    // We cannot use server.stop() because AddonTestUtils.createHttpServer takes
    // care of that and does not expect callers to stop the server.
    // Do the next best thing, i.e. unregistering the request handler.
    this.server.registerPrefixHandler("/", null);
    this.server = null;
  }

  async setResponseToFile(pathSuffix, filepath) {
    if (filepath instanceof Ci.nsIFile) {
      filepath = filepath.path;
    } else {
      filepath = RELATIVE_DIR + filepath;
    }
    this._resources.set(pathSuffix, (await OS.File.read(filepath)).buffer);
  }

  setResponseToText(pathSuffix, text) {
    this._resources.set(pathSuffix, new TextEncoder().encode(text).buffer);
  }

  blockNextResponses() {
    this._unblockPromise = new Promise(resolve => {
      this.unblockResponses = resolve;
    });
  }

  unblockResponses(responseText) {
    throw new Error("You need to call blockNextResponses first!");
  }

  // nsIHttpRequestHandler::handle
  async handle(request, response) {
    let body = this._getResourceForPath(request.path);
    ok(body, `Must have response for: ${request.path}`);

    response.setHeader("Cache-Control", "no-cache");
    response.processAsync();
    await this._unblockPromise;

    let binStream = new ArrayBufferInputStream(body, 0, body.byteLength);
    response.bodyOutputStream.writeFrom(binStream, body.byteLength);
    response.finish();
  }

  _getResourceForPath(path) {
    for (let [suffix, body] of this._resources) {
      if (path.endsWith(suffix)) {
        this.requestCounters[suffix] = (this.requestCounters[suffix] || 0) + 1;
        return body;
      }
    }
    return null;
  }
}

// Creates a server and register |apiText| as the response to the discovery API
// for use with the discopane.
async function createAMOServer(apiText) {
  // Replace all URLs in the API response so that our server will intercept
  // requests to those URLs. And include a unique number in them to ensure that
  // every occurring URL will result in a new request.
  apiText = apiText.replace(
    /"https?:\/\/[^\/"]+/g,
    () => `"http://${AMO_TEST_HOST}/${getUniqueId()}}`);

  let amoServer = new MockAPIServer(AMO_TEST_HOST);
  amoServer.setResponseToText("discoapi", apiText);
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.getAddons.discovery.api_url",
           `http://${AMO_TEST_HOST}/discoapi`]],
  });
  return amoServer;
}

// Retrieve the list of visible action elements inside a document or container.
function getVisibleActions(documentOrElement) {
  return Array.from(documentOrElement.querySelectorAll("[action]"))
    .filter(elem => elem.offsetWidth && elem.offsetHeight);
}

function getActionName(actionElement) {
  return actionElement.getAttribute("action");
}

function getDiscoveryElement(win) {
  return win.document.querySelector("discovery-pane");
}

function getCardContainer(win) {
  return getDiscoveryElement(win).querySelector("recommended-addon-list");
}

function getCardByAddonId(win, addonId) {
  for (let card of win.document.querySelectorAll("recommended-addon-card")) {
    if (card.addonId === addonId) {
      return card;
    }
  }
  return null;
}

// Wait until the current `<discovery-pane>` element has finished loading its
// cards. This can be used after the cards have been loaded.
function promiseDiscopaneUpdate(win) {
  let {cardsReady} = getCardContainer(win);
  ok(cardsReady, "Discovery cards should have started to initialize");
  return cardsReady;
}

// Switch to a different view so we can switch back to the discopane later.
async function switchToNonDiscoView(win) {
  // Listeners registered while the discopane was the active view continue to be
  // active when the view switches to the extensions list, because both views
  // share the same document.
  let loaded = waitForViewLoad(win);
  win.managerWindow.gViewController.loadView("addons://list/extensions");
  await loaded;
  ok(win.document.querySelector("addon-list"),
     "Should be at the extension list view");
}

// Switch to the discopane and wait until it has fully rendered, including any
// cards from the discovery API.
async function switchToDiscoView(win) {
  is(getDiscoveryElement(win), null,
     "Cannot switch to discopane when the discopane is already shown");
  let loaded = waitForViewLoad(win);
  win.managerWindow.gViewController.loadView("addons://discover/");
  await loaded;
  await promiseDiscopaneUpdate(win);
}

// Wait until all images in the DOM have successfully loaded.
// There must be at least one `<img>` in the document.
// Returns the number of loaded images.
async function waitForAllImagesLoaded(win) {
  let imgs = Array.from(win.document.querySelectorAll("img[src]"));
  function areAllImagesLoaded() {
    let loadCount = imgs.filter(img => img.naturalWidth).length;
    info(`Loaded ${loadCount} out of ${imgs.length} images`);
    return loadCount === imgs.length;
  }
  if (!areAllImagesLoaded()) {
    await promiseEvent(win.document, "load", true, areAllImagesLoaded);
  }
  return imgs.length;
}

function checkEqualFloat(a, b, message) {
  let epsilon = 0.1;
  Assert.less(Math.abs(a - b), epsilon, `${message} - ${a} vs ${b}`);
}

/**
 * Checks whether all given elements have equivalent geometry.
 *
 * @param {HTMLElement[]} elements
 *        The elements whose dimensions are checked. The first element is used
 *        as a reference of how an element is supposed to look like.
 * @param {String[]} dimensions
 *        An array of dimension names that should be checked. Any of:
 *        "left", "right", "top", "bottom", "height", "width".
 * @param {string} desc
 *        Description of the test.
 */
function checkEqualGeometry(elements, dimensions, desc) {
  let reference = elements[0].getBoundingClientRect();
  for (let [i, element] of elements.entries()) {
    if (i === 0) {
      // Skip reference element.
      continue;
    }
    let test = element.getBoundingClientRect();
    for (let d of dimensions) {
      checkEqualFloat(test[d], reference[d], `${desc}: elements[${i}].${d}`);
    }
  }
}

// A helper that waits until an installation has been requested from `amoServer`
// and proceeds with approving the installation.
async function promiseAddonInstall(amoServer, extensionData) {
  let description = extensionData.manifest.description;
  let xpiFile = AddonTestUtils.createTempWebExtensionFile(extensionData);
  amoServer.setResponseToFile("xpi", xpiFile);

  let addonId = extensionData.manifest.applications.gecko.id;
  let installedPromise =
    waitAppMenuNotificationShown("addon-installed", addonId, true);

  if (!extensionData.manifest.theme) {
    info(`${description}: Waiting for permission prompt`);
    // Extensions have install prompts.
    let panel = await promisePopupNotificationShown("addon-webext-permissions");
    panel.button.click();
  } else {
    info(`${description}: Waiting for install prompt`);
    let panel =
      await promisePopupNotificationShown("addon-install-confirmation");
    panel.button.click();
  }

  info("Waiting for post-install doorhanger");
  await installedPromise;

  let addon = await AddonManager.getAddonByID(addonId);
  Assert.deepEqual(addon.installTelemetryInfo, {
    // This is the expected source because before the HTML-based discopane,
    // "disco" was already used to mark installs from the AMO-hosted discopane.
    source: "disco",
  }, "The installed add-on should have the expected telemetry info");
}

// Install an add-on by clicking on the card.
// The promise resolves once the card has been updated.
async function testCardInstall(card) {
  Assert.deepEqual(
    getVisibleActions(card).map(getActionName),
    ["install-addon"],
    "Should have an Install button before install");

  let installButton =
    card.querySelector("[data-l10n-id='install-extension-button']") ||
    card.querySelector("[data-l10n-id='install-theme-button']");

  let updatePromise = promiseEvent(card, "disco-card-updated");
  installButton.click();
  await updatePromise;

  Assert.deepEqual(
    getVisibleActions(card).map(getActionName),
    ["manage-addon"],
    "Should have a Manage button after install");
}

// Uninstall the add-on (not via the card, since it has no uninstall button).
// The promise resolves once the card has been updated.
async function testAddonUninstall(card) {
  Assert.deepEqual(
    getVisibleActions(card).map(getActionName),
    ["manage-addon"],
    "Should have a Manage button before uninstall");

  let addon = await AddonManager.getAddonByID(card.addonId);

  let updatePromise = promiseEvent(card, "disco-card-updated");
  await addon.uninstall();
  await updatePromise;

  Assert.deepEqual(
    getVisibleActions(card).map(getActionName),
    ["install-addon"],
    "Should have an Install button after uninstall");
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable HTML for all because some tests load non-discopane views.
      ["extensions.htmlaboutaddons.enabled", true],
      ["extensions.htmlaboutaddons.discover.enabled", true],
      // Disable the telemetry client ID (and its associated UI warning).
      // browser_html_discover_view_clientid.js covers this functionality.
      ["browser.discovery.enabled", false],
    ],
  });
});

// Test that the discopane can be loaded and that meaningful results are shown.
// This relies on response data from the AMO API, stored in API_RESPONSE_FILE.
add_task(async function discopane_with_real_api_data() {
  const apiText = await OS.File.read(API_RESPONSE_FILE, {encoding: "utf-8"});
  const amoServer = await createAMOServer(apiText);

  const apiResultArray = JSON.parse(apiText).results;
  ok(apiResultArray.length, `Mock has ${Array.length} results`);

  // Map images to a valid image file, so that waitForAllImagesLoaded finishes.
  amoServer.setResponseToFile("png", "discovery/small-1x1.png");

  amoServer.blockNextResponses();
  let win = await loadInitialView("discover");

  Assert.deepEqual(
    getVisibleActions(win.document).map(getActionName),
    [],
    "The AMO button should be invisible when the AMO API hasn't responded");

  amoServer.unblockResponses();
  await promiseDiscopaneUpdate(win);

  let actionElements = getVisibleActions(win.document);
  Assert.deepEqual(
    actionElements.map(getActionName),
    [
      // Expecting an install button for every result.
      ...new Array(apiResultArray.length).fill("install-addon"),
      "open-amo",
    ],
    "All add-on cards should be rendered, with AMO button at the end.");

  await waitForAllImagesLoaded(win);

  // Check position of install buttons.
  {
    let installThemeButtons = actionElements.filter(
      e => e.matches("[data-l10n-id='install-theme-button']"));
    let installExtensionButtons = actionElements.filter(
      e => e.matches("[data-l10n-id='install-extension-button']"));

    ok(installThemeButtons.length, "There must be at least one theme");
    ok(installExtensionButtons.length, "There must be at least one extension");
    is(installThemeButtons.length + installExtensionButtons.length,
       apiResultArray.length,
      "The only install buttons are for extensions and themes.");

    checkEqualGeometry(installThemeButtons,
                       ["left", "right", "width", "height"],
                       "Geometry of theme install buttons should be equal");
    checkEqualGeometry(installExtensionButtons,
                       ["left", "right", "width", "height"],
                       "Geometry of extension install buttons should be equal");
    // The width/left offset may differ due to different button labels.
    checkEqualGeometry(
      [installThemeButtons[0], installExtensionButtons[0]],
      ["right", "height"],
      "Extension and theme install buttons should be aligned at the right.");
  }

  // Check that the cards have the expected content.
  let cards =
    Array.from(win.document.querySelectorAll("recommended-addon-card"));
  is(cards.length, apiResultArray.length, "Every API result has a card");
  for (let [i, card] of cards.entries()) {
    let expectations = getTestExpectationFromApiResult(apiResultArray[i]);
    info(`Expectations for card ${i}: ${JSON.stringify(expectations)}`);

    let checkContent = (selector, expectation) => {
      let text = card.querySelector(selector).textContent;
      is(text, expectation, `Content of selector "${selector}"`);
    };
    checkContent(".disco-addon-name", expectations.addonName);
    checkContent(".disco-addon-author [data-l10n-name='author']",
                 expectations.authorName);

    let actions = getVisibleActions(card);
    is(actions.length, 1, "Card should only have one install button");
    let installButton = actions[0];
    if (expectations.typeIsTheme) {
      // Theme button + screenshot
      ok(installButton.matches("[data-l10n-id='install-theme-button'"),
         "Has theme install button");
      ok(card.querySelector(".card-heading-image").offsetWidth,
         "Preview image must be visible");
    } else {
      // Extension button + extended description.
      ok(installButton.matches("[data-l10n-id='install-extension-button'"),
         "Has extension install button");
      checkContent(".disco-description-intro", expectations.editorialHead);
      checkContent(".disco-description-main", expectations.editorialBody);
    }
  }

  Assert.deepEqual(amoServer.requestCounters, {
    // The discovery API should be fetched only once.
    discoapi: 1,
    // Every card has either one extension icon, or one theme preview.
    png: apiResultArray.length,
  }, "request counters for discopane load with AMO API data");

  await closeView(win);
  amoServer.unregister();
});

// Test whether extensions and themes can be installed from the discopane.
// Also checks that items in the list do not change position after installation,
// and that they are shown at the bottom of the list when the discopane is
// reopened.
add_task(async function install_from_discopane() {
  const apiText = await OS.File.read(API_RESPONSE_FILE, {encoding: "utf-8"});
  const apiResultArray = JSON.parse(apiText).results;
  let getAddonIdByAMOAddonType =
    type => apiResultArray.find(r => r.addon.type === type).addon.guid;
  const FIRST_EXTENSION_ID = getAddonIdByAMOAddonType("extension");
  const FIRST_THEME_ID = getAddonIdByAMOAddonType("statictheme");

  let amoServer = await createAMOServer(apiText);
  // Map images to a valid image file, so that waitForAllImagesLoaded finishes.
  amoServer.setResponseToFile("png", "discovery/small-1x1.png");

  let win = await loadInitialView("discover");
  await promiseDiscopaneUpdate(win);
  let imageCount = await waitForAllImagesLoaded(win);

  // Test extension install.
  let installExtensionPromise = promiseAddonInstall(amoServer, {
    manifest: {
      name: "My Awesome Add-on",
      description: "Test extension install button",
      applications: {gecko: {id: FIRST_EXTENSION_ID}},
      permissions: ["<all_urls>"],
    },
  });
  await testCardInstall(getCardByAddonId(win, FIRST_EXTENSION_ID));
  await installExtensionPromise;

  // Test theme install.
  let installThemePromise = promiseAddonInstall(amoServer, {
    manifest: {
      name: "My Fancy Theme",
      description: "Test theme install button",
      applications: {gecko: {id: FIRST_THEME_ID}},
      theme: {
        colors: {
          tab_selected: "red",
        },
      },
    },
  });
  let promiseThemeChange = promiseObserved("lightweight-theme-styling-update");
  await testCardInstall(getCardByAddonId(win, FIRST_THEME_ID));
  await installThemePromise;
  await promiseThemeChange;

  // After installing, the cards should have manage buttons instead of install
  // buttons. The cards should still be at the top of the pane (and not be
  // moved to the bottom).
  Assert.deepEqual(
    getVisibleActions(win.document).map(getActionName),
    [
      "manage-addon",
      "manage-addon",
      ...new Array(apiResultArray.length - 2).fill("install-addon"),
      "open-amo",
    ],
    "The Install buttons should be replaced with Manage buttons");

  Assert.deepEqual(amoServer.requestCounters, {
    discoapi: 1,
    png: imageCount,
    xpi: 2,
  }, "Request counters after add-on installs");

  // End of the testing installation from a card.
  // Now we are going to force an updated rendering and check that the cards are
  // in the expected order, and then test uninstallation of the above add-ons.

  // Force the pane to render again.
  await switchToNonDiscoView(win);
  await switchToDiscoView(win);
  await waitForAllImagesLoaded(win);

  Assert.deepEqual(
    getVisibleActions(win.document).map(getActionName),
    [
      ...new Array(apiResultArray.length - 2).fill("install-addon"),
      "manage-addon",
      "manage-addon",
      "open-amo",
    ],
    "Already-installed add-ons should be rendered at the end of the list");

  // The images may or may not have been loaded from the cache; we don't care.
  amoServer.requestCounters = {}; // Reset counters.

  await testAddonUninstall(getCardByAddonId(win, FIRST_THEME_ID));
  await testAddonUninstall(getCardByAddonId(win, FIRST_EXTENSION_ID));

  Assert.deepEqual(amoServer.requestCounters, {
  }, "Should not trigger new requests when an add-on is uninstalled");

  await closeView(win);
  amoServer.unregister();
});

// Tests that the page is able to switch views while the discopane is loading,
// without inadvertently replacing the page when the request finishes.
add_task(async function discopane_navigate_while_loading() {
  let amoServer = await createAMOServer(`{"results": []}`);

  amoServer.blockNextResponses();
  let win = await loadInitialView("discover");

  let updatePromise = promiseDiscopaneUpdate(win);
  let didUpdateDiscopane = false;
  updatePromise.then(() => { didUpdateDiscopane = true; });

  // Switch views while the request is pending.
  await switchToNonDiscoView(win);

  is(didUpdateDiscopane, false,
     "discopane should still not be updated because the request is blocked");
  is(getDiscoveryElement(win), null,
     "Discopane should be removed after switching to the extension list");

  // Release pending requests, to verify that completing the request will not
  // cause changes to the visible view. The updatePromise will still resolve
  // though, because the event is dispatched to the removed `<discovery-pane>`.
  amoServer.unblockResponses();

  await updatePromise;
  ok(win.document.querySelector("addon-list"),
     "Should still be at the extension list view");
  is(getDiscoveryElement(win), null,
     "Discopane should not be in the document when it is not the active view");

  Assert.deepEqual(amoServer.requestCounters, {
    discoapi: 1,
  }, "discovery API should be requested once");

  await closeView(win);
  amoServer.unregister();
});

// Tests that invalid responses are handled correctly and not cached.
// Also verifies that the response is cached as long as the page is active,
// but not when the page is fully reloaded.
add_task(async function discopane_cache_api_responses() {
  const INVALID_RESPONSE_BODY = `{"This is some": invalid} JSON`;
  let amoServer = await createAMOServer(INVALID_RESPONSE_BODY);

  let expectedErrMsg;
  try {
    JSON.parse(INVALID_RESPONSE_BODY);
    ok(false, "JSON.parse should have thrown");
  } catch (e) {
    expectedErrMsg = e.message;
  }

  let invalidResponseHandledPromise = new Promise(resolve => {
    Services.console.registerListener(function listener(msg) {
      if (msg.message.includes(expectedErrMsg)) {
        resolve();
        Services.console.unregisterListener(listener);
      }
    });
  });

  let win = await loadInitialView("discover"); // Request #1
  await promiseDiscopaneUpdate(win);

  info("Waiting for expected error");
  await invalidResponseHandledPromise;

  Assert.deepEqual(
    getVisibleActions(win.document).map(getActionName),
    ["open-amo"],
    "The AMO button should be visible even when the response was invalid");

  // Change to a valid response, so that the next response will be cached.
  amoServer.setResponseToText("discoapi", `{"results": []}`);

  await switchToNonDiscoView(win);
  await switchToDiscoView(win); // Request #2

  Assert.deepEqual(amoServer.requestCounters, {
    discoapi: 2,
  }, "Should fetch new data because an invalid response should not be cached");

  amoServer.requestCounters = {}; // Reset counters.

  await switchToNonDiscoView(win);
  await switchToDiscoView(win);
  await closeView(win);

  Assert.deepEqual(amoServer.requestCounters, {
  }, "The previous response was valid and should have been reused");

  // Now open a new about:addons page and verify that a new API request is sent.
  let anotherWin = await loadInitialView("discover");
  await promiseDiscopaneUpdate(anotherWin);
  await closeView(anotherWin);

  Assert.deepEqual(amoServer.requestCounters, {
    discoapi: 1,
  }, "discovery API should be requested again");

  amoServer.unregister();
});
