/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PRODUCT_TEST_URL = "https://example.com/Some-Product/dp/ABCDEFG123";
const OTHER_PRODUCT_TEST_URL =
  "https://example.com/Another-Product/dp/HIJKLMN456";

async function verifyProductInfo(sidebar, expectedProductInfo) {
  await SpecialPowers.spawn(
    sidebar.querySelector("browser"),
    [expectedProductInfo],
    async prodInfo => {
      let doc = content.document;
      let container = doc.querySelector("shopping-container");
      let root = container.shadowRoot;
      let reviewReliability = root.querySelector("review-reliability");
      // The async fetch could take some time.
      while (!reviewReliability) {
        info("Waiting for update.");
        await container.updateComplete;
      }
      let adjustedRating = root.querySelector("adjusted-rating");
      Assert.equal(
        reviewReliability.getAttribute("letter"),
        prodInfo.letterGrade,
        `Should have correct letter grade for product ${prodInfo.id}.`
      );
      Assert.equal(
        adjustedRating.getAttribute("rating"),
        prodInfo.adjustedRating,
        `Should have correct adjusted rating for product ${prodInfo.id}.`
      );
      Assert.equal(
        content.windowGlobalChild
          .getExistingActor("ShoppingSidebar")
          ?.getProductURI()?.spec,
        prodInfo.productURL,
        `Should have correct url in the child.`
      );
    }
  );
}

add_task(async function test_sidebar_navigation() {
  await BrowserTestUtils.withNewTab(PRODUCT_TEST_URL, async browser => {
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");
    Assert.ok(sidebar, "Sidebar should exist");
    Assert.ok(
      BrowserTestUtils.is_visible(sidebar),
      "Sidebar should be visible."
    );
    info("Waiting for sidebar to update.");
    await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);
    info("Verifying product info for initial product.");
    await verifyProductInfo(sidebar, {
      productURL: PRODUCT_TEST_URL,
      adjustedRating: "4.1",
      letterGrade: "B",
    });

    // Navigate the browser from the parent:
    let loadedPromise = Promise.all([
      BrowserTestUtils.browserLoaded(browser, false, OTHER_PRODUCT_TEST_URL),
      promiseSidebarUpdated(sidebar, OTHER_PRODUCT_TEST_URL),
    ]);
    BrowserTestUtils.loadURIString(browser, OTHER_PRODUCT_TEST_URL);
    info("Loading another product.");
    await loadedPromise;
    Assert.ok(sidebar, "Sidebar should exist.");
    Assert.ok(
      BrowserTestUtils.is_visible(sidebar),
      "Sidebar should be visible."
    );
    info("Verifying another product.");
    await verifyProductInfo(sidebar, {
      productURL: OTHER_PRODUCT_TEST_URL,
      adjustedRating: "1",
      letterGrade: "F",
    });

    // Navigate to a non-product URL:
    loadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      "https://example.com/1"
    );
    BrowserTestUtils.loadURIString(browser, "https://example.com/1");
    info("Go to a non-product.");
    await loadedPromise;
    Assert.ok(BrowserTestUtils.is_hidden(sidebar));

    // Navigate using pushState:
    loadedPromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      PRODUCT_TEST_URL
    );
    info("Navigate to the first product using pushState.");
    await SpecialPowers.spawn(browser, [PRODUCT_TEST_URL], urlToUse => {
      content.history.pushState({}, null, urlToUse);
    });
    info("Waiting to load first product again.");
    await loadedPromise;
    info("Waiting for the sidebar to have updated.");
    await promiseSidebarUpdated(sidebar, PRODUCT_TEST_URL);
    Assert.ok(sidebar, "Sidebar should exist");
    Assert.ok(
      BrowserTestUtils.is_visible(sidebar),
      "Sidebar should be visible."
    );

    info("Waiting to verify the first product a second time.");
    await verifyProductInfo(sidebar, {
      productURL: PRODUCT_TEST_URL,
      adjustedRating: "4.1",
      letterGrade: "B",
    });
  });
});
