var gListener = null;
const kURL =
  "data:text/html;charset=utf-8,Caret browsing is fun.<input id='in'>";

const kPrefShortcutEnabled = "accessibility.browsewithcaret_shortcut.enabled";
const kPrefWarnOnEnable = "accessibility.warn_on_browsewithcaret";
const kPrefCaretBrowsingOn = "accessibility.browsewithcaret";

var oldPrefs = {};
for (let pref of [
  kPrefShortcutEnabled,
  kPrefWarnOnEnable,
  kPrefCaretBrowsingOn,
]) {
  oldPrefs[pref] = Services.prefs.getBoolPref(pref);
}

Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

registerCleanupFunction(function() {
  for (let pref of [
    kPrefShortcutEnabled,
    kPrefWarnOnEnable,
    kPrefCaretBrowsingOn,
  ]) {
    Services.prefs.setBoolPref(pref, oldPrefs[pref]);
  }
});

// NB: not using BrowserTestUtils.domWindowOpened here because there's no way to
// undo waiting for a window open. If we don't want the window to be opened, and
// wait for it to verify that it indeed does not open, we need to be able to
// then "stop" waiting so that when we next *do* want it to open, our "old"
// listener doesn't fire and do things we don't want (like close the window...).
let gCaretPromptOpeningObserver;
function promiseCaretPromptOpened() {
  return new Promise(resolve => {
    function observer(subject, topic, data) {
      if (topic == "domwindowopened") {
        Services.ww.unregisterNotification(observer);
        let win = subject;
        BrowserTestUtils.waitForEvent(
          win,
          "load",
          false,
          e => e.target.location.href != "about:blank"
        ).then(() => resolve(win));
        gCaretPromptOpeningObserver = null;
      }
    }
    Services.ww.registerNotification(observer);
    gCaretPromptOpeningObserver = observer;
  });
}

function hitF7(async = true) {
  let f7 = () => EventUtils.sendKey("F7");
  // Need to not stop execution inside this task:
  if (async) {
    executeSoon(f7);
  } else {
    f7();
  }
}

function syncToggleCaretNoDialog(expected) {
  let openedDialog = false;
  promiseCaretPromptOpened().then(function(win) {
    openedDialog = true;
    win.close(); // This will eventually return focus here and allow the test to continue...
  });
  // Cause the dialog to appear sync, if it still does.
  hitF7(false);

  let expectedStr = expected ? "on." : "off.";
  ok(
    !openedDialog,
    "Shouldn't open a dialog to turn caret browsing " + expectedStr
  );
  // Need to clean up if the dialog wasn't opened, so the observer doesn't get
  // re-triggered later on causing "issues".
  if (!openedDialog) {
    Services.ww.unregisterNotification(gCaretPromptOpeningObserver);
    gCaretPromptOpeningObserver = null;
  }
  let prefVal = Services.prefs.getBoolPref(kPrefCaretBrowsingOn);
  is(prefVal, expected, "Caret browsing should now be " + expectedStr);
}

function waitForFocusOnInput(browser) {
  return SpecialPowers.spawn(browser, [], async function() {
    let textEl = content.document.getElementById("in");
    return ContentTaskUtils.waitForCondition(() => {
      return content.document.activeElement == textEl;
    }, "Input should get focused.");
  });
}

function focusInput(browser) {
  return SpecialPowers.spawn(browser, [], async function() {
    let textEl = content.document.getElementById("in");
    textEl.focus();
  });
}

