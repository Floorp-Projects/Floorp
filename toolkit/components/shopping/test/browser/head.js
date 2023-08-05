/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
    ).then(e => true);
  });
}
