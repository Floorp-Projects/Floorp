/* eslint max-len: ["error", 80] */
"use strict";

const {ClientID} = ChromeUtils.import("resource://gre/modules/ClientID.jsm");

const {
  AddonTestUtils,
} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

const {
  TelemetryTestUtils,
} = ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm");

AddonTestUtils.initMochitest(this);
const server = AddonTestUtils.createHttpServer();
const serverBaseUrl = `http://localhost:${server.identity.primaryPort}/`;
server.registerPathHandler("/sumo/personalized-extension-recommendations",
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
  return getNoticeButton(win).closest("message-bar").offsetHeight > 0;
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
  let expectedUrl =
    `${serverBaseUrl}sumo/personalized-extension-recommendations`;
  let tabPromise = BrowserTestUtils.waitForNewTab(tabbrowser, expectedUrl);

  getNoticeButton(win).click();

  info(`Waiting for new tab with URL: ${expectedUrl}`);
  let tab = await tabPromise;
  BrowserTestUtils.removeTab(tab);

  await closeView(win);

  TelemetryTestUtils.assertEvents([{
    method: "link",
    value: "disconotice",
    extra: {
      view: "discover",
    },
  }], {
    category: "addonsManager",
    method: "link",
    object: "aboutAddons",
  });
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
