/* eslint max-len: ["error", 80] */
"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const {
  ExtensionUtils: { promiseEvent, promiseObserved },
} = ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

// The response to the discovery API, as documented at:
// https://addons-server.readthedocs.io/en/latest/topics/api/discovery.html
//
// The test is designed to easily verify whether the discopane works with the
// latest AMO API, by replacing API_RESPONSE_FILE's content with latest AMO API
// response, e.g. from https://addons.allizom.org/api/v4/discovery/?lang=en-US
// The response must contain at least one theme, and one extension.
const API_RESPONSE_FILE = RELATIVE_DIR + "discovery/api_response.json";

const AMO_TEST_HOST = "rewritten-for-testing.addons.allizom.org";

const ArrayBufferInputStream = Components.Constructor(
  "@mozilla.org/io/arraybuffer-input-stream;1",
  "nsIArrayBufferInputStream",
  "setData"
);

AddonTestUtils.initMochitest(this);

const amoServer = AddonTestUtils.createHttpServer({ hosts: [AMO_TEST_HOST] });

amoServer.registerFile(
  "/png",
  FileUtils.getFile(
    "CurWorkD",
    `${RELATIVE_DIR}discovery/small-1x1.png`.split("/")
  )
);
amoServer.registerPathHandler("/dummy", (request, response) => {
  response.write("Dummy");
});

// `result` is an element in the `results` array from AMO's discovery API,
// stored in API_RESPONSE_FILE.
function getTestExpectationFromApiResult(result) {
  return {
    typeIsTheme: result.addon.type === "statictheme",
    addonName: result.addon.name,
    authorName: result.addon.authors[0].name,
    editorialHead: result.heading_text,
    editorialBody: result.description_text,
    dailyUsers: result.addon.average_daily_users,
    rating: result.addon.ratings.average,
  };
}

// Read the content of API_RESPONSE_FILE, and replaces any embedded URLs with
// URLs that point to the `amoServer` test server.
async function readAPIResponseFixture() {
  let apiText = await OS.File.read(API_RESPONSE_FILE, { encoding: "utf-8" });
  apiText = apiText.replace(/\bhttps?:\/\/[^"]+(?=")/g, url => {
    try {
      url = new URL(url);
    } catch (e) {
      // Responses may contain "http://*/*"; ignore it.
      return url;
    }
    // In this test, we only need to distinguish between different file types,
    // so just use the file extension as path name for amoServer.
    let ext = url.pathname.split(".").pop();
    return `http://${AMO_TEST_HOST}/${ext}?${url.pathname}${url.search}`;
  });

  return apiText;
}

// A helper to declare a response to discovery API requests.
class DiscoveryAPIHandler {
  constructor(responseText) {
    this.setResponseText(responseText);
    this.requestCount = 0;

    // Overwrite the previous discovery response handler.
    amoServer.registerPathHandler("/discoapi", this);
  }

  setResponseText(responseText) {
    this.responseBody = new TextEncoder().encode(responseText).buffer;
  }

  // Suspend discovery API requests until unblockResponses is called.
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
    ++this.requestCount;

    response.setHeader("Cache-Control", "no-cache", false);
    response.processAsync();
    await this._unblockPromise;

    let body = this.responseBody;
    let binStream = new ArrayBufferInputStream(body, 0, body.byteLength);
    response.bodyOutputStream.writeFrom(binStream, body.byteLength);
    response.finish();
  }
}

// Retrieve the list of visible action elements inside a document or container.
function getVisibleActions(documentOrElement) {
  return Array.from(documentOrElement.querySelectorAll("[action]")).filter(
    elem => elem.offsetWidth && elem.offsetHeight
  );
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
  let { cardsReady } = getCardContainer(win);
  ok(cardsReady, "Discovery cards should have started to initialize");
  return cardsReady;
}

// Switch to a different view so we can switch back to the discopane later.
async function switchToNonDiscoView(win) {
  // Listeners registered while the discopane was the active view continue to be
  // active when the view switches to the extensions list, because both views
  // share the same document.
  win.managerWindow.gViewController.loadView("addons://list/extensions");
  await wait_for_view_load(win.managerWindow);
  ok(
    win.document.querySelector("addon-list"),
    "Should be at the extension list view"
  );
}

