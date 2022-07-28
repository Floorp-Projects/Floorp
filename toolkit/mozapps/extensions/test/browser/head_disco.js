/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* eslint max-len: ["error", 80] */

/* exported DISCOAPI_DEFAULT_FIXTURE, getCardContainer,
            getDiscoveryElement, promiseAddonInstall, promiseDiscopaneUpdate,
            promiseEvent, promiseObserved, readAPIResponseFixture */

/* globals RELATIVE_DIR, promisePopupNotificationShown,
           waitAppMenuNotificationShown */

"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const {
  ExtensionUtils: { promiseEvent, promiseObserved },
} = ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");

AddonTestUtils.initMochitest(this);

// The response to the discovery API, as documented at:
// https://addons-server.readthedocs.io/en/latest/topics/api/discovery.html
//
// The tests using this fixure are meant to verify that the discopane works
// with the latest AMO API.
// The following fixure file should be kept in sync with the content of
// latest AMO API response, e.g. from
//
// https://addons.allizom.org/api/v4/discovery/?lang=en-US
//
// The response must contain at least one theme, and one extension.
const DISCOAPI_DEFAULT_FIXTURE = PathUtils.join(
  Services.dirsvc.get("CurWorkD", Ci.nsIFile).path,
  ...RELATIVE_DIR.split("/"),
  "discovery",
  "api_response.json"
);

// Read the content of API_RESPONSE_FILE, and replaces any embedded URLs with
// URLs that point to the `amoServer` test server.
async function readAPIResponseFixture(
  amoTestHost,
  fixtureFilePath = DISCOAPI_DEFAULT_FIXTURE
) {
  let apiText = await IOUtils.readUTF8(fixtureFilePath);
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
    return `http://${amoTestHost}/${ext}?${url.pathname}${url.search}`;
  });

  return apiText;
}

// Wait until the current `<discovery-pane>` element has finished loading its
// cards. This can be used after the cards have been loaded.
function promiseDiscopaneUpdate(win) {
  let { cardsReady } = getCardContainer(win);
  ok(cardsReady, "Discovery cards should have started to initialize");
  return cardsReady;
}

function getCardContainer(win) {
  return getDiscoveryElement(win).querySelector("recommended-addon-list");
}

function getDiscoveryElement(win) {
  return win.document.querySelector("discovery-pane");
}

// A helper that waits until an installation has been requested from `amoServer`
// and proceeds with approving the installation.
async function promiseAddonInstall(
  amoServer,
  extensionData,
  expectedTelemetryInfo = { source: "disco", taarRecommended: false }
) {
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
    expectedTelemetryInfo,
    "The installed add-on should have the expected telemetry info"
  );
}
