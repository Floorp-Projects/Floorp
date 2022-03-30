/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* eslint max-len: ["error", 80] */

"use strict";

const { AMTelemetry } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

loadTestSubscript("head_disco.js");

const AMO_TEST_HOST = "rewritten-for-testing.addons.allizom.org";

const amoServer = AddonTestUtils.createHttpServer({ hosts: [AMO_TEST_HOST] });

const DISCO_URL = `http://${AMO_TEST_HOST}/discoapi`;

// Helper function to retrieve test addon ids from the fixture disco api results
// used in this test, this is set in the `setup` test task below.
let getAddonIdFromDiscoResult;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabling the Data Upload pref may upload data.
      // Point data reporting services to localhost so the data doesn't escape.
      ["toolkit.telemetry.server", "https://localhost:1337"],
      ["telemetry.fog.test.localhost_port", -1],
      ["extensions.getAddons.discovery.api_url", DISCO_URL],
    ],
  });

  let apiText = await readAPIResponseFixture(AMO_TEST_HOST);

  // Parse the Disco API fixture and store it for being used in the
  // test case to retrieve the recommended addon that match the needs
  // of the particular test case.
  const apiResultArray = JSON.parse(apiText).results;
  ok(apiResultArray.length, `Mock has ${apiResultArray.length} results`);

  // Ensure we do have one extension entry that has is_recommendation set
  // to true.
  let result = apiResultArray.find(r => r.addon.type === "extension");
  is(
    typeof result.is_recommendation,
    "boolean",
    "has is_recommendation property"
  );
  result.is_recommendation = true;

  // Define the helper we'll use in the test tasks that follows to pick an
  // addonId to test from the api fixtures.
  getAddonIdFromDiscoResult = selectParam => {
    const { type, is_recommendation } = selectParam;
    const addonId = apiResultArray.find(
      r => r.addon.type === type && r.is_recommendation === is_recommendation
    )?.addon.guid;
    ok(
      addonId,
      `Got a recommended addon id for ${JSON.stringify(
        selectParam
      )}: ${addonId}`
    );
    return addonId;
  };

  // Make sure that we have marked at least one extension as taar recommended.
  getAddonIdFromDiscoResult({ type: "extension", is_recommendation: true });

  // Stringify back the mock disco api result and register the HTTP server
  // path handler.
  apiText = JSON.stringify({ results: apiResultArray });

  amoServer.registerPathHandler("/discoapi", (request, response) => {
    response.write(apiText);
  });
});

async function switchToView(win, viewId) {
  win.gViewController.loadView(viewId);
  await wait_for_view_load(win);
}

async function run_view_telemetry_test(taarEnabled) {
  Services.telemetry.clearEvents();
  let win = await loadInitialView("discover");
  await switchToView(win, "addons://list/theme");
  await switchToView(win, "addons://list/plugin");
  await switchToView(win, "addons://list/extension");
  await closeView(win);

  const taar_enabled = AMTelemetry.convertToString(taarEnabled);

  assertAboutAddonsTelemetryEvents(
    [
      ["addonsManager", "view", "aboutAddons", "discover", { taar_enabled }],
      ["addonsManager", "view", "aboutAddons", "list", { type: "theme" }],
      ["addonsManager", "view", "aboutAddons", "list", { type: "plugin" }],
      ["addonsManager", "view", "aboutAddons", "list", { taar_enabled }],
    ],
    { category: "addonsManager", methods: ["view"] }
  );
}