// Switch to the discopane and wait until it has fully rendered, including any
// cards from the discovery API.
async function switchToDiscoView(win) {
  is(
    getDiscoveryElement(win),
    null,
    "Cannot switch to discopane when the discopane is already shown"
  );
  win.managerWindow.gViewController.loadView("addons://discover/");
  await wait_for_view_load(win.managerWindow);
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

// A helper that waits until an installation has been requested from `amoServer`
// and proceeds with approving the installation.
async function promiseAddonInstall(amoServer, extensionData) {
  let description = extensionData.manifest.description;
  let xpiFile = AddonTestUtils.createTempWebExtensionFile(extensionData);
  amoServer.registerFile("/xpi", xpiFile);

  let addonId = extensionData.manifest.applications.gecko.id;
  let installedPromise = waitAppMenuNotificationShown(
    "addon-installed",
    addonId,
    true
  );

  if (!extensionData.manifest.theme) {
    info(`${description}: Waiting for permission prompt`);
    // Extensions have install prompts.
    let panel = await promisePopupNotificationShown("addon-webext-permissions");
    panel.button.click();
  } else {
    info(`${description}: Waiting for install prompt`);
    let panel = await promisePopupNotificationShown(
      "addon-install-confirmation"
    );
    panel.button.click();
  }

  info("Waiting for post-install doorhanger");
  await installedPromise;

  let addon = await AddonManager.getAddonByID(addonId);
  Assert.deepEqual(
    addon.installTelemetryInfo,
    {
      // This is the expected source because before the HTML-based discopane,
      // "disco" was already used to mark installs from the AMO-hosted
      // discopane.
      source: "disco",
    },
    "The installed add-on should have the expected telemetry info"
  );
}

// Install an add-on by clicking on the card.
// The promise resolves once the card has been updated.
async function testCardInstall(card) {
  Assert.deepEqual(
    getVisibleActions(card).map(getActionName),
    ["install-addon"],
    "Should have an Install button before install"
  );

  let installButton =
    card.querySelector("[data-l10n-id='install-extension-button']") ||
    card.querySelector("[data-l10n-id='install-theme-button']");

  let updatePromise = promiseEvent(card, "disco-card-updated");
  installButton.click();
  await updatePromise;

  Assert.deepEqual(
    getVisibleActions(card).map(getActionName),
    ["manage-addon"],
    "Should have a Manage button after install"
  );
}

// Uninstall the add-on (not via the card, since it has no uninstall button).
// The promise resolves once the card has been updated.
async function testAddonUninstall(card) {
  Assert.deepEqual(
    getVisibleActions(card).map(getActionName),
    ["manage-addon"],
    "Should have a Manage button before uninstall"
  );

  let addon = await AddonManager.getAddonByID(card.addonId);

  let updatePromise = promiseEvent(card, "disco-card-updated");
  await addon.uninstall();
  await updatePromise;

  Assert.deepEqual(
    getVisibleActions(card).map(getActionName),
    ["install-addon"],
    "Should have an Install button after uninstall"
  );
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "extensions.getAddons.discovery.api_url",
        `http://${AMO_TEST_HOST}/discoapi`,
      ],
      ["extensions.htmlaboutaddons.discover.enabled", true],
      // Disable non-discopane recommendations to avoid unexpected discovery
      // API requests.
      ["extensions.htmlaboutaddons.recommendations.enabled", false],
      // Disable the telemetry client ID (and its associated UI warning).
      // browser_html_discover_view_clientid.js covers this functionality.
      ["browser.discovery.enabled", false],
    ],
  });
});

