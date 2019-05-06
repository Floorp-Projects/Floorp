/* eslint max-len: ["error", 80] */
"use strict";

const {ClientID} = ChromeUtils.import("resource://gre/modules/ClientID.jsm");

const {
  AddonTestUtils,
} = ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");

AddonTestUtils.initMochitest(this);
const server = AddonTestUtils.createHttpServer();
const serverBaseUrl = `http://localhost:${server.identity.primaryPort}/`;

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

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable clientid - see Discovery.jsm for the first two prefs.
      ["browser.discovery.enabled", true],
      ["datareporting.healthreport.uploadEnabled", true],
      ["extensions.getAddons.discovery.api_url", `${serverBaseUrl}discoapi`],
      ["extensions.htmlaboutaddons.enabled", true],
    ],
  });
});

// Test that the clientid is passed to the API when enabled via prefs.
add_task(async function clientid_enabled() {
  let EXPECTED_CLIENT_ID = await ClientID.getClientIdHash();
  ok(EXPECTED_CLIENT_ID, "ClientID should be available");

  let requestPromise = promiseOneDiscoveryApiRequest();
  let win = await loadInitialView("discover");
  is(await requestPromise, EXPECTED_CLIENT_ID,
     "Moz-Client-Id should be set when telemetry & discovery are enabled");
  await closeView(win);
});

// Test that the clientid is not sent when disabled via prefs.
add_task(async function clientid_disabled() {
  // Temporarily override the prefs that we had set in setup.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.discovery.enabled", false]],
  });
  let requestPromise = promiseOneDiscoveryApiRequest();
  let win = await loadInitialView("discover");
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
