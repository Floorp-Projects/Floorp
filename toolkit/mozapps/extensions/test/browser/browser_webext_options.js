/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://testing-common/ContentTask.jsm", {});

// Wrapper to run a test that consists of:
//  1. opening the add-ons manager viewing the list of extensions
//  2. installing an extension (using the provider installer callable)
//  3. opening the preferences panel for the new extension and verifying
//     that it opens cleanly
async function runTest(installer) {
  let mgrWindow = await open_manager("addons://list/extension");

  let {addon, id} = await installer();
  isnot(addon, null, "Extension is installed");

  let element = get_addon_element(mgrWindow, id);
  element.parentNode.ensureElementIsVisible(element);

  let button = mgrWindow.document.getAnonymousElementByAttribute(element, "anonid", "preferences-btn");
  is_element_visible(button, "Preferences button should be visible");

  EventUtils.synthesizeMouseAtCenter(button, {clickCount: 1}, mgrWindow);

  await TestUtils.topicObserved(AddonManager.OPTIONS_NOTIFICATION_DISPLAYED,
                                (subject, data) => data == id);

  is(mgrWindow.gViewController.currentViewId,
     `addons://detail/${encodeURIComponent(id)}/preferences`,
     "Current view should scroll to preferences");

  var browser = mgrWindow.document.querySelector("#detail-grid > rows > .inline-options-browser");
  var rows = browser.parentNode;

  let url = await ContentTask.spawn(browser, {}, () => content.location.href);

  ok(browser, "Grid should have a browser child");
  is(browser.localName, "browser", "Grid should have a browser child");
  is(url, element.mAddon.optionsURL, "Browser has the expected options URL loaded")

  is(browser.clientWidth, rows.clientWidth,
     "Browser should be the same width as its parent node");

  button = mgrWindow.document.getElementById("detail-prefs-btn");
  is_element_hidden(button, "Preferences button should not be visible");

  await close_manager(mgrWindow);

  addon.uninstall();
}

function promiseWebExtensionStartup() {
  const {Management} = Components.utils.import("resource://gre/modules/Extension.jsm", {});

  return new Promise(resolve => {
    let listener = (event, extension) => {
      Management.off("startup", listener);
      resolve(extension);
    };

    Management.on("startup", listener);
  });
}

// Test that deferred handling of optionsURL works for a signed webextension
add_task(async function test_options_signed() {
  await runTest(async function() {
    // The extension in-tree is signed with this ID:
    const ID = "{9792932b-32b2-4567-998c-e7bf6c4c5e35}";

    await Promise.all([
      promiseWebExtensionStartup(),
      install_addon("addons/options_signed.xpi"),
    ]);
    let addon = await promiseAddonByID(ID);

    return {addon, id: ID};
  });
});

add_task(async function test_options_temporary() {
  await runTest(async function() {
    let dir = get_addon_file_url("options_signed").file;
    let [addon] = await Promise.all([
      AddonManager.installTemporaryAddon(dir),
      promiseWebExtensionStartup(),
    ]);
    isnot(addon, null, "Extension is installed (temporarily)");

    return {addon, id: addon.id};
  });
});
