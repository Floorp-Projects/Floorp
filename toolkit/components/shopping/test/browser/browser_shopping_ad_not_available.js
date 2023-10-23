/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
      ["browser.shopping.experience2023.ads.enabled", true],
      ["browser.shopping.experience2023.ads.userEnabled", true],
    ],
  });
});

/**
 * Check that we send the right telemetry if no ad is available.
 */
add_task(async function test_no_ad_available_telemetry() {
  await BrowserTestUtils.withNewTab(OTHER_PRODUCT_TEST_URL, async browser => {
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");
    Assert.ok(sidebar, "Sidebar should exist");
    Assert.ok(
      BrowserTestUtils.is_visible(sidebar),
      "Sidebar should be visible."
    );

    info("Waiting for sidebar to update.");
    await promiseSidebarAdsUpdated(sidebar, OTHER_PRODUCT_TEST_URL);
    // Test the lack of ad was recorded by telemetry
    await Services.fog.testFlushAllChildren();
    let noAdsAvailableEvents =
      Glean.shopping.surfaceNoAdsAvailable.testGetValue();
    Assert.equal(
      noAdsAvailableEvents?.length,
      1,
      "Should have recorded lack of ads."
    );
    let noAdsEvent = noAdsAvailableEvents?.[0];
    Assert.equal(noAdsEvent?.category, "shopping");
    Assert.equal(noAdsEvent?.name, "surface_no_ads_available");
  });
});
