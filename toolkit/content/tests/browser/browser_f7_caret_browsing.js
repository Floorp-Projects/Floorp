XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");

let gTab = null;
let gListener = null;
const kURL = "data:text/html;charset=utf-8,Caret browsing is fun.<input id='in'>";

const kPrefShortcutEnabled = "accessibility.browsewithcaret_shortcut.enabled";
const kPrefWarnOnEnable    = "accessibility.warn_on_browsewithcaret";
const kPrefCaretBrowsingOn = "accessibility.browsewithcaret";

let oldPrefs = {};
for (let pref of [kPrefShortcutEnabled, kPrefWarnOnEnable, kPrefCaretBrowsingOn]) {
  oldPrefs[pref] = Services.prefs.getBoolPref(pref);
}

Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

registerCleanupFunction(function() {
  if (gTab)
    gBrowser.removeTab(gTab);
  if (gListener)
    Services.wm.removeListener(gListener);

  for (let pref of [kPrefShortcutEnabled, kPrefWarnOnEnable, kPrefCaretBrowsingOn]) {
    Services.prefs.setBoolPref(pref, oldPrefs[pref]);
  }
});

function promiseWaitForFocusEvent(el) {
  let deferred = Promise.defer();
  el.addEventListener("focus", function listener() {
    el.removeEventListener("focus", listener, false);
    deferred.resolve();
  }, false);
  return deferred.promise;
}

function promiseTestPageLoad() {
  let deferred = Promise.defer();
  info("Waiting for test page to load.");

  gTab = gBrowser.selectedTab = gBrowser.addTab(kURL);
  let browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function listener() {
    if (browser.currentURI.spec == "about:blank")
      return;
    info("Page loaded: " + browser.currentURI.spec);
    browser.removeEventListener("load", listener, true);

    deferred.resolve();
  }, true);

  return deferred.promise;
}

function promiseCaretPromptOpened() {
  let deferred = Promise.defer();
  if (gListener) {
    console.trace();
    ok(false, "Should not be waiting for another prompt right now.");
    return false;
  }
  info("Waiting for caret prompt to open");
  gListener = {
    onOpenWindow: function(win) {
      let window = win.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindow);
      window.addEventListener("load", function listener() {
        window.removeEventListener("load", listener);
        if (window.location.href == "chrome://global/content/commonDialog.xul") {
          info("Caret prompt opened, removing listener and focusing");
          Services.wm.removeListener(gListener);
          gListener = null;
          deferred.resolve(window);
        }
      });
    },
    onCloseWindow: function() {},
  };
  Services.wm.addListener(gListener);
  return deferred.promise;
}

function hitF7(async = true) {
  let f7 = () => EventUtils.sendKey("F7", window.content);
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
  if (gListener) {
    Services.wm.removeListener(gListener);
    gListener = null;
  }
  let expectedStr = expected ? "on." : "off.";
  ok(!openedDialog, "Shouldn't open a dialog to turn caret browsing " + expectedStr);
  let prefVal = Services.prefs.getBoolPref(kPrefCaretBrowsingOn);
  is(prefVal, expected, "Caret browsing should now be " + expectedStr);
}

add_task(function* checkTogglingCaretBrowsing() {
  yield promiseTestPageLoad();
  let textEl = window.content.document.getElementById("in");
  textEl.focus();

  let promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  let prompt = yield promiseGotKey;
  let doc = prompt.document;
  is(doc.documentElement.defaultButton, "cancel", "'No' button should be the default");
  ok(!doc.getElementById("checkbox").checked, "Checkbox shouldn't be checked by default.");
  let promiseInputFocused = promiseWaitForFocusEvent(textEl);
  doc.documentElement.cancelDialog();
  yield promiseInputFocused;
  ok(!Services.prefs.getBoolPref(kPrefCaretBrowsingOn), "Caret browsing should still be off after cancelling the dialog.");

  promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  prompt = yield promiseGotKey;

  doc = prompt.document;
  is(doc.documentElement.defaultButton, "cancel", "'No' button should be the default");
  ok(!doc.getElementById("checkbox").checked, "Checkbox shouldn't be checked by default.");
  promiseInputFocused = promiseWaitForFocusEvent(textEl);
  doc.documentElement.acceptDialog();
  yield promiseInputFocused;
  ok(Services.prefs.getBoolPref(kPrefCaretBrowsingOn), "Caret browsing should be on after accepting the dialog.");

  syncToggleCaretNoDialog(false);

  promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  prompt = yield promiseGotKey;
  doc = prompt.document;

  is(doc.documentElement.defaultButton, "cancel", "'No' button should be the default");
  ok(!doc.getElementById("checkbox").checked, "Checkbox shouldn't be checked by default.");

  promiseInputFocused = promiseWaitForFocusEvent(textEl);
  doc.documentElement.cancelDialog();
  yield promiseInputFocused;

  ok(!Services.prefs.getBoolPref(kPrefCaretBrowsingOn), "Caret browsing should still be off after cancelling the dialog.");

  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

  gBrowser.removeTab(gTab);
  gTab = null;
});

add_task(function* toggleCheckboxNoCaretBrowsing() {
  yield promiseTestPageLoad();
  let textEl = window.content.document.getElementById("in");
  textEl.focus();

  let promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  let prompt = yield promiseGotKey;
  let doc = prompt.document;
  is(doc.documentElement.defaultButton, "cancel", "'No' button should be the default");
  let checkbox = doc.getElementById("checkbox");
  ok(!checkbox.checked, "Checkbox shouldn't be checked by default.");

  // Check the box:
  checkbox.click();
  let promiseInputFocused = promiseWaitForFocusEvent(textEl);
  // Say no:
  doc.documentElement.getButton("cancel").click();
  yield promiseInputFocused;
  ok(!Services.prefs.getBoolPref(kPrefCaretBrowsingOn), "Caret browsing should still be off.");

  ok(!Services.prefs.getBoolPref(kPrefShortcutEnabled), "Shortcut should now be disabled.");

  syncToggleCaretNoDialog(false);
  ok(!Services.prefs.getBoolPref(kPrefShortcutEnabled), "Shortcut should still be disabled.");

  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

  gBrowser.removeTab(gTab);
  gTab = null;
});


add_task(function* toggleCheckboxNoCaretBrowsing() {
  yield promiseTestPageLoad();
  let textEl = window.content.document.getElementById("in");
  textEl.focus();

  let promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  let prompt = yield promiseGotKey;
  let doc = prompt.document;
  is(doc.documentElement.defaultButton, "cancel", "'No' button should be the default");
  let checkbox = doc.getElementById("checkbox");
  ok(!checkbox.checked, "Checkbox shouldn't be checked by default.");

  // Check the box:
  checkbox.click();
  let promiseInputFocused = promiseWaitForFocusEvent(textEl);
  // Say yes:
  doc.documentElement.acceptDialog();
  yield promiseInputFocused;
  ok(Services.prefs.getBoolPref(kPrefCaretBrowsingOn), "Caret browsing should now be on.");
  ok(Services.prefs.getBoolPref(kPrefShortcutEnabled), "Shortcut should still be enabled.");
  ok(!Services.prefs.getBoolPref(kPrefWarnOnEnable), "Should no longer warn when enabling.");


  syncToggleCaretNoDialog(false);
  syncToggleCaretNoDialog(true);
  syncToggleCaretNoDialog(false);
  
  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

  gBrowser.removeTab(gTab);
  gTab = null;
});




