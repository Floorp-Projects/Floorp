"use strict";

add_task(async function test_summary() {
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url: "chrome://payments/content/paymentRequest.xhtml",
  }, async browser => {
    // eslint-disable-next-line mozilla/no-cpows-in-tests
    ok(browser.contentDocument.getElementById("cancel"), "Cancel button exists");
  });
});