add_task(async function checkTogglingCaretBrowsing() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kURL);
  await focusInput(tab.linkedBrowser);

  let promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  let prompt = await promiseGotKey;
  let doc = prompt.document;
  let dialog = doc.getElementById("commonDialog");
  is(dialog.defaultButton, "cancel", "No button should be the default");
  ok(
    !doc.getElementById("checkbox").checked,
    "Checkbox shouldn't be checked by default."
  );
  let promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");

  dialog.cancelDialog();
  await promiseDialogUnloaded;
  info("Dialog unloaded");
  await waitForFocusOnInput(tab.linkedBrowser);
  ok(
    !Services.prefs.getBoolPref(kPrefCaretBrowsingOn),
    "Caret browsing should still be off after cancelling the dialog."
  );

  promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  prompt = await promiseGotKey;

  doc = prompt.document;
  dialog = doc.getElementById("commonDialog");
  is(dialog.defaultButton, "cancel", "No button should be the default");
  ok(
    !doc.getElementById("checkbox").checked,
    "Checkbox shouldn't be checked by default."
  );
  promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");

  dialog.acceptDialog();
  await promiseDialogUnloaded;
  info("Dialog unloaded");
  await waitForFocusOnInput(tab.linkedBrowser);
  ok(
    Services.prefs.getBoolPref(kPrefCaretBrowsingOn),
    "Caret browsing should be on after accepting the dialog."
  );

  syncToggleCaretNoDialog(false);

  promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  prompt = await promiseGotKey;
  doc = prompt.document;
  dialog = doc.getElementById("commonDialog");

  is(dialog.defaultButton, "cancel", "No button should be the default");
  ok(
    !doc.getElementById("checkbox").checked,
    "Checkbox shouldn't be checked by default."
  );

  promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");
  dialog.cancelDialog();
  await promiseDialogUnloaded;
  info("Dialog unloaded");
  await waitForFocusOnInput(tab.linkedBrowser);

  ok(
    !Services.prefs.getBoolPref(kPrefCaretBrowsingOn),
    "Caret browsing should still be off after cancelling the dialog."
  );

  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function toggleCheckboxNoCaretBrowsing() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kURL);
  await focusInput(tab.linkedBrowser);

  let promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  let prompt = await promiseGotKey;
  let doc = prompt.document;
  let dialog = doc.getElementById("commonDialog");
  is(dialog.defaultButton, "cancel", "No button should be the default");
  let checkbox = doc.getElementById("checkbox");
  ok(!checkbox.checked, "Checkbox shouldn't be checked by default.");

  // Check the box:
  checkbox.click();

  let promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");

  // Say no:
  dialog.getButton("cancel").click();

  await promiseDialogUnloaded;
  info("Dialog unloaded");
  await waitForFocusOnInput(tab.linkedBrowser);
  ok(
    !Services.prefs.getBoolPref(kPrefCaretBrowsingOn),
    "Caret browsing should still be off."
  );
  ok(
    !Services.prefs.getBoolPref(kPrefShortcutEnabled),
    "Shortcut should now be disabled."
  );

  syncToggleCaretNoDialog(false);
  ok(
    !Services.prefs.getBoolPref(kPrefShortcutEnabled),
    "Shortcut should still be disabled."
  );

  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function toggleCheckboxWantCaretBrowsing() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kURL);
  await focusInput(tab.linkedBrowser);

  let promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  let prompt = await promiseGotKey;
  let doc = prompt.document;
  let dialog = doc.getElementById("commonDialog");
  is(dialog.defaultButton, "cancel", "No button should be the default");
  let checkbox = doc.getElementById("checkbox");
  ok(!checkbox.checked, "Checkbox shouldn't be checked by default.");

  // Check the box:
  checkbox.click();

  let promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");

  // Say yes:
  dialog.acceptDialog();
  await promiseDialogUnloaded;
  info("Dialog unloaded");
  await waitForFocusOnInput(tab.linkedBrowser);
  ok(
    Services.prefs.getBoolPref(kPrefCaretBrowsingOn),
    "Caret browsing should now be on."
  );
  ok(
    Services.prefs.getBoolPref(kPrefShortcutEnabled),
    "Shortcut should still be enabled."
  );
  ok(
    !Services.prefs.getBoolPref(kPrefWarnOnEnable),
    "Should no longer warn when enabling."
  );

  syncToggleCaretNoDialog(false);
  syncToggleCaretNoDialog(true);
  syncToggleCaretNoDialog(false);

  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

  BrowserTestUtils.removeTab(tab);
});
