/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint max-len: ["error", 80] */

"use strict";

add_task(async function enableHtmlViews() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.htmlaboutaddons.enabled", true],
      ["extensions.htmlaboutaddons.inline-options.enabled", true],
    ],
  });
});

async function testOptionsInTab({id, options_ui_options}) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Prefs extension",
      applications: {gecko: {id}},
      options_ui: {
        page: "options.html",
        ...options_ui_options,
      },
    },
    background() {
      browser.test.sendMessage(
        "options-url", browser.runtime.getURL("options.html"));
    },
    files: {
      "options.html": `<script src="options.js"></script>`,
      "options.js": () => {
        browser.test.sendMessage("options-loaded");
      },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();
  let optionsUrl = await extension.awaitMessage("options-url");

  let win = await loadInitialView("extension");
  let doc = win.document;
  let aboutAddonsTab = gBrowser.selectedTab;

  let card = doc.querySelector(`addon-card[addon-id="${id}"]`);

  let prefsBtn = card.querySelector('panel-item[action="preferences"]');
  ok(!prefsBtn.hidden, "The button is not hidden");

  info("Open the preferences page from list");
  let tabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, optionsUrl);
  prefsBtn.click();
  BrowserTestUtils.removeTab(await tabLoaded);
  await extension.awaitMessage("options-loaded");

  info("Load details page");
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // Find the expanded card.
  card = doc.querySelector(`addon-card[addon-id="${id}"]`);

  info("Check that the button is still visible");
  prefsBtn = card.querySelector('panel-item[action="preferences"]');
  ok(!prefsBtn.hidden, "The button is not hidden");

  info("Open the preferences page from details");
  tabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, optionsUrl);
  prefsBtn.click();
  let prefsTab = await tabLoaded;
  await extension.awaitMessage("options-loaded");

  info("Switch back to about:addons and open prefs again");
  await BrowserTestUtils.switchTab(gBrowser, aboutAddonsTab);
  let tabSwitched = BrowserTestUtils.waitForEvent(gBrowser, "TabSwitchDone");
  prefsBtn.click();
  await tabSwitched;
  is(gBrowser.selectedTab, prefsTab, "The prefs tab was selected");

  BrowserTestUtils.removeTab(prefsTab);

  await closeView(win);
  await extension.unload();
}

add_task(async function testPreferencesLink() {
  let id = "prefs@mochi.test";
  await testOptionsInTab({id, options_ui_options: {open_in_tab: true}});
});

add_task(async function testPreferencesInlineDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.inline-options.enabled", false]],
  });

  let id = "inline-disabled@mochi.test";
  await testOptionsInTab({id, options_ui_options: {}});

  await SpecialPowers.popPrefEnv();
});

add_task(async function testNoPreferences() {
  let id = "no-prefs@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "No Prefs extension",
      applications: {gecko: {id}},
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = doc.querySelector(`addon-card[addon-id="${id}"]`);

  info("Check button on list");
  let prefsBtn = card.querySelector('panel-item[action="preferences"]');
  ok(prefsBtn.hidden, "The button is hidden");

  info("Load details page");
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  // Find the expanded card.
  card = doc.querySelector(`addon-card[addon-id="${id}"]`);

  info("Check that the button is still hidden on detail");
  prefsBtn = card.querySelector('panel-item[action="preferences"]');
  ok(prefsBtn.hidden, "The button is hidden");

  await closeView(win);
  await extension.unload();
});
