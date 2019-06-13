/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 566194 - safe mode / security & compatibility check status are not exposed in new addon manager UI

async function loadDetail(aWindow, id) {
  let loaded = wait_for_view_load(aWindow, undefined, true);
  // Check the detail view.
  if (aWindow.useHtmlViews) {
    let browser = await aWindow.getHtmlBrowser();
    let card = browser.contentDocument.querySelector(
      `addon-card[addon-id="${id}"]`);
    EventUtils.synthesizeMouseAtCenter(card, {}, browser.contentWindow);
  } else {
    let card = aWindow.document.querySelector(`.addon.card[value="${id}"]`);
    EventUtils.synthesizeMouseAtCenter(card, {}, aWindow);
  }
  await loaded;
}

async function checkCompatibility(hboxSelector, buttonSelector) {
  info("Testing compatibility checking warning");

  info("Setting checkCompatibility to false");
  AddonManager.checkCompatibility = false;

  let id = "test@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {applications: {gecko: {id}}},
    useAddonManager: "temporary",
  });
  await extension.startup();

  let aWindow = await open_manager("addons://list/extension");
  let hbox = aWindow.document.querySelector(hboxSelector);
  let button = aWindow.document.querySelector(buttonSelector);

  function checkMessage(visible) {
    if (visible) {
      is_element_visible(hbox, "Check Compatibility warning hbox should be visible");
      is_element_visible(button, "Check Compatibility warning button should be visible");
    } else {
      is_element_hidden(hbox, "Check Compatibility warning hbox should be hidden");
      is_element_hidden(button, "Check Compatibility warning button should be hidden");
    }
  }

  // Check the extension list view.
  checkMessage(true);

  // Check the detail view.
  await loadDetail(aWindow, id);
  checkMessage(true);

  // Check other views.
  let views = ["plugin", "theme"];
  let categoryUtilities = new CategoryUtilities(aWindow);
  for (let view of views) {
    await categoryUtilities.openType(view);
    checkMessage(true);
  }

  // Check the button works.
  info("Clicking 'Enable' button");
  EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);
  is(AddonManager.checkCompatibility, true, "Check Compatibility pref should be cleared");
  checkMessage(false);

  await close_manager(aWindow);
  await extension.unload();
}

async function checkSecurity(hboxSelector, buttonSelector) {
  info("Testing update security checking warning");

  var pref = "extensions.checkUpdateSecurity";
  info("Setting " + pref + " pref to false");
  Services.prefs.setBoolPref(pref, false);

  let id = "test-security@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {applications: {gecko: {id}}},
    useAddonManager: "temporary",
  });
  await extension.startup();

  let aWindow = await open_manager("addons://list/extension");
  let hbox = aWindow.document.querySelector(hboxSelector);
  let button = aWindow.document.querySelector(buttonSelector);

  function checkMessage(visible) {
    if (visible) {
      is_element_visible(hbox, "Check Update Security warning hbox should be visible");
      is_element_visible(button, "Check Update Security warning button should be visible");
    } else {
      is_element_hidden(hbox, "Check Update Security warning hbox should be hidden");
      is_element_hidden(button, "Check Update Security warning button should be hidden");
    }
  }

  // Check extension list view.
  checkMessage(true);

  // Check detail view.
  await loadDetail(aWindow, id);
  checkMessage(true);

  // Check other views.
  let views = ["plugin", "theme"];
  let categoryUtilities = new CategoryUtilities(aWindow);
  for (let view of views) {
    await categoryUtilities.openType(view);
    checkMessage(true);
  }

  // Check the button works.
  info("Clicking 'Enable' button");
  EventUtils.synthesizeMouse(button, 2, 2, { }, aWindow);
  is(Services.prefs.prefHasUserValue(pref), false, "Check Update Security pref should be cleared");
  checkMessage(false);

  await close_manager(aWindow);
  await extension.unload();
}

async function checkSafeMode(hboxSelector) {
  info("Testing safe mode warning");

  let id = "test-safemode@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {applications: {gecko: {id}}},
    useAddonManager: "temporary",
  });
  await extension.startup();

  let aWindow = await open_manager("addons://list/extension");
  let hbox = aWindow.document.querySelector(hboxSelector);

  function checkMessage(visible) {
    if (visible) {
      is_element_visible(hbox, "Check safe mode warning hbox should be visible");
    } else {
      is_element_hidden(hbox, "Check safe mode warning hbox should be hidden");
    }
  }

  // Check extension list view hidden.
  checkMessage(false);

  // Mock safe mode by setting the page attribute.
  aWindow.document.getElementById("addons-page").setAttribute("warning", "safemode");

  // Check detail view.
  await loadDetail(aWindow, id);
  checkMessage(true);

  // Check other views.
  let categoryUtilities = new CategoryUtilities(aWindow);
  await categoryUtilities.openType("theme");
  checkMessage(true);
  await categoryUtilities.openType("plugin");
  checkMessage(false);

  await close_manager(aWindow);
  await extension.unload();
}

add_task(async function testCompatCheckXUL() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", false]],
  });
  await checkCompatibility(
    "#list-view hbox.global-warning-checkcompatibility",
    "#list-view button.global-warning-checkcompatibility");
  // No popPrefEnv because of bug 1557397.
});

add_task(async function testCompatCheckHTML() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
  await checkCompatibility(
    "#html-view .global-warning-checkcompatibility",
    "#html-view .global-warning-checkcompatibility button");
  // No popPrefEnv because of bug 1557397.
});

add_task(async function testSecurityCheckXUL() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", false]],
  });
  await checkSecurity(
    "#list-view hbox.global-warning-updatesecurity",
    "#list-view button.global-warning-updatesecurity");
  // No popPrefEnv because of bug 1557397.
});

add_task(async function testSecurityCheckHTML() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
  await checkSecurity(
    "#html-view .global-warning-updatesecurity",
    "#html-view .global-warning-updatesecurity button");
  // No popPrefEnv because of bug 1557397.
});

add_task(async function testSafeModeXUL() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", false]],
  });
  await checkSafeMode("#list-view hbox.global-warning-safemode");
  // No popPrefEnv because of bug 1557397.
});

add_task(async function testSafeModeHTML() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
  await checkSafeMode("#html-view .global-warning-safemode");
  // No popPrefEnv because of bug 1557397.
});
