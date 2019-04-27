/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PLUGIN_DESCRIPTION = "Flash plug-in for testing purposes.";

add_task(async function enableHtmlViews() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
});

function checkItems(items, checked) {
  for (let item of items) {
    let action = item.getAttribute("action");
    ok(!item.disabled, `${action} is enabled`);
    if (action == checked) {
      ok(item.checked, `${action} is checked`);
    } else {
      ok(!item.checked, `${action} isn't checked`);
    }
  }
}

add_task(async function testAskToActivate() {
  let plugins = await AddonManager.getAddonsByTypes(["plugin"]);
  let flash = plugins.find(
    plugin => plugin.description == TEST_PLUGIN_DESCRIPTION);
  let win = await loadInitialView("plugin");
  let doc = win.document;

  let card = doc.querySelector(`addon-card[addon-id="${flash.id}"]`);
  let panelItems = Array.from(card.querySelectorAll("panel-item"));
  let actions = panelItems.map(item => item.getAttribute("action"));
  Assert.deepEqual(
    actions, ["ask-to-activate", "always-activate", "never-activate", "expand"],
    "The panel items are for a plugin");

  checkItems(panelItems, "ask-to-activate");

  is(flash.userDisabled, AddonManager.STATE_ASK_TO_ACTIVATE,
     "Flash is ask-to-activate");
  ok(flash.isActive, "Flash is active");

  // Switch the plugin to always activate.
  let updated = BrowserTestUtils.waitForEvent(card, "update");
  panelItems[1].click();
  await updated;
  checkItems(panelItems, "always-activate");
  ok(flash.userDisabled != AddonManager.STATE_ASK_TO_ACTIVATE,
     "Flash isn't ask-to-activate");
  ok(flash.isActive, "Flash is still active");

  // Switch to never activate.
  updated = BrowserTestUtils.waitForEvent(card, "update");
  panelItems[2].click();
  await updated;
  checkItems(panelItems, "never-activate");
  ok(flash.userDisabled, `Flash is not userDisabled... for some reason`);
  ok(!flash.isActive, "Flash isn't active");

  // Switch it back to ask to activate.
  updated = BrowserTestUtils.waitForEvent(card, "update");
  panelItems[0].click();
  await updated;
  checkItems(panelItems, "ask-to-activate");
  is(flash.userDisabled, AddonManager.STATE_ASK_TO_ACTIVATE,
     "Flash is ask-to-activate");
  ok(flash.isActive, "Flash is active");

  await closeView(win);
});
