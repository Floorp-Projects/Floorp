/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function testOpenMenu(btn, method) {
  let shown = BrowserTestUtils.waitForEvent(btn.ownerGlobal, "shown", true);
  await method();
  await shown;
  is(btn.getAttribute("aria-expanded"), "true", "expanded when open");
}

async function testCloseMenu(btn, method) {
  let hidden = BrowserTestUtils.waitForEvent(btn.ownerGlobal, "hidden", true);
  await method();
  await hidden;
  is(btn.getAttribute("aria-expanded"), "false", "not expanded when closed");
}

async function testButton(btn) {
  let win = btn.ownerGlobal;

  is(btn.getAttribute("aria-haspopup"), "menu", "it has a menu");
  is(btn.getAttribute("aria-expanded"), "false", "not expanded");

  info("Test open/close with mouse");
  await testOpenMenu(btn, () => {
    EventUtils.synthesizeMouseAtCenter(btn, {}, win);
  });
  await testCloseMenu(btn, () => {
    let spacer = win.document.querySelector(".main-heading .spacer");
    // We intentionally turn off this a11y check, because the following click
    // is purposefully targeting a non-interactive element to dismiss the
    // opened menu with a mouse which can be done by assistive technology and
    // keyboard by pressing `Esc` key, this rule check shall be ignored by
    // a11y_checks suite.
    AccessibilityUtils.setEnv({ mustHaveAccessibleRule: false });
    EventUtils.synthesizeMouseAtCenter(spacer, {}, win);
    AccessibilityUtils.resetEnv();
  });

  info("Test open/close with keyboard");
  await testOpenMenu(btn, async () => {
    btn.focus();
    EventUtils.synthesizeKey(" ", {}, win);
  });
  await testCloseMenu(btn, () => {
    EventUtils.synthesizeKey("Escape", {}, win);
  });
}

add_task(async function testPageOptionsMenuButton() {
  let win = await loadInitialView("extension");

  await testButton(
    win.document.querySelector(".page-options-menu .more-options-button")
  );

  await closeView(win);
});

add_task(async function testCardMoreOptionsButton() {
  let id = "more-options-button@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let card = getAddonCard(win, id);

  info("Check list page");
  await testButton(card.querySelector(".more-options-button"));

  let viewLoaded = waitForViewLoad(win);

  EventUtils.synthesizeMouseAtCenter(
    card.querySelector(".addon-name-link"),
    {},
    win
  );
  await viewLoaded;

  info("Check detail page");
  card = getAddonCard(win, id);
  await testButton(card.querySelector(".more-options-button"));

  await closeView(win);
  await extension.unload();
});