// Test that the discopane can be loaded and that meaningful results are shown.
// This relies on response data from the AMO API, stored in API_RESPONSE_FILE.
add_task(async function discopane_with_real_api_data() {
  const apiText = await readAPIResponseFixture();
  let apiHandler = new DiscoveryAPIHandler(apiText);

  const apiResultArray = JSON.parse(apiText).results;
  ok(apiResultArray.length, `Mock has ${Array.length} results`);

  apiHandler.blockNextResponses();
  let win = await loadInitialView("discover");

  Assert.deepEqual(
    getVisibleActions(win.document).map(getActionName),
    [],
    "The AMO button should be invisible when the AMO API hasn't responded"
  );

  apiHandler.unblockResponses();
  await promiseDiscopaneUpdate(win);

  let actionElements = getVisibleActions(win.document);
  Assert.deepEqual(
    actionElements.map(getActionName),
    [
      // Expecting an install button for every result.
      ...new Array(apiResultArray.length).fill("install-addon"),
      "open-amo",
    ],
    "All add-on cards should be rendered, with AMO button at the end."
  );

  let imgCount = await waitForAllImagesLoaded(win);
  is(imgCount, apiResultArray.length, "Expected an image for every result");

  // Check that the cards have the expected content.
  let cards = Array.from(
    win.document.querySelectorAll("recommended-addon-card")
  );
  is(cards.length, apiResultArray.length, "Every API result has a card");
  for (let [i, card] of cards.entries()) {
    let expectations = getTestExpectationFromApiResult(apiResultArray[i]);
    info(`Expectations for card ${i}: ${JSON.stringify(expectations)}`);

    let checkContent = (selector, expectation) => {
      let text = card.querySelector(selector).textContent;
      is(text, expectation, `Content of selector "${selector}"`);
    };
    checkContent(".disco-addon-name", expectations.addonName);
    await win.document.l10n.translateFragment(card);
    checkContent(
      ".disco-addon-author [data-l10n-name='author']",
      expectations.authorName
    );

    let amoListingLink = card.querySelector(".disco-addon-author a");
    ok(
      amoListingLink.search.includes("utm_source=firefox-browser"),
      `Listing link should have attribution parameter, url=${amoListingLink}`
    );

    let actions = getVisibleActions(card);
    is(actions.length, 1, "Card should only have one install button");
    let installButton = actions[0];
    if (expectations.typeIsTheme) {
      // Theme button + screenshot
      ok(
        installButton.matches("[data-l10n-id='install-theme-button'"),
        "Has theme install button"
      );
      ok(
        card.querySelector(".card-heading-image").offsetWidth,
        "Preview image must be visible"
      );
    } else {
      // Extension button + extended description.
      ok(
        installButton.matches("[data-l10n-id='install-extension-button'"),
        "Has extension install button"
      );
      checkContent(".disco-description-intro", expectations.editorialHead);
      checkContent(".disco-description-main", expectations.editorialBody);

      let ratingElem = card.querySelector("five-star-rating");
      if (expectations.rating) {
        is(ratingElem.rating, expectations.rating, "Expected rating value");
        ok(ratingElem.offsetWidth, "Rating element is visible");
      } else {
        is(ratingElem.offsetWidth, 0, "Rating element is not visible");
      }

      let userCountElem = card.querySelector(".disco-user-count");
      if (expectations.dailyUsers) {
        Assert.deepEqual(
          win.document.l10n.getAttributes(userCountElem),
          { id: "user-count", args: { dailyUsers: expectations.dailyUsers } },
          "Card count should be rendered"
        );
      } else {
        is(userCountElem.offsetWidth, 0, "User count element is not visible");
      }
    }
  }

  is(apiHandler.requestCount, 1, "Discovery API should be fetched once");

  await closeView(win);
});

// Test whether extensions and themes can be installed from the discopane.
// Also checks that items in the list do not change position after installation,
// and that they are shown at the bottom of the list when the discopane is
// reopened.
add_task(async function install_from_discopane() {
  const apiText = await readAPIResponseFixture();
  const apiResultArray = JSON.parse(apiText).results;
  let getAddonIdByAMOAddonType = type =>
    apiResultArray.find(r => r.addon.type === type).addon.guid;
  const FIRST_EXTENSION_ID = getAddonIdByAMOAddonType("extension");
  const FIRST_THEME_ID = getAddonIdByAMOAddonType("statictheme");

  let apiHandler = new DiscoveryAPIHandler(apiText);

  let win = await loadInitialView("discover");
  await promiseDiscopaneUpdate(win);
  await waitForAllImagesLoaded(win);

  Services.telemetry.clearEvents();

  // Test extension install.
  let installExtensionPromise = promiseAddonInstall(amoServer, {
    manifest: {
      name: "My Awesome Add-on",
      description: "Test extension install button",
      applications: { gecko: { id: FIRST_EXTENSION_ID } },
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
      applications: { gecko: { id: FIRST_THEME_ID } },
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
    "The Install buttons should be replaced with Manage buttons"
  );

  assertAboutAddonsTelemetryEvents([
    [
      "addonsManager",
      "action",
      "aboutAddons",
      null,
      {
        action: "installFromRecommendation",
        view: "discover",
        addonId: FIRST_EXTENSION_ID,
        type: "extension",
      },
    ],
    [
      "addonsManager",
      "action",
      "aboutAddons",
      null,
      {
        action: "installFromRecommendation",
        view: "discover",
        addonId: FIRST_THEME_ID,
        type: "theme",
      },
    ],
  ]);

  // End of the testing installation from a card.

  // Click on the Manage button to verify that it does something useful,
  // and in order to be able to force the discovery pane to be rendered again.
  let loaded = waitForViewLoad(win);
  getCardByAddonId(win, FIRST_EXTENSION_ID)
    .querySelector("[action='manage-addon']")
    .click();
  await loaded;
  {
    let addonCard = win.document.querySelector(
      `addon-card[addon-id="${FIRST_EXTENSION_ID}"]`
    );
    ok(addonCard, "Add-on details should be shown");
    ok(addonCard.expanded, "The card should have been expanded");
    // TODO bug 1540253: Check that the "recommended" badge is visible.
  }

  assertAboutAddonsTelemetryEvents(
    [
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        {
          action: "manage",
          view: "discover",
          addonId: FIRST_EXTENSION_ID,
          type: "extension",
        },
      ],
    ],
    { methods: ["action"] }
  );

  // Now we are going to force an updated rendering and check that the cards are
  // in the expected order, and then test uninstallation of the above add-ons.
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
    "Already-installed add-ons should be rendered at the end of the list"
  );

  promiseThemeChange = promiseObserved("lightweight-theme-styling-update");
  await testAddonUninstall(getCardByAddonId(win, FIRST_THEME_ID));
  await promiseThemeChange;
  await testAddonUninstall(getCardByAddonId(win, FIRST_EXTENSION_ID));

  is(apiHandler.requestCount, 1, "Discovery API should be fetched once");

  await closeView(win);
});

