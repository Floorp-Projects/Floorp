/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function shoppingSidebarLoaded(sidebar) {
  let innerBrowser = sidebar.querySelector("browser");
  await SpecialPowers.spawn(innerBrowser, [], function () {
    let doc = content.document;
    if (
      doc.readyState == "complete" &&
      doc.location.href == "about:shoppingsidebar"
    ) {
      return null;
    }
    return ContentTaskUtils.waitForEvent(doc, "load", true);
  });
  return innerBrowser;
}

function promiseSidebarUpdated(sidebar, expectedProduct) {
  let browser = sidebar.querySelector("browser");
  return SpecialPowers.spawn(browser, [expectedProduct], prod => {
    function isProductCurrent() {
      let actor = content.windowGlobalChild.getExistingActor("ShoppingSidebar");
      return actor?.getProductURI()?.spec == prod;
    }
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
