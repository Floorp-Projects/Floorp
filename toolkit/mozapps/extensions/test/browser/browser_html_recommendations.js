/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint max-len: ["error", 80] */

"use strict";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
});

async function checkRecommendedBadge(id, hidden) {
  function checkBadge() {
    let card = win.document.querySelector(`addon-card[addon-id="${id}"]`);
    let badge = card.querySelector(".addon-badge-recommended");
    is(badge.hidden, hidden, `badge is ${hidden ? "hidden" : "shown"}`);
    return card;
  }

  let win = await loadInitialView("extension");

  // Check list view.
  let card = checkBadge();

  // Load detail view.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // Check detail view.
  checkBadge();

  await closeView(win);
}

add_task(async function testNotRecommended() {
  let id = "not-recommended@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {applications: {gecko: {id}}},
    useAddonManager: "temporary",
  });
  await extension.startup();

  await checkRecommendedBadge(id, true);

  await extension.unload();
});

add_task(async function testRecommended() {
  let id = "recommended@mochi.test";
  let provider = new MockProvider();
  provider.createAddons([{
    id,
    isRecommended: true,
    name: "Recommended",
    type: "extension",
  }]);

  await checkRecommendedBadge(id, false);
});
