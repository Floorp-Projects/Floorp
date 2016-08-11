/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Wrapper to run a test that consists of:
//  1. opening the add-ons manager viewing the list of extensions
//  2. installing an extension (using the provider installer callable)
//  3. opening the preferences panel for the new extension and verifying
//     that it opens cleanly
function* runTest(installer) {
  let mgrWindow = yield open_manager("addons://list/extension");

  let {addon, id} = yield installer();
  isnot(addon, null, "Extension is installed");

  let element = get_addon_element(mgrWindow, id);
  element.parentNode.ensureElementIsVisible(element);

  let button = mgrWindow.document.getAnonymousElementByAttribute(element, "anonid", "preferences-btn");
  is_element_visible(button, "Preferences button should be visible");

  EventUtils.synthesizeMouseAtCenter(button, {clickCount: 1}, mgrWindow);

  yield TestUtils.topicObserved(AddonManager.OPTIONS_NOTIFICATION_DISPLAYED,
                                (subject, data) => data == id);

  is(mgrWindow.gViewController.currentViewId,
     `addons://detail/${encodeURIComponent(id)}/preferences`,
     "Current view should scroll to preferences");

  var browser = mgrWindow.document.querySelector("#detail-grid > rows > .inline-options-browser");
  var rows = browser.parentNode;

  ok(browser, "Grid should have a browser child");
  is(browser.localName, "browser", "Grid should have a browser child");
  is(browser.currentURI.spec, element.mAddon.optionsURL, "Browser has the expected options URL loaded")

  is(browser.clientWidth, rows.clientWidth,
     "Browser should be the same width as its parent node");

  button = mgrWindow.document.getElementById("detail-prefs-btn");
  is_element_hidden(button, "Preferences button should not be visible");

  yield close_manager(mgrWindow);

  addon.uninstall();
}

// Test that deferred handling of optionsURL works for a signed webextension
add_task(function test_options_signed() {
  return runTest(function*() {
    // The extension in-tree is signed with this ID:
    const ID = "{9792932b-32b2-4567-998c-e7bf6c4c5e35}";

    yield install_addon("addons/options_signed.xpi");
    let addon = yield promiseAddonByID(ID);

    return {addon, id: ID};
  });
});

add_task(function* test_options_temporary() {
  return runTest(function*() {
    let dir = get_addon_file_url("options_signed").file;
    let addon = yield AddonManager.installTemporaryAddon(dir);
    isnot(addon, null, "Extension is installed (temporarily)");

    return {addon, id: addon.id};
  });
});
