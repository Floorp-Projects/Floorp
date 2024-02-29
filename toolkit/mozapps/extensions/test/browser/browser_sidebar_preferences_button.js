/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testNoOtherTabsPresent() {
  let addonsWin = await loadInitialView("extension");
  let preferencesButton =
    addonsWin.document.querySelector("#preferencesButton");

  let preferencesPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    "about:preferences"
  );

  preferencesButton.click();

  let preferencesTab = await preferencesPromise;

  is(
    gBrowser.currentURI.spec,
    "about:preferences",
    "about:preferences should open if neither it nor about:settings are present"
  );

  gBrowser.removeTab(preferencesTab);

  await closeView(addonsWin);
});

async function ensurePreferencesButtonFocusesTab(expectedUri) {
  let addonsWin = await loadInitialView("extension");
  let preferencesButton =
    addonsWin.document.querySelector("#preferencesButton");

  let tabCountBeforeClick = gBrowser.tabCount;
  preferencesButton.click();
  let tabCountAfterClick = gBrowser.tabCount;

  is(
    tabCountAfterClick,
    tabCountBeforeClick,
    "preferences button should not open new tabs"
  );
  is(
    gBrowser.currentURI.spec,
    expectedUri,
    "the correct tab should be focused"
  );

  addonsWin.focus();
  await closeView(addonsWin);
}

add_task(async function testAboutPreferencesPresent() {
  await BrowserTestUtils.withNewTab("about:preferences", async () => {
    await ensurePreferencesButtonFocusesTab("about:preferences");
  });
});

add_task(async function testAboutSettingsPresent() {
  await BrowserTestUtils.withNewTab("about:settings", async () => {
    await ensurePreferencesButtonFocusesTab("about:settings");
  });
});

add_task(async function testAboutSettingsAndPreferencesPresent() {
  await BrowserTestUtils.withNewTab("about:settings", async () => {
    await BrowserTestUtils.withNewTab("about:preferences", async () => {
      await ensurePreferencesButtonFocusesTab("about:settings");
    });
  });
});