// Tests that the page is able to switch views while the discopane is loading,
// without inadvertently replacing the page when the request finishes.
add_task(async function discopane_navigate_while_loading() {
  let apiHandler = new DiscoveryAPIHandler(`{"results": []}`);

  apiHandler.blockNextResponses();
  let win = await loadInitialView("discover");

  let updatePromise = promiseDiscopaneUpdate(win);
  let didUpdateDiscopane = false;
  updatePromise.then(() => {
    didUpdateDiscopane = true;
  });

  // Switch views while the request is pending.
  await switchToNonDiscoView(win);

  is(
    didUpdateDiscopane,
    false,
    "discopane should still not be updated because the request is blocked"
  );
  is(
    getDiscoveryElement(win),
    null,
    "Discopane should be removed after switching to the extension list"
  );

  // Release pending requests, to verify that completing the request will not
  // cause changes to the visible view. The updatePromise will still resolve
  // though, because the event is dispatched to the removed `<discovery-pane>`.
  apiHandler.unblockResponses();

  await updatePromise;
  ok(
    win.document.querySelector("addon-list"),
    "Should still be at the extension list view"
  );
  is(
    getDiscoveryElement(win),
    null,
    "Discopane should not be in the document when it is not the active view"
  );

  is(apiHandler.requestCount, 1, "Discovery API should be fetched once");

  await closeView(win);
});

// Tests that invalid responses are handled correctly and not cached.
// Also verifies that the response is cached as long as the page is active,
// but not when the page is fully reloaded.
add_task(async function discopane_cache_api_responses() {
  const INVALID_RESPONSE_BODY = `{"This is some": invalid} JSON`;
  let apiHandler = new DiscoveryAPIHandler(INVALID_RESPONSE_BODY);

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
  is(apiHandler.requestCount, 1, "Discovery API should be fetched once");

  Assert.deepEqual(
    getVisibleActions(win.document).map(getActionName),
    ["open-amo"],
    "The AMO button should be visible even when the response was invalid"
  );

  // Change to a valid response, so that the next response will be cached.
  apiHandler.setResponseText(`{"results": []}`);

  await switchToNonDiscoView(win);
  await switchToDiscoView(win); // Request #2

  is(
    apiHandler.requestCount,
    2,
    "Should fetch new data because an invalid response should not be cached"
  );

  await switchToNonDiscoView(win);
  await switchToDiscoView(win);
  await closeView(win);

  is(
    apiHandler.requestCount,
    2,
    "The previous response was valid and should have been reused"
  );

  // Now open a new about:addons page and verify that a new API request is sent.
  let anotherWin = await loadInitialView("discover");
  await promiseDiscopaneUpdate(anotherWin);
  await closeView(anotherWin);

  is(apiHandler.requestCount, 3, "discovery API should be requested again");
});

