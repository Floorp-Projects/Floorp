/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 566194 - safe mode / security & compatibility check status are not exposed in new addon manager UI

async function loadDetail(win, id) {
  let loaded = waitForViewLoad(win);
  // Check the detail view.
  let card = win.document.querySelector(`addon-card[addon-id="${id}"]`);
  EventUtils.synthesizeMouseAtCenter(card, {}, win);
  await loaded;
}

function checkMessageShown(win, type, hasButton) {
  let stack = win.document.querySelector("global-warnings");
  is(stack.childElementCount, 1, "There is one message");
  let messageBar = stack.firstElementChild;
  ok(messageBar, "There is a message bar");
  is(messageBar.localName, "message-bar", "The message bar is a message-bar");
  is_element_visible(messageBar, "Message bar is visible");
  is(messageBar.getAttribute("warning-type"), type);
  if (hasButton) {
    let button = messageBar.querySelector("button");
    is_element_visible(button, "Button is visible");
    is(button.getAttribute("action"), type, "Button action is set");
  }
}

function checkNoMessages(win) {
  let stack = win.document.querySelector("global-warnings");
  if (stack.childElementCount) {
    // The safe mode message is hidden in CSS on the plugin list.
    for (let child of stack.children) {
      is_element_hidden(child, "The message is hidden");
    }
  } else {
    is(stack.childElementCount, 0, "There are no message bars");
  }
}

function clickMessageAction(win) {
  let stack = win.document.querySelector("global-warnings");
  let button = stack.firstElementChild.querySelector("button");
  EventUtils.synthesizeMouseAtCenter(button, {}, win);
}

add_task(async function checkCompatibility() {
  info("Testing compatibility checking warning");

  info("Setting checkCompatibility to false");
  AddonManager.checkCompatibility = false;

  let id = "test@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { applications: { gecko: { id } } },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");

  // Check the extension list view.
  checkMessageShown(win, "check-compatibility", true);

  // Check the detail view.
  await loadDetail(win, id);
  checkMessageShown(win, "check-compatibility", true);

  // Check other views.
  let views = ["plugin", "theme"];
  for (let view of views) {
    await switchView(win, view);
    checkMessageShown(win, "check-compatibility", true);
  }

  // Check the button works.
  info("Clicking 'Enable' button");
  clickMessageAction(win);
  is(
    AddonManager.checkCompatibility,
    true,
    "Check Compatibility pref should be cleared"
  );
  checkNoMessages(win);

  await closeView(win);
  await extension.unload();
});

add_task(async function checkSecurity() {
  info("Testing update security checking warning");

  var pref = "extensions.checkUpdateSecurity";
  info("Setting " + pref + " pref to false");
  await SpecialPowers.pushPrefEnv({
    set: [[pref, false]],
  });

  let id = "test-security@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { applications: { gecko: { id } } },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");

  // Check extension list view.
  checkMessageShown(win, "update-security", true);

  // Check detail view.
  await loadDetail(win, id);
  checkMessageShown(win, "update-security", true);

  // Check other views.
  let views = ["plugin", "theme"];
  for (let view of views) {
    await switchView(win, view);
    checkMessageShown(win, "update-security", true);
  }

  // Check the button works.
  info("Clicking 'Enable' button");
  clickMessageAction(win);
  is(
    Services.prefs.prefHasUserValue(pref),
    false,
    "Check Update Security pref should be cleared"
  );
  checkNoMessages(win);

  await closeView(win);
  await extension.unload();
});

add_task(async function checkSafeMode() {
  info("Testing safe mode warning");

  let id = "test-safemode@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { applications: { gecko: { id } } },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");

  // Check extension list view hidden.
  checkNoMessages(win);

  let globalWarnings = win.document.querySelector("global-warnings");
  globalWarnings.inSafeMode = true;
  globalWarnings.refresh();

  // Check detail view.
  await loadDetail(win, id);
  checkMessageShown(win, "safe-mode");

  // Check other views.
  await switchView(win, "theme");
  checkMessageShown(win, "safe-mode");
  await switchView(win, "plugin");
  checkNoMessages(win);

  await closeView(win);
  await extension.unload();
});
