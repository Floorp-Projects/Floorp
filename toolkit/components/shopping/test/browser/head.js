/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PRODUCT_TEST_URL = "https://example.com/Some-Product/dp/ABCDEFG123";
const OTHER_PRODUCT_TEST_URL =
  "https://example.com/Another-Product/dp/HIJKLMN456";
const BAD_PRODUCT_TEST_URL = "https://example.com/Bad-Product/dp/0000000000";
const NEEDS_ANALYSIS_TEST_URL = "https://example.com/Bad-Product/dp/OPQRSTU789";

async function promiseSidebarUpdated(sidebar, expectedProduct) {
  let browser = sidebar.querySelector("browser");
  if (
    !browser.currentURI?.equals(Services.io.newURI("about:shoppingsidebar"))
  ) {
    await BrowserTestUtils.browserLoaded(
      browser,
      false,
      "about:shoppingsidebar"
    );
  }
  return SpecialPowers.spawn(browser, [expectedProduct], prod => {
    function isProductCurrent() {
      let actor = content.windowGlobalChild.getExistingActor("ShoppingSidebar");
      return actor?.getProductURI()?.spec == prod;
    }
    if (
      isProductCurrent() &&
      !!content.document.querySelector("shopping-container").wrappedJSObject
        .data
    ) {
      info("Product already loaded.");
      return true;
    }
    info(
      "Waiting for product to be updated. Document: " +
        content.document.location.href
    );
    return ContentTaskUtils.waitForEvent(
      content.document,
      "Update",
      true,
      e => {
        info("Sidebar updated for product: " + JSON.stringify(e.detail));
        return !!e.detail.data && isProductCurrent();
      },
      true
    ).then(() => true);
  });
}

async function promiseSidebarAdsUpdated(sidebar, expectedProduct) {
  await promiseSidebarUpdated(sidebar, expectedProduct);
  let browser = sidebar.querySelector("browser");
  return SpecialPowers.spawn(browser, [], () => {
    let container =
      content.document.querySelector("shopping-container").wrappedJSObject;
    if (container.recommendationData) {
      return true;
    }
    return ContentTaskUtils.waitForEvent(
      content.document,
      "UpdateRecommendations",
      true,
      null,
      true
    ).then(() => true);
  });
}

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
