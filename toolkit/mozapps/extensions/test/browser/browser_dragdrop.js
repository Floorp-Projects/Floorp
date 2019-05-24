/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This tests simulated drag and drop of files into the add-ons manager.
// We test with the add-ons manager in its own tab if in Firefox otherwise
// in its own window.
// Tests are only simulations of the drag and drop events, we cannot really do
// this automatically.

// Instead of loading EventUtils.js into the test scope in browser-test.js for all tests,
// we only need EventUtils.js for a few files which is why we are using loadSubScript.
var gManagerWindow;
var EventUtils = {};
Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

async function checkInstallConfirmation(...names) {
  let notificationCount = 0;
  let observer = {
    observe(aSubject, aTopic, aData) {
      let installInfo = aSubject.wrappedJSObject;
      isnot(installInfo.browser, null, "Notification should have non-null browser");

      is(installInfo.installs.length, 1, "Got one AddonInstall instance as expected");

      Assert.deepEqual(installInfo.installs[0].installTelemetryInfo,
                       {source: "about:addons", method: "drag-and-drop"},
                       "Got the expected installTelemetryInfo");

      notificationCount++;
    },
  };
  Services.obs.addObserver(observer, "addon-install-started");

  let results = [];

  let promise = promisePopupNotificationShown("addon-webext-permissions");
  for (let i = 0; i < names.length; i++) {
    let panel = await promise;
    let name = panel.getAttribute("name");
    results.push(name);

    info(`Saw install for ${name}`);
    if (results.length < names.length) {
      info(`Waiting for installs for ${names.filter(n => !results.includes(n))}`);

      promise = promisePopupNotificationShown("addon-webext-permissions");
    }
    panel.secondaryButton.click();
  }

  Assert.deepEqual(results.sort(), names.sort(), "Got expected installs");

  is(notificationCount, names.length, `Saw ${names.length} addon-install-started notification`);
  Services.obs.removeObserver(observer, "addon-install-started");
}

function getViewContainer(gManagerWindow) {
  if (gManagerWindow.useHtmlViews) {
    return gManagerWindow.document.getElementById("category-box");
  }
  return gManagerWindow.document.getElementById("view-port");
}

// Simulates dropping a URL onto the manager
add_task(async function test_drop_url() {
  let url = TESTROOT + "addons/browser_dragdrop1.xpi";
  gManagerWindow = await open_manager("addons://list/extension");
  let promise = checkInstallConfirmation("Drag Drop test 1");
  let viewContainer = getViewContainer(gManagerWindow);
  let effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
                                         [[{type: "text/x-moz-url", data: url}]],
                                         "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
  await promise;
  await close_manager(gManagerWindow);
});

// Simulates dropping a file onto the manager
add_task(async function test_drop_file() {
  let fileurl = get_addon_file_url("browser_dragdrop1.xpi");
  gManagerWindow = await open_manager("addons://list/extension");
  await wait_for_view_load(gManagerWindow);
  let promise = checkInstallConfirmation("Drag Drop test 1");
  let viewContainer = getViewContainer(gManagerWindow);
  let effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
                                         [[{type: "application/x-moz-file", data: fileurl.file}]],
                                         "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
  await promise;
  await close_manager(gManagerWindow);
});

// Simulates dropping two urls onto the manager
add_task(async function test_drop_multiple_urls() {
  let url1 = TESTROOT + "addons/browser_dragdrop1.xpi";
  let url2 = TESTROOT2 + "addons/browser_dragdrop2.xpi";
  gManagerWindow = await open_manager("addons://list/extension");
  await wait_for_view_load(gManagerWindow);
  let promise = checkInstallConfirmation("Drag Drop test 1", "Drag Drop test 2");
  let viewContainer = getViewContainer(gManagerWindow);
  let effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
                                         [[{type: "text/x-moz-url", data: url1}],
                                           [{type: "text/x-moz-url", data: url2}]],
                                         "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
  await promise;
  await close_manager(gManagerWindow);
});

// Simulates dropping two files onto the manager
add_task(async function test_drop_multiple_files() {
  let fileurl1 = get_addon_file_url("browser_dragdrop1.xpi");
  let fileurl2 = get_addon_file_url("browser_dragdrop2.xpi");
  gManagerWindow = await open_manager("addons://list/extension");
  await wait_for_view_load(gManagerWindow);
  let promise = checkInstallConfirmation("Drag Drop test 1", "Drag Drop test 2");
  let viewContainer = getViewContainer(gManagerWindow);
  let effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
                                         [[{type: "application/x-moz-file", data: fileurl1.file}],
                                           [{type: "application/x-moz-file", data: fileurl2.file}]],
                                         "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
  await promise;
  await close_manager(gManagerWindow);
});

// Simulates dropping a file and a url onto the manager (weird, but should still work)
add_task(async function test_drop_file_and_url() {
  let url = TESTROOT + "addons/browser_dragdrop1.xpi";
  let fileurl = get_addon_file_url("browser_dragdrop2.xpi");
  gManagerWindow = await open_manager("addons://list/extension");
  await wait_for_view_load(gManagerWindow);
  let promise = checkInstallConfirmation("Drag Drop test 1", "Drag Drop test 2");
  let viewContainer = getViewContainer(gManagerWindow);
  let effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
                                         [[{type: "text/x-moz-url", data: url}],
                                           [{type: "application/x-moz-file", data: fileurl.file}]],
                                         "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
  await promise;
  await close_manager(gManagerWindow);
});

// Test that drag-and-drop of an incompatible addon generates
// an error.
add_task(async function test_drop_incompat_file() {
  let gManagerWindow = await open_manager("addons://list/extension");
  await wait_for_view_load(gManagerWindow);

  let errorPromise = TestUtils.topicObserved("addon-install-failed");

  let url = `${TESTROOT}/addons/browser_dragdrop_incompat.xpi`;
  let viewContainer = getViewContainer(gManagerWindow);
  EventUtils.synthesizeDrop(viewContainer, viewContainer,
                            [[{type: "text/x-moz-url", data: url}]],
                            "copy", gManagerWindow);

  await errorPromise;
  ok(true, "Got addon-install-failed event");

  await close_manager(gManagerWindow);
});
