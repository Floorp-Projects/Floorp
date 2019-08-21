/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint max-len: ["error", 80] */

"use strict";

const SUPPORT_URL = Services.urlFormatter.formatURL(
  Services.prefs.getStringPref("app.support.baseURL")
);
const SUMO_URL = SUPPORT_URL + "recommended-extensions-program";

async function checkRecommendedBadge(id, hidden) {
  async function checkBadge() {
    let card = win.document.querySelector(`addon-card[addon-id="${id}"]`);
    let badge = card.querySelector(".addon-badge-recommended");
    is(badge.hidden, hidden, `badge is ${hidden ? "hidden" : "shown"}`);
    if (!hidden) {
      info("Verify the badge links to the support page");
      let tabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, SUMO_URL);
      EventUtils.synthesizeMouseAtCenter(badge, {}, win);
      BrowserTestUtils.removeTab(await tabLoaded);
    }
    return card;
  }

  let win = await loadInitialView("extension");

  // Check list view.
  let card = await checkBadge();

  // Load detail view.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // Check detail view.
  await checkBadge();

  await closeView(win);
}

add_task(async function testNotRecommended() {
  let id = "not-recommended@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { applications: { gecko: { id } } },
    useAddonManager: "temporary",
  });
  await extension.startup();

  await checkRecommendedBadge(id, true);

  await extension.unload();
});

add_task(async function testRecommended() {
  let id = "recommended@mochi.test";
  let provider = new MockProvider();
  provider.createAddons([
    {
      id,
      isRecommended: true,
      name: "Recommended",
      type: "extension",
    },
  ]);

  await checkRecommendedBadge(id, false);
});
