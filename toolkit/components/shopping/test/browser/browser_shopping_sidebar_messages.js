/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NOT_ENOUGH_REVIEWS_TEST_URL =
  "https://example.com/Bad-Product/dp/N0T3NOUGHR";
const NOT_SUPPORTED_TEST_URL = "https://example.com/Bad-Product/dp/PAG3N0TSUP";
const UNPROCESSABLE_TEST_URL = "https://example.com/Bad-Product/dp/UNPR0C3SSA";

add_task(async function test_sidebar_error() {
  // Disable OHTTP for now to get this landed; we'll re-enable with proper
  // mocking in the near future.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  await BrowserTestUtils.withNewTab(BAD_PRODUCT_TEST_URL, async browser => {
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");

    Assert.ok(sidebar, "Sidebar should exist");

    Assert.ok(
      BrowserTestUtils.isVisible(sidebar),
      "Sidebar should be visible."
    );
    info("Waiting for sidebar to update.");
    await promiseSidebarUpdated(sidebar, BAD_PRODUCT_TEST_URL);

    info("Verifying a generic error is shown.");
    await SpecialPowers.spawn(
      sidebar.querySelector("browser"),
      [],
      async prodInfo => {
        let doc = content.document;
        let shoppingContainer =
          doc.querySelector("shopping-container").wrappedJSObject;

        ok(
          shoppingContainer.shoppingMessageBarEl,
          "Got shopping-message-bar element"
        );
        is(
          shoppingContainer.shoppingMessageBarEl.getAttribute("type"),
          "generic-error",
          "generic-error type should be correct"
        );
      }
    );
  });
});

add_task(async function test_sidebar_analysis_status_page_not_supported() {
  // Disable OHTTP for now to get this landed; we'll re-enable with proper
  // mocking in the near future.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });

  // Product not supported status
  await BrowserTestUtils.withNewTab(NOT_SUPPORTED_TEST_URL, async browser => {
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");

    Assert.ok(sidebar, "Sidebar should exist");

    Assert.ok(
      BrowserTestUtils.isVisible(sidebar),
      "Sidebar should be visible."
    );
    info("Waiting for sidebar to update.");
    await promiseSidebarUpdated(sidebar, NOT_SUPPORTED_TEST_URL);

    info("Verifying a generic error is shown.");
    await SpecialPowers.spawn(
      sidebar.querySelector("browser"),
      [],
      async prodInfo => {
        let doc = content.document;
        let shoppingContainer =
          doc.querySelector("shopping-container").wrappedJSObject;

        ok(
          shoppingContainer.shoppingMessageBarEl,
          "Got shopping-message-bar element"
        );
        is(
          shoppingContainer.shoppingMessageBarEl.getAttribute("type"),
          "page-not-supported",
          "message type should be correct"
        );
      }
    );
  });
});

add_task(async function test_sidebar_analysis_status_unprocessable() {
  // Disable OHTTP for now to get this landed; we'll re-enable with proper
  // mocking in the near future.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });

  // Unprocessable status
  await BrowserTestUtils.withNewTab(UNPROCESSABLE_TEST_URL, async browser => {
    let sidebar = gBrowser.getPanel(browser).querySelector("shopping-sidebar");

    Assert.ok(sidebar, "Sidebar should exist");

    Assert.ok(
      BrowserTestUtils.isVisible(sidebar),
      "Sidebar should be visible."
    );
    info("Waiting for sidebar to update.");
    await promiseSidebarUpdated(sidebar, UNPROCESSABLE_TEST_URL);

    info("Verifying a generic error is shown.");
    await SpecialPowers.spawn(
      sidebar.querySelector("browser"),
      [],
      async prodInfo => {
        let doc = content.document;
        let shoppingContainer =
          doc.querySelector("shopping-container").wrappedJSObject;

        ok(
          shoppingContainer.shoppingMessageBarEl,
          "Got shopping-message-bar element"
        );
        is(
          shoppingContainer.shoppingMessageBarEl.getAttribute("type"),
          "generic-error",
          "message type should be correct"
        );
      }
    );
  });
});

add_task(async function test_sidebar_analysis_status_not_enough_reviews() {
  // Disable OHTTP for now to get this landed; we'll re-enable with proper
  // mocking in the near future.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["toolkit.shopping.ohttpRelayURL", ""],
      ["toolkit.shopping.ohttpConfigURL", ""],
    ],
  });
  // Not enough reviews status
  await BrowserTestUtils.withNewTab(
    NOT_ENOUGH_REVIEWS_TEST_URL,
    async browser => {
      let sidebar = gBrowser
        .getPanel(browser)
        .querySelector("shopping-sidebar");

      Assert.ok(sidebar, "Sidebar should exist");

      Assert.ok(
        BrowserTestUtils.isVisible(sidebar),
        "Sidebar should be visible."
      );
      info("Waiting for sidebar to update.");
      await promiseSidebarUpdated(sidebar, NOT_ENOUGH_REVIEWS_TEST_URL);

      info("Verifying a generic error is shown.");
      await SpecialPowers.spawn(
        sidebar.querySelector("browser"),
        [],
        async prodInfo => {
          let doc = content.document;
          let shoppingContainer =
            doc.querySelector("shopping-container").wrappedJSObject;

          ok(
            shoppingContainer.shoppingMessageBarEl,
            "Got shopping-message-bar element"
          );
          is(
            shoppingContainer.shoppingMessageBarEl.getAttribute("type"),
            "not-enough-reviews",
            "message type should be correct"
          );
        }
      );
    }
  );
});
