/* eslint max-len: ["error", 80] */
"use strict";

const {ClientID} = ChromeUtils.import("resource://gre/modules/ClientID.jsm");

const {
  AddonTestUtils,
} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

AddonTestUtils.initMochitest(this);
const server = AddonTestUtils.createHttpServer();
const serverBaseUrl = `http://localhost:${server.identity.primaryPort}/`;
server.registerPathHandler("/sumo/personalized-addons",
  (request, response) => {
    response.write("This is a SUMO page that explains personalized add-ons.");
  });

// Before a discovery API request is triggered, this method should be called.
// Resolves with the value of the "telemetry-client-id" query parameter.
async function promiseOneDiscoveryApiRequest() {
  return new Promise(resolve => {
    let requestCount = 0;
    // Overwrite previous request handler, if any.
    server.registerPathHandler("/discoapi", (request, response) => {
      is(++requestCount, 1, "Expecting one discovery API request");
      response.write(`{"results": []}`);
      let searchParams = new URLSearchParams(request.queryString);
      let clientId = searchParams.get("telemetry-client-id");
      resolve(clientId);
    });
  });
}

function getNoticeButton(win) {
  return win.document.querySelector("[action='notice-learn-more']");
}

function isNoticeVisible(win) {
  let message = win.document.querySelector("taar-notice");
  return message && message.offsetHeight > 0;
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable clientid - see Discovery.jsm for the first two prefs.
      ["browser.discovery.enabled", true],
      ["datareporting.healthreport.uploadEnabled", true],
      ["extensions.getAddons.discovery.api_url", `${serverBaseUrl}discoapi`],
      ["app.support.baseURL", `${serverBaseUrl}sumo/`],
      ["extensions.htmlaboutaddons.discover.enabled", true],
      ["extensions.htmlaboutaddons.enabled", true],
      // Discovery API requests can be triggered by the discopane and the
      // recommendations in the list view. To make sure that the every test
      // checks the behavior of the view they're testing, ensure that only one
      // of the two views is enabled at a time.
      ["extensions.htmlaboutaddons.recommendations.enabled", false],
    ],
  });
});

// Test that the clientid is passed to the API when enabled via prefs.
add_task(async function clientid_enabled() {
  let EXPECTED_CLIENT_ID = await ClientID.getClientIdHash();
  ok(EXPECTED_CLIENT_ID, "ClientID should be available");

  let requestPromise = promiseOneDiscoveryApiRequest();
  let win = await loadInitialView("discover");

  ok(isNoticeVisible(win), "Notice about personalization should be visible");

  // TODO: This should ideally check whether the result is the expected ID.
  // But run with --verify, the test may fail with EXPECTED_CLIENT_ID being
  // "baae8d197cf6b0865d7ba7ddf83829cd2d9844374d7271a5c704199d91059316",
  // which is sha256(TelemetryUtils.knownClientId).
  // This happens because at the end of the test, the pushPrefEnv from setup is
  // reverted, which resets datareporting.healthreport.uploadEnabled to false.
  // When TelemetryController.jsm detects this, it asynchronously resets the
  // ClientID to knownClientId - which may happen at the next run of the test.
  // TODO: Fix this together with bug 1537933
  //
  // is(await requestPromise, EXPECTED_CLIENT_ID,
  ok(await requestPromise,
     "Moz-Client-Id should be set when telemetry & discovery are enabled");

  Services.telemetry.clearEvents();

  let tabbrowser = win.windowRoot.ownerGlobal.gBrowser;
  let expectedUrl = `${serverBaseUrl}sumo/personalized-addons`;
  let tabPromise = BrowserTestUtils.waitForNewTab(tabbrowser, expectedUrl);

  getNoticeButton(win).click();

  info(`Waiting for new tab with URL: ${expectedUrl}`);
  let tab = await tabPromise;
  BrowserTestUtils.removeTab(tab);

  await closeView(win);

  assertAboutAddonsTelemetryEvents([
    ["addonsManager", "link", "aboutAddons", "disconotice", {view: "discover"}],
  ], {methods: ["link"]});
});

// Test that the clientid is not sent when disabled via prefs.
add_task(async function clientid_disabled() {
  // Temporarily override the prefs that we had set in setup.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.discovery.enabled", false]],
  });
  let requestPromise = promiseOneDiscoveryApiRequest();
  let win = await loadInitialView("discover");
  ok(!isNoticeVisible(win), "Notice about personalization should be hidden");
  is(await requestPromise, null,
     "Moz-Client-Id should not be sent when discovery is disabled");
  await closeView(win);
  await SpecialPowers.popPrefEnv();
});

// Test that the clientid is not sent from private windows.
add_task(async function clientid_from_private_window() {
  let privateWindow =
    await BrowserTestUtils.openNewBrowserWindow({private: true});

  let requestPromise = promiseOneDiscoveryApiRequest();
  let managerWindow =
    await open_manager("addons://discover/", null, null, null, privateWindow);
  ok(PrivateBrowsingUtils.isContentWindowPrivate(managerWindow),
     "Addon-manager is in a private window");

  is(await requestPromise, null,
     "Moz-Client-Id should not be sent in private windows");

  await close_manager(managerWindow);
  await BrowserTestUtils.closeWindow(privateWindow);
});

add_task(async function clientid_enabled_from_extension_list() {
  await SpecialPowers.pushPrefEnv({
    // Override prefs from setup to enable recommendations.
    set: [
      ["extensions.htmlaboutaddons.discover.enabled", false],
      ["extensions.htmlaboutaddons.recommendations.enabled", true],
    ],
  });

  // Force the extension list to be the first load. This pref will be
  // overwritten once the view loads.
  Services.prefs.setCharPref(PREF_UI_LASTCATEGORY, "addons://list/extension");

  let requestPromise = promiseOneDiscoveryApiRequest();
  let win = await loadInitialView("extension");

  ok(isNoticeVisible(win), "Notice about personalization should be visible");

  ok(await requestPromise,
     "Moz-Client-Id should be set when telemetry & discovery are enabled");

  // Make sure switching to the theme view doesn't trigger another request.
  await switchView(win, "theme");

  // Wait until the request would have happened so promiseOneDiscoveryApiRequest
  // can fail if it does.
  let recommendations = win.document.querySelector("recommended-addon-list");
  await recommendations.loadCardsIfNeeded();

  await closeView(win);
  await SpecialPowers.popPrefEnv();
});

add_task(async function clientid_enabled_from_theme_list() {
  await SpecialPowers.pushPrefEnv({
    // Override prefs from setup to enable recommendations.
    set: [
      ["extensions.htmlaboutaddons.discover.enabled", false],
      ["extensions.htmlaboutaddons.recommendations.enabled", true],
    ],
  });

  // Force the theme list to be the first load. This pref will be overwritten
  // once the view loads.
  Services.prefs.setCharPref(PREF_UI_LASTCATEGORY, "addons://list/theme");

  let requestPromise = promiseOneDiscoveryApiRequest();
  let win = await loadInitialView("theme");

  ok(!isNoticeVisible(win), "Notice about personalization should be hidden");

  is(await requestPromise, null,
     "Moz-Client-Id should not be sent when loading themes initially");

  info("Load the extension list and verify the client ID is now sent");

  requestPromise = promiseOneDiscoveryApiRequest();
  await switchView(win, "extension");

  ok(await requestPromise,
     "Moz-Client-Id is now sent for extensions");

  await closeView(win);
  await SpecialPowers.popPrefEnv();
});