async function run_install_recommended_telemetry_test(taarRecommended) {
  const extensionId = getAddonIdFromDiscoResult({
    type: "extension",
    // Pick an extension that match the taarBased expectation for the test
    // (otherwise we would need to change the fixture to cover the case where
    // the user did disable the taar-based recommendations).
    is_recommendation: taarRecommended,
  });
  const themeId = getAddonIdFromDiscoResult({
    type: "statictheme",
    is_recommendation: false,
  });

  Services.telemetry.clearEvents();
  let win = await loadInitialView("discover");
  await promiseDiscopaneUpdate(win);

  // Test extension install.
  info("Install recommended extension");
  let installExtensionPromise = promiseAddonInstall(
    amoServer,
    {
      manifest: {
        name: "A fake recommended extension",
        description: "Test disco taar telemetry",
        applications: { gecko: { id: extensionId } },
        permissions: ["<all_urls>"],
      },
    },
    { source: "disco", taarRecommended }
  );
  await testCardInstall(getCardByAddonId(win, extensionId));
  await installExtensionPromise;

  // Test theme install.
  info("Install recommended theme");
  let installThemePromise = promiseAddonInstall(
    amoServer,
    {
      manifest: {
        name: "A fake recommended theme",
        description: "Test disco taar telemetry",
        applications: { gecko: { id: themeId } },
        theme: {
          colors: {
            tab_selected: "red",
          },
        },
      },
    },
    { source: "disco", taarRecommended: false }
  );
  let promiseThemeChange = promiseObserved("lightweight-theme-styling-update");
  await testCardInstall(getCardByAddonId(win, themeId));
  await installThemePromise;
  await promiseThemeChange;

  await switchToView(win, "addons://list/extension");
  await closeView(win);

  info("Uninstall recommended addon");
  const extAddon = await AddonManager.getAddonByID(extensionId);
  await extAddon.uninstall();

  info("Uninstall recommended theme");
  promiseThemeChange = promiseObserved("lightweight-theme-styling-update");
  const themeAddon = await AddonManager.getAddonByID(themeId);
  await themeAddon.uninstall();
  await promiseThemeChange;

  const taar_based = AMTelemetry.convertToString(taarRecommended);
  assertAboutAddonsTelemetryEvents(
    [
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        {
          action: "installFromRecommendation",
          view: "discover",
          taar_based,
          addonId: extensionId,
          type: "extension",
        },
      ],
      ["addonsManager", "install_stats", "extension", /.*/, { taar_based }],
      [
        "addonsManager",
        "action",
        "aboutAddons",
        null,
        {
          action: "installFromRecommendation",
          view: "discover",
          // taar is expected to be always marked as not active for themes.
          taar_based: "0",
          addonId: themeId,
          type: "theme",
        },
      ],
      ["addonsManager", "install_stats", "theme", /.*/, { taar_based: "0" }],
    ],
    {
      methods: ["action", "install_stats"],
      objects: ["theme", "extension", "aboutAddons"],
    }
  );
}

add_task(async function test_taar_enabled_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Make sure taar-based recommendation are enabled.
      ["browser.discovery.enabled", true],
      ["datareporting.healthreport.uploadEnabled", true],
      ["extensions.htmlaboutaddons.recommendations.enabled", true],
    ],
  });

  const expectTaarEnabled = true;
  await run_view_telemetry_test(expectTaarEnabled);
  await run_install_recommended_telemetry_test(expectTaarEnabled);

  await SpecialPowers.popPrefEnv();
});

add_task(async function test_taar_disabled_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Make sure taar-based recommendations are disabled.
      ["browser.discovery.enabled", false],
      ["datareporting.healthreport.uploadEnabled", true],
      ["extensions.htmlaboutaddons.recommendations.enabled", true],
    ],
  });

  const expectTaarDisabled = false;
  await run_view_telemetry_test(expectTaarDisabled);
  await run_install_recommended_telemetry_test(expectTaarDisabled);

  await SpecialPowers.popPrefEnv();
});

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

function getCardByAddonId(win, addonId) {
  for (let card of win.document.querySelectorAll("recommended-addon-card")) {
    if (card.addonId === addonId) {
      return card;
    }
  }
  return null;
}

// Retrieve the list of visible action elements inside a document or container.
function getVisibleActions(documentOrElement) {
  return Array.from(documentOrElement.querySelectorAll("[action]")).filter(
    elem =>
      elem.getAttribute("action") !== "page-options" &&
      elem.offsetWidth &&
      elem.offsetHeight
  );
}

function getActionName(actionElement) {
  return actionElement.getAttribute("action");
}
