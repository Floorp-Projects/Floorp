/* eslint max-len: ["error", 80] */
"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);
const server = AddonTestUtils.createHttpServer();
const TEST_API_URL = `http://localhost:${server.identity.primaryPort}/discoapi`;

async function checkIfDiscoverVisible(expectVisible) {
  let requestCount = 0;
  let requestPromise = new Promise(resolve => {
    // Overwrites previous request handler, if any.
    server.registerPathHandler("/discoapi", (request, response) => {
      ++requestCount;
      response.write(`{"results": []}`);
      resolve();
    });
  });

  // Open about:addons with default view.
  let managerWindow = await open_manager(null);
  let categoryUtilities = new CategoryUtilities(managerWindow);

  is(
    categoryUtilities.isTypeVisible("discover"),
    expectVisible,
    "Visibility of discopane"
  );

  await wait_for_view_load(managerWindow);
  if (expectVisible) {
    is(
      categoryUtilities.selectedCategory,
      "discover",
      "Expected discopane as the default view"
    );
    await requestPromise;
    is(requestCount, 1, "Expected discovery API request");
  } else {
    // The next view (after discopane) is the extension list.
    is(
      categoryUtilities.selectedCategory,
      "extension",
      "Should fall back to another view when the discopane is disabled"
    );
    is(requestCount, 0, "Discovery API should not be requested");
  }

  await close_manager(managerWindow);
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.getAddons.discovery.api_url", TEST_API_URL],
      // Disable recommendations at the HTML about:addons view to avoid sending
      // a discovery API request from the fallback view (extension list) in the
      // showPane_false test.
      ["extensions.htmlaboutaddons.recommendations.enabled", false],
    ],
  });
});

add_task(async function showPane_true() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_DISCOVER_ENABLED, true]],
    clear: [[PREF_UI_LASTCATEGORY]],
  });
  await checkIfDiscoverVisible(true);
  await SpecialPowers.popPrefEnv();
});

add_task(async function showPane_false() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_DISCOVER_ENABLED, false]],
    clear: [[PREF_UI_LASTCATEGORY]],
  });
  await checkIfDiscoverVisible(false);
  await SpecialPowers.popPrefEnv();
});
