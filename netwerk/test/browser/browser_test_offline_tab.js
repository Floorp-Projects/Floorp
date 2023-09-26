/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_set_tab_offline() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    // Set the tab to offline
    gBrowser.selectedBrowser.browsingContext.forceOffline = true;

    await SpecialPowers.spawn(browser, [], async () => {
      try {
        await content.fetch("https://example.com/empty.html");
        ok(false, "Should not load since tab is offline");
      } catch (err) {
        is(err.name, "TypeError", "Should fail since tab is offline");
      }
    });
  });
});

add_task(async function test_set_tab_online() {
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    // Set the tab to online
    gBrowser.selectedBrowser.browsingContext.forceOffline = false;

    await SpecialPowers.spawn(browser, [], async () => {
      try {
        await content.fetch("https://example.com/empty.html");
        ok(true, "Should load since tab is online");
      } catch (err) {
        ok(false, "Should not fail since tab is online");
      }
    });
  });
});
