/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var kPrefName = "accessibility.typeaheadfind.prefillwithselection";
var kEmptyURI = "data:text/html,";

// This pref is false by default in OSX; ensure the test still works there.
Services.prefs.setBoolPref(kPrefName, true);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref(kPrefName);
});

add_task(function* () {
  let aTab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, kEmptyURI);
  ok(!gFindBarInitialized, "findbar isn't initialized yet");

  // Note: the use case here is when the user types directly in the findbar
  // _before_ it's prefilled with a text selection in the page.

  // So `yield BrowserTestUtils.sendChar()` can't be used here:
  //  - synthesizing a key in the browser won't actually send it to the
  //    findbar; the findbar isn't part of the browser content.
  //  - we need to _not_ wait for _startFindDeferred to be resolved; yielding
  //    a synthesized keypress on the browser implicitely happens after the
  //    browser has dispatched its return message with the prefill value for
  //    the findbar, which essentially nulls these tests.

  let findBar = gFindBar;
  is(findBar._findField.value, "", "findbar is empty");

  // Test 1
  //  Any input in the findbar should erase a previous search.

  findBar._findField.value = "xy";
  findBar.startFind();
  is(findBar._findField.value, "xy", "findbar should have xy initial query");
  is(findBar._findField.mInputField,
    document.activeElement,
    "findbar is now focused");

  EventUtils.sendChar("z", window);
  is(findBar._findField.value, "z", "z erases xy");

  findBar._findField.value = "";
  ok(!findBar._findField.value, "erase findbar after first test");

  // Test 2
  //  Prefilling the findbar should be ignored if a search has been run.

  findBar.startFind();
  ok(findBar._startFindDeferred, "prefilled value hasn't been fetched yet");
  is(findBar._findField.mInputField,
    document.activeElement,
    "findbar is still focused");

  EventUtils.sendChar("a", window);
  EventUtils.sendChar("b", window);
  is(findBar._findField.value, "ab", "initial ab typed in the findbar");

  // This resolves _startFindDeferred if it's still pending; let's just skip
  // over waiting for the browser's return message that should do this as it
  // doesn't really matter.
  findBar.onCurrentSelection("foo", true);
  ok(!findBar._startFindDeferred, "prefilled value fetched");
  is(findBar._findField.value, "ab", "ab kept instead of prefill value");

  EventUtils.sendChar("c", window);
  is(findBar._findField.value, "abc", "c is appended after ab");

  // Clear the findField value to make the test  run successfully
  // for multiple runs in the same browser session.
  findBar._findField.value = "";
  yield BrowserTestUtils.removeTab(aTab);
});