add_task(async function discopane_no_cookies() {
  let requestPromise = new Promise(resolve => {
    amoServer.registerPathHandler("/discoapi", resolve);
  });
  Services.cookies.add(
    AMO_TEST_HOST,
    "/",
    "name",
    "value",
    false,
    false,
    false,
    Date.now() / 1000 + 600,
    {},
    Ci.nsICookie.SAMESITE_NONE
  );
  let win = await loadInitialView("discover");
  let request = await requestPromise;
  ok(!request.hasHeader("Cookie"), "discovery API should not receive cookies");
  await closeView(win);
});

// Telemetry for interaction that have not been covered by other tests yet.
// - "Find more addons" button.
// - Link to add-on listing in recommended add-on card.
// Other interaction is already covered elsewhere:
// - Install/Manage buttons have bee tested before in install_from_discopane.
// - "Learn more" button is checked by browser_html_discover_view_clientid.js.
add_task(async function discopane_interaction_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.getAddons.link.url", `http://${AMO_TEST_HOST}/dummy`]],
  });
  // Minimal API response to get the link in recommended-addon-card to render.
  const DUMMY_EXTENSION_ID = "dummy@extensionid";
  const apiResponse = {
    results: [
      {
        addon: {
          guid: DUMMY_EXTENSION_ID,
          type: "extension",
          authors: [
            {
              name: "Some author",
            },
          ],
          url: `http://${AMO_TEST_HOST}/dummy`,
          icon_url: `http://${AMO_TEST_HOST}/png`,
        },
      },
    ],
  };
  let apiHandler = new DiscoveryAPIHandler(JSON.stringify(apiResponse));

  let expectedAmoUrlFor = where => {
    // eslint-disable-next-line max-len
    return `http://${AMO_TEST_HOST}/dummy?utm_source=firefox-browser&utm_medium=firefox-browser&utm_content=${where}`;
  };

  let testClickInDiscoCard = async (selector, utmContentParam) => {
    let tabbrowser = win.windowRoot.ownerGlobal.gBrowser;
    let tabPromise = BrowserTestUtils.waitForNewTab(tabbrowser);
    getDiscoveryElement(win)
      .querySelector(selector)
      .click();
    let tab = await tabPromise;
    is(
      tab.linkedBrowser.currentURI.spec,
      expectedAmoUrlFor(utmContentParam),
      "Expected URL of new tab"
    );
    BrowserTestUtils.removeTab(tab);
  };

  let win = await loadInitialView("discover");
  await promiseDiscopaneUpdate(win);
  is(await waitForAllImagesLoaded(win), 1, "One recommendation in results");

  Services.telemetry.clearEvents();

  // "Find more add-ons" button.
  await testClickInDiscoCard("[action='open-amo']", "find-more-link-bottom");

  // Link to AMO listing
  await testClickInDiscoCard(".disco-addon-author a", "discopane-entry-link");

  assertAboutAddonsTelemetryEvents([
    ["addonsManager", "link", "aboutAddons", "discomore", { view: "discover" }],
    ["addonsManager", "link", "aboutAddons", "discohome", { view: "discover" }],
  ]);

  is(apiHandler.requestCount, 1, "Discovery API should be fetched once");

  await closeView(win);
  await SpecialPowers.popPrefEnv();
});

// The CSP of about:addons whitelists http:, but not data:, hence we are
// loading a little red data: image which gets blocked by the CSP.
add_task(async function csp_img_src() {
  const RED_DATA_IMAGE =
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAA" +
    "AHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";

  // Minimal API response to get the image in recommended-addon-card to render.
  const DUMMY_EXTENSION_ID = "dummy-csp@extensionid";
  const apiResponse = {
    results: [
      {
        addon: {
          guid: DUMMY_EXTENSION_ID,
          type: "extension",
          authors: [
            {
              name: "Some CSP author",
            },
          ],
          url: `http://${AMO_TEST_HOST}/dummy`,
          icon_url: RED_DATA_IMAGE,
        },
      },
    ],
  };

  let apiHandler = new DiscoveryAPIHandler(JSON.stringify(apiResponse));
  apiHandler.blockNextResponses();
  let win = await loadInitialView("discover");

  let cspPromise = new Promise(resolve => {
    win.addEventListener("securitypolicyviolation", e => {
      // non http(s) loads only report the scheme
      is(e.blockedURI, "data", "CSP: blocked URI");
      is(e.violatedDirective, "img-src", "CSP: violated directive");
      resolve();
    });
  });

  apiHandler.unblockResponses();
  await cspPromise;

  await closeView(win);
});
