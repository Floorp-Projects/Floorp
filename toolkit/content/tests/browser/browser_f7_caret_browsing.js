var gListener = null;
const kURL = "data:text/html;charset=utf-8,Caret browsing is fun.<input id='in'>";

const kPrefShortcutEnabled = "accessibility.browsewithcaret_shortcut.enabled";
const kPrefWarnOnEnable    = "accessibility.warn_on_browsewithcaret";
const kPrefCaretBrowsingOn = "accessibility.browsewithcaret";

var oldPrefs = {};
for (let pref of [kPrefShortcutEnabled, kPrefWarnOnEnable, kPrefCaretBrowsingOn]) {
  oldPrefs[pref] = Services.prefs.getBoolPref(pref);
}

Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

registerCleanupFunction(function() {
  for (let pref of [kPrefShortcutEnabled, kPrefWarnOnEnable, kPrefCaretBrowsingOn]) {
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
        let win = subject.QueryInterface(Ci.nsIDOMWindow);
        BrowserTestUtils.waitForEvent(win, "load", false, e => e.target.location.href != "about:blank").then(() => resolve(win));
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
  ok(!openedDialog, "Shouldn't open a dialog to turn caret browsing " + expectedStr);
  // Need to clean up if the dialog wasn't opened, so the observer doesn't get
  // re-triggered later on causing "issues".
  if (!openedDialog) {
    Services.ww.unregisterNotification(gCaretPromptOpeningObserver);
    gCaretPromptOpeningObserver = null;
  }
  let prefVal = Services.prefs.getBoolPref(kPrefCaretBrowsingOn);
  is(prefVal, expected, "Caret browsing should now be " + expectedStr);
}

function waitForFocusOnInput(browser)
{
  return ContentTask.spawn(browser, null, function* () {
    let textEl = content.document.getElementById("in");
    return ContentTaskUtils.waitForCondition(() => {
      return content.document.activeElement == textEl;
    }, "Input should get focused.");
  });
}

function focusInput(browser)
{
  return ContentTask.spawn(browser, null, function* () {
    let textEl = content.document.getElementById("in");
    textEl.focus();
  });
}

add_task(function* checkTogglingCaretBrowsing() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, kURL);
  yield focusInput(tab.linkedBrowser);

  let promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  let prompt = yield promiseGotKey;
  let doc = prompt.document;
  is(doc.documentElement.defaultButton, "cancel", "No button should be the default");
  ok(!doc.getElementById("checkbox").checked, "Checkbox shouldn't be checked by default.");
  let promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");

  doc.documentElement.cancelDialog();
  yield promiseDialogUnloaded;
  info("Dialog unloaded");
  yield waitForFocusOnInput(tab.linkedBrowser);
  ok(!Services.prefs.getBoolPref(kPrefCaretBrowsingOn), "Caret browsing should still be off after cancelling the dialog.");

  promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  prompt = yield promiseGotKey;

  doc = prompt.document;
  is(doc.documentElement.defaultButton, "cancel", "No button should be the default");
  ok(!doc.getElementById("checkbox").checked, "Checkbox shouldn't be checked by default.");
  promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");

  doc.documentElement.acceptDialog();
  yield promiseDialogUnloaded;
  info("Dialog unloaded");
  yield waitForFocusOnInput(tab.linkedBrowser);
  ok(Services.prefs.getBoolPref(kPrefCaretBrowsingOn), "Caret browsing should be on after accepting the dialog.");

  syncToggleCaretNoDialog(false);

  promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  prompt = yield promiseGotKey;
  doc = prompt.document;

  is(doc.documentElement.defaultButton, "cancel", "No button should be the default");
  ok(!doc.getElementById("checkbox").checked, "Checkbox shouldn't be checked by default.");

  promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");
  doc.documentElement.cancelDialog();
  yield promiseDialogUnloaded;
  info("Dialog unloaded");
  yield waitForFocusOnInput(tab.linkedBrowser);

  ok(!Services.prefs.getBoolPref(kPrefCaretBrowsingOn), "Caret browsing should still be off after cancelling the dialog.");

  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* toggleCheckboxNoCaretBrowsing() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, kURL);
  yield focusInput(tab.linkedBrowser);

  let promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  let prompt = yield promiseGotKey;
  let doc = prompt.document;
  is(doc.documentElement.defaultButton, "cancel", "No button should be the default");
  let checkbox = doc.getElementById("checkbox");
  ok(!checkbox.checked, "Checkbox shouldn't be checked by default.");

  // Check the box:
  checkbox.click();

  let promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");

  // Say no:
  doc.documentElement.getButton("cancel").click();

  yield promiseDialogUnloaded;
  info("Dialog unloaded");
  yield waitForFocusOnInput(tab.linkedBrowser);
  ok(!Services.prefs.getBoolPref(kPrefCaretBrowsingOn), "Caret browsing should still be off.");
  ok(!Services.prefs.getBoolPref(kPrefShortcutEnabled), "Shortcut should now be disabled.");

  syncToggleCaretNoDialog(false);
  ok(!Services.prefs.getBoolPref(kPrefShortcutEnabled), "Shortcut should still be disabled.");

  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

  yield BrowserTestUtils.removeTab(tab);
});


add_task(function* toggleCheckboxWantCaretBrowsing() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, kURL);
  yield focusInput(tab.linkedBrowser);

  let promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  let prompt = yield promiseGotKey;
  let doc = prompt.document;
  is(doc.documentElement.defaultButton, "cancel", "No button should be the default");
  let checkbox = doc.getElementById("checkbox");
  ok(!checkbox.checked, "Checkbox shouldn't be checked by default.");

  // Check the box:
  checkbox.click();

  let promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");

  // Say yes:
  doc.documentElement.acceptDialog();
  yield promiseDialogUnloaded;
  info("Dialog unloaded");
  yield waitForFocusOnInput(tab.linkedBrowser);
  ok(Services.prefs.getBoolPref(kPrefCaretBrowsingOn), "Caret browsing should now be on.");
  ok(Services.prefs.getBoolPref(kPrefShortcutEnabled), "Shortcut should still be enabled.");
  ok(!Services.prefs.getBoolPref(kPrefWarnOnEnable), "Should no longer warn when enabling.");

  syncToggleCaretNoDialog(false);
  syncToggleCaretNoDialog(true);
  syncToggleCaretNoDialog(false);

  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

  yield BrowserTestUtils.removeTab(tab);
});




