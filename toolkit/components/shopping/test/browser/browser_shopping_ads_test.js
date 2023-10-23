/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

function recommendedAdsEventListener(eventName, sidebar) {
  return SpecialPowers.spawn(
    sidebar.querySelector("browser"),
    [eventName],
    name => {
      let shoppingContainer =
        content.document.querySelector("shopping-container").wrappedJSObject;
      let adEl = shoppingContainer.recommendedAdEl;
      return ContentTaskUtils.waitForEvent(adEl, name, false, null, true).then(
        ev => null
      );
    }
  );
}

function recommendedAdVisible(sidebar) {
  return SpecialPowers.spawn(sidebar.querySelector("browser"), [], async () => {
    await ContentTaskUtils.waitForCondition(() => {
      let shoppingContainer =
        content.document.querySelector("shopping-container").wrappedJSObject;
      return (
        shoppingContainer?.recommendedAdEl &&
        ContentTaskUtils.is_visible(shoppingContainer?.recommendedAdEl)
      );
    });
  });
}

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

add_task(async function test_ad_attribution() {
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async browser => {
    // Test that impression event is fired when opening sidebar
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");
    Assert.ok(sidebar, "Sidebar should exist");
    Assert.ok(
      BrowserTestUtils.is_visible(sidebar),
      "Sidebar should be visible."
    );
    let shoppingButton = document.getElementById("shopping-sidebar-button");
    ok(
      BrowserTestUtils.is_visible(shoppingButton),
      "Shopping Button should be visible on a product page"
    );

    info("Waiting for sidebar to update.");
    await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);
    await recommendedAdVisible(sidebar);

    let impressionEvent = recommendedAdsEventListener("AdImpression", sidebar);

    info("Verifying product info for initial product.");
    await verifyProductInfo(sidebar, {
      productURL: PRODUCT_TEST_URL,
      adjustedRating: "4.1",
      letterGrade: "B",
    });

    info("Waiting for ad impression event.");
    await impressionEvent;
    Assert.ok(true, "Got ad impression event");

    // Test the impression was recorded by telemetry
    await Services.fog.testFlushAllChildren();
    var adsImpressionEvents =
      Glean.shopping.surfaceAdsImpression.testGetValue();
    Assert.equal(
      adsImpressionEvents.length,
      1,
      "should have recorded an event"
    );
    Assert.equal(adsImpressionEvents[0].category, "shopping");
    Assert.equal(adsImpressionEvents[0].name, "surface_ads_impression");

    //
    // Test that impression event is fired after switching to a tab that was
    // opened in the background

    let tab = BrowserTestUtils.addTab(gBrowser, PRODUCT_TEST_URL);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    let tabSidebar = gBrowser
      .getPanel(tab.linkedBrowser)
      .querySelector("shopping-sidebar");
    Assert.ok(tabSidebar, "Sidebar should exist");

    info("Waiting for sidebar to update.");
    await promiseSidebarUpdated(tabSidebar, PRODUCT_TEST_URL);
    await recommendedAdVisible(tabSidebar);

    // Need to wait the impression timeout to confirm that no impression event
    // has been dispatched
    // Bug 1859029 should update this to use sinon fake timers instead of using
    // setTimeout
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 2000));

    let hasImpressed = await SpecialPowers.spawn(
      tabSidebar.querySelector("browser"),
      [],
      () => {
        let shoppingContainer =
          content.document.querySelector("shopping-container").wrappedJSObject;
        let adEl = shoppingContainer.recommendedAdEl;
        return adEl.hasImpressed;
      }
    );
    Assert.ok(!hasImpressed, "We haven't seend the ad yet");

    impressionEvent = recommendedAdsEventListener("AdImpression", tabSidebar);
    await BrowserTestUtils.switchTab(gBrowser, tab);
    await recommendedAdVisible(tabSidebar);

    info("Waiting for ad impression event.");
    await impressionEvent;
    Assert.ok(true, "Got ad impression event");

    //
    // Test that the impression event is fired after opening foreground tab,
    // switching away and the event is not fired, then switching back and the
    // event does fire

    gBrowser.removeTab(tab);

    tab = BrowserTestUtils.addTab(gBrowser, PRODUCT_TEST_URL);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    tabSidebar = gBrowser
      .getPanel(tab.linkedBrowser)
      .querySelector("shopping-sidebar");
    Assert.ok(tabSidebar, "Sidebar should exist");

    info("Waiting for sidebar to update.");
    await promiseSidebarUpdated(tabSidebar, PRODUCT_TEST_URL);
    await recommendedAdVisible(tabSidebar);

    // Switch to new sidebar tab
    await BrowserTestUtils.switchTab(gBrowser, tab);
    // switch back to original tab
    await BrowserTestUtils.switchTab(
      gBrowser,
      gBrowser.getTabForBrowser(browser)
    );

    // Need to wait the impression timeout to confirm that no impression event
    // has been dispatched
    // Bug 1859029 should update this to use sinon fake timers instead of using
    // setTimeout
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 2000));

    hasImpressed = await SpecialPowers.spawn(
      tabSidebar.querySelector("browser"),
      [],
      () => {
        let shoppingContainer =
          content.document.querySelector("shopping-container").wrappedJSObject;
        let adEl = shoppingContainer.recommendedAdEl;
        return adEl.hasImpressed;
      }
    );
    Assert.ok(!hasImpressed, "We haven't seend the ad yet");

    impressionEvent = recommendedAdsEventListener("AdImpression", tabSidebar);
    await BrowserTestUtils.switchTab(gBrowser, tab);
    await recommendedAdVisible(tabSidebar);

    info("Waiting for ad impression event.");
    await impressionEvent;
    Assert.ok(true, "Got ad impression event");

    gBrowser.removeTab(tab);

    //
    // Test ad clicked event

    let adOpenedTabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      PRODUCT_TEST_URL,
      true
    );

    let clickedEvent = recommendedAdsEventListener("AdClicked", sidebar);
    await SpecialPowers.spawn(sidebar.querySelector("browser"), [], () => {
      let shoppingContainer =
        content.document.querySelector("shopping-container").wrappedJSObject;
      let adEl = shoppingContainer.recommendedAdEl;
      adEl.linkEl.click();
    });

    let adTab = await adOpenedTabPromise;

    info("Waiting for ad clicked event.");
    await clickedEvent;
    Assert.ok(true, "Got ad clicked event");

    // Test the click was recorded by telemetry
    await Services.fog.testFlushAllChildren();
    var adsClickedEvents = Glean.shopping.surfaceAdsClicked.testGetValue();
    Assert.equal(adsClickedEvents.length, 1, "should have recorded a click");
    Assert.equal(adsClickedEvents[0].category, "shopping");
    Assert.equal(adsClickedEvents[0].name, "surface_ads_clicked");

    gBrowser.removeTab(adTab);
    Services.fog.testResetFOG();
  });
});
