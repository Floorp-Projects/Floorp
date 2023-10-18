/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var doc, tab;

add_setup(async function () {
  info("Starting about:webauthn");
  tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:webauthn",
    waitForLoad: true,
  });

  doc = tab.linkedBrowser.contentDocument;
});

registerCleanupFunction(async function () {
  // Close tab.
  await BrowserTestUtils.removeTab(tab);
});

add_task(async function verify_page_no_token() {
  let info_text = doc.getElementById("info-text-div");
  is(info_text.hidden, false, "info-text-div should be visible");
  let categories = doc.getElementById("categories");
  is(categories.hidden, true, "categories-sidebar should be invisible");
});

add_task(async function verify_no_auth_info() {
  let field = doc.getElementById("info-text-field");
  let promise = BrowserTestUtils.waitForMutationCondition(
    field,
    { attributes: true, attributeFilter: ["data-l10n-id"] },
    () =>
      field.getAttribute("data-l10n-id") ===
      "about-webauthn-text-non-ctap2-device"
  );
  Services.obs.notifyObservers(
    null,
    "about-webauthn-prompt",
    JSON.stringify({ type: "selected-device", auth_info: null })
  );
  await promise;

  let info_text = doc.getElementById("info-text-div");
  is(info_text.hidden, false);
});
