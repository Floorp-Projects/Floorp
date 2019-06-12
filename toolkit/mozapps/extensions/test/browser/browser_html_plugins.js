/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint max-len: ["error", 80] */

"use strict";

const TEST_PLUGIN_DESCRIPTION = "Flash plug-in for testing purposes.";

add_task(async function enableHtmlViews() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });

  let gProvider = new MockProvider();
  gProvider.createAddons([{
    id: "no-ask-to-activate@mochi.test",
    name: "No ask to activate",
    getFullDescription(doc) {
      let a = doc.createElement("a");
      a.textContent = "A link";
      a.href = "http://example.com/no-ask-to-activate";
      return a;
    },
    type: "plugin",
  }]);

  Services.telemetry.clearEvents();
});

add_task(async function testAskToActivate() {
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

  let plugins = await AddonManager.getAddonsByTypes(["plugin"]);
  let flash = plugins.find(
    plugin => plugin.description == TEST_PLUGIN_DESCRIPTION);
  let addonId = flash.id;

  // Reset to default value.
  flash.userDisabled = AddonManager.STATE_ASK_TO_ACTIVATE;

  let win = await loadInitialView("plugin");
  let doc = win.document;

  let card = doc.querySelector(`addon-card[addon-id="${flash.id}"]`);
  let panelItems = card.querySelectorAll("panel-item:not([hidden])");
  let actions = Array.from(panelItems).map(item => item.getAttribute("action"));
  Assert.deepEqual(actions, [
    "ask-to-activate", "always-activate", "never-activate", "preferences",
    "expand",
  ], "The panel items are for a plugin");

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

  // Check the detail view, too.
  let loaded = waitForViewLoad(win);
  card.querySelector("[action=expand]").click();
  await loaded;

  // Set the state to always activate.
  card = doc.querySelector("addon-card");
  panelItems = card.querySelectorAll("panel-item");
  checkItems(panelItems, "ask-to-activate");
  updated = BrowserTestUtils.waitForEvent(card, "update");
  panelItems[1].click();
  await updated;
  checkItems(panelItems, "always-activate");

  await closeView(win);

  assertAboutAddonsTelemetryEvents([
    ["addonsManager", "view", "aboutAddons", "list", {type: "plugin"}],
    ["addonsManager", "action", "aboutAddons", null,
     {type: "plugin", addonId, view: "list", action: "enable"}],
    ["addonsManager", "action", "aboutAddons", null,
     {type: "plugin", addonId, view: "list", action: "disable"}],
    // Ask-to-activate doesn't trigger a telemetry event.
    ["addonsManager", "view", "aboutAddons", "detail",
     {type: "plugin", addonId}],
    ["addonsManager", "action", "aboutAddons", null,
     {type: "plugin", addonId, view: "detail", action: "enable"}],
  ]);
});

add_task(async function testNoAskToActivate() {
  function checkItems(menuItems) {
    for (let item of menuItems) {
      let action = item.getAttribute("action");
      if (action === "ask-to-activate") {
        ok(item.disabled, "ask-to-activate is disabled");
      } else {
        ok(!item.disabled, `${action} is enabled`);
      }
    }
  }

  let win = await loadInitialView("plugin");
  let doc = win.document;

  let id = "no-ask-to-activate@mochi.test";
  let card = doc.querySelector(`addon-card[addon-id="${id}"]`);
  ok(card, "The card was found");

  let menuItems = card.querySelectorAll("panel-item:not([hidden])");
  checkItems(menuItems);

  // There's no preferences option.
  let actions = Array.from(menuItems).map(item => item.getAttribute("action"));
  Assert.deepEqual(
    actions, ["ask-to-activate", "always-activate", "never-activate", "expand"],
   "The panel items are for a plugin");

  // Open the details page.
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = doc.querySelector("addon-card");
  is(card.addon.id, id, "The right plugin page was loaded");

  // Check that getFullDescription() will set the description.
  let description = card.querySelector(".addon-detail-description");
  is(description.childElementCount, 1, "The description has one element");
  let link = description.firstElementChild;
  is(link.localName, "a", "The element is a link");
  is(link.href, "http://example.com/no-ask-to-activate", "The href is set");
  is(link.textContent, "A link", "The text is set");

  menuItems = card.querySelectorAll("panel-item:not([hidden])");
  checkItems(menuItems);

  // There's no preferences option, and expand is now hidden.
  actions = Array.from(menuItems).map(item => item.getAttribute("action"));
  Assert.deepEqual(
    actions, ["ask-to-activate", "always-activate", "never-activate"],
   "The panel items are for a detail page plugin");

  await closeView(win);
});
