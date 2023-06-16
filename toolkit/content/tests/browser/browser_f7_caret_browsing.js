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

registerCleanupFunction(function () {
  for (let pref of [
    kPrefShortcutEnabled,
    kPrefWarnOnEnable,
    kPrefCaretBrowsingOn,
  ]) {
    Services.prefs.setBoolPref(pref, oldPrefs[pref]);
  }
});

// NB: not using BrowserTestUtils.promiseAlertDialog here because there's no way to
// undo waiting for a dialog. If we don't want the window to be opened, and
// wait for it to verify that it indeed does not open, we need to be able to
// then "stop" waiting so that when we next *do* want it to open, our "old"
// listener doesn't fire and do things we don't want (like close the window...).
let gCaretPromptOpeningObserver;
function promiseCaretPromptOpened() {
  return new Promise(resolve => {
    function observer(subject, topic, data) {
      info("Dialog opened.");
      resolve(subject);
      gCaretPromptOpeningObserver();
    }
    Services.obs.addObserver(observer, "common-dialog-loaded");
    gCaretPromptOpeningObserver = () => {
      Services.obs.removeObserver(observer, "common-dialog-loaded");
      gCaretPromptOpeningObserver = () => {};
    };
  });
}

function hitF7() {
  SimpleTest.executeSoon(() => EventUtils.synthesizeKey("KEY_F7"));
}

async function toggleCaretNoDialog(expected) {
  let openedDialog = false;
  promiseCaretPromptOpened().then(function (win) {
    openedDialog = true;
    win.close(); // This will eventually return focus here and allow the test to continue...
  });
  // Cause the dialog to appear synchronously when focused element is in chrome,
  // otherwise, i.e., when focused element is in remote content, it appears
  // asynchronously.
  const focusedElementInChrome = Services.focus.focusedElement;
  const isAsync = focusedElementInChrome?.isRemoteBrowser;
  const waitForF7KeyHandled = new Promise(resolve => {
    let eventCount = 0;
    const expectedEventCount = isAsync ? 2 : 1;
    let listener = async event => {
      if (event.key == "F7") {
        info("F7 keypress is fired");
        if (++eventCount == expectedEventCount) {
          window.removeEventListener("keypress", listener, {
            capture: true,
            mozSystemGroup: true,
          });
          // Wait for the event handled in chrome.
          await TestUtils.waitForTick();
          resolve();
          return;
        }
        info(
          "Waiting for next F7 keypress which is a reply event from the remote content"
        );
      }
    };
    info(
      `Synthesizing "F7" key press and wait ${expectedEventCount} keypress events...`
    );
    window.addEventListener("keypress", listener, {
      capture: true,
      mozSystemGroup: true,
    });
  });
  hitF7();
  await waitForF7KeyHandled;

  let expectedStr = expected ? "on." : "off.";
  ok(
    !openedDialog,
    "Shouldn't open a dialog to turn caret browsing " + expectedStr
  );
  // Need to clean up if the dialog wasn't opened, so the observer doesn't get
  // re-triggered later on causing "issues".
  if (!openedDialog) {
    gCaretPromptOpeningObserver();
  }
  let prefVal = Services.prefs.getBoolPref(kPrefCaretBrowsingOn);
  is(prefVal, expected, "Caret browsing should now be " + expectedStr);
}

function waitForFocusOnInput(browser) {
  return SpecialPowers.spawn(browser, [], async function () {
    let textEl = content.document.getElementById("in");
    return ContentTaskUtils.waitForCondition(() => {
      return content.document.activeElement == textEl;
    }, "Input should get focused.");
  });
}

function focusInput(browser) {
  return SpecialPowers.spawn(browser, [], async function () {
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

  await toggleCaretNoDialog(false);

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

  await toggleCaretNoDialog(false);
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

  await toggleCaretNoDialog(false);
  await toggleCaretNoDialog(true);
  await toggleCaretNoDialog(false);

  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);

  BrowserTestUtils.removeTab(tab);
});

// Test for bug 1743878: Many repeated modal caret-browsing dialogs, if you
// accidentally hold down F7 for a few seconds
add_task(async function testF7SpamDoesNotOpenDialogs() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  registerCleanupFunction(() => BrowserTestUtils.removeTab(tab));

  let promiseGotKey = promiseCaretPromptOpened();
  hitF7();
  let prompt = await promiseGotKey;
  let doc = prompt.document;
  let dialog = doc.getElementById("commonDialog");

  let promiseDialogUnloaded = BrowserTestUtils.waitForEvent(prompt, "unload");

  // Listen for an additional prompt to open, which should not happen.
  let promiseDialogOrTimeout = () =>
    Promise.race([
      promiseCaretPromptOpened(),
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      new Promise(resolve => setTimeout(resolve, 100)),
    ]);

  let openedPromise = promiseDialogOrTimeout();

  // Hit F7 two more times: once to test that _awaitingToggleCaretBrowsingPrompt
  // is applied, and again to test that its value isn't somehow reset by
  // pressing F7 while the dialog is open.
  for (let i = 0; i < 2; i++) {
    await new Promise(resolve =>
      SimpleTest.executeSoon(() => {
        hitF7();
        resolve();
      })
    );
  }

  // Say no:
  dialog.cancelDialog();
  await promiseDialogUnloaded;
  info("Dialog unloaded");

  let openedDialog = await openedPromise;
  ok(!openedDialog, "No additional dialog should have opened.");

  // If the test fails, clean up any dialogs we erroneously opened so they don't
  // interfere with other tests.
  let extraDialogs = 0;
  while (openedDialog) {
    extraDialogs += 1;
    let doc = openedDialog.document;
    let dialog = doc.getElementById("commonDialog");
    openedPromise = promiseDialogOrTimeout();
    dialog.cancelDialog();
    openedDialog = await openedPromise;
  }
  if (extraDialogs) {
    info(`Closed ${extraDialogs} extra dialogs.`);
  }

  // Either way, we now have an extra observer, so clean it up.
  gCaretPromptOpeningObserver();

  Services.prefs.setBoolPref(kPrefShortcutEnabled, true);
  Services.prefs.setBoolPref(kPrefWarnOnEnable, true);
  Services.prefs.setBoolPref(kPrefCaretBrowsingOn, false);
});
