/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Bug 570760 - Make ctrl-f and / focus the search box in the add-ons manager

var gManagerWindow;
var focusCount = 0;

async function test() {
  waitForExplicitFinish();

  // The discovery pane does not display the about:addons searchbox,
  // open the extensions pane instead.
  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;

  var searchBox = gManagerWindow.document.getElementById("header-search");
  function focusHandler() {
    searchBox.blur();
    focusCount++;
  }
  searchBox.inputField.addEventListener("focus", focusHandler);
  f_key_test();
  slash_key_test();
  searchBox.inputField.removeEventListener("focus", focusHandler);
  end_test();
}

function end_test() {
  close_manager(gManagerWindow, finish);
}

function f_key_test() {
  EventUtils.synthesizeKey("f", { accelKey: true }, gManagerWindow);
  is(focusCount, 1, "Search box should have been focused due to the f key");
}

function slash_key_test() {
  EventUtils.synthesizeKey("/", { }, gManagerWindow);
  is(focusCount, 2, "Search box should have been focused due to the / key");
}
