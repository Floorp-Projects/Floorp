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

/**
 * Wait for the given PopupNotification to display
 *
 * @param {string} name
 *        The name of the notification to wait for.
 *
 * @returns {Promise}
 *          Resolves with the notification window.
 */
function promisePopupNotificationShown(name) {
  return new Promise(resolve => {
    function popupshown() {
      let notification = PopupNotifications.getNotification(name);
      if (!notification) { return; }

      ok(notification, `${name} notification shown`);
      ok(PopupNotifications.isPanelOpen, "notification panel open");

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      resolve(PopupNotifications.panel.firstChild);
    }

    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

async function checkInstallConfirmation(...names) {
  let notificationCount = 0;
  let observer = {
    observe(aSubject, aTopic, aData) {
      var installInfo = aSubject.wrappedJSObject;
      isnot(installInfo.browser, null, "Notification should have non-null browser");
      notificationCount++;
    }
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

  executeSoon(run_next_test);
}

async function test() {
  waitForExplicitFinish();

  let aWindow = await open_manager("addons://list/extension");
  gManagerWindow = aWindow;
  run_next_test();
}

async function end_test() {
  await close_manager(gManagerWindow);
  finish();
}

// Simulates dropping a URL onto the manager
add_test(function test_drop_url() {
  var url = TESTROOT + "addons/browser_dragdrop1.xpi";

  checkInstallConfirmation("Drag Drop test 1");

  var viewContainer = gManagerWindow.document.getElementById("view-port");
  var effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
               [[{type: "text/x-moz-url", data: url}]],
               "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
});

// Simulates dropping a file onto the manager
add_test(function test_drop_file() {
  var fileurl = get_addon_file_url("browser_dragdrop1.xpi");

  checkInstallConfirmation("Drag Drop test 1");

  var viewContainer = gManagerWindow.document.getElementById("view-port");
  var effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
               [[{type: "application/x-moz-file", data: fileurl.file}]],
               "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
});

// Simulates dropping two urls onto the manager
add_test(function test_drop_multiple_urls() {
  var url1 = TESTROOT + "addons/browser_dragdrop1.xpi";
  var url2 = TESTROOT2 + "addons/browser_dragdrop2.xpi";

  checkInstallConfirmation("Drag Drop test 1", "Drag Drop test 2");

  var viewContainer = gManagerWindow.document.getElementById("view-port");
  var effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
               [[{type: "text/x-moz-url", data: url1}],
                [{type: "text/x-moz-url", data: url2}]],
               "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
});

// Simulates dropping two files onto the manager
add_test(function test_drop_multiple_files() {
  var fileurl1 = get_addon_file_url("browser_dragdrop1.xpi");
  var fileurl2 = get_addon_file_url("browser_dragdrop2.xpi");

  checkInstallConfirmation("Drag Drop test 1", "Drag Drop test 2");

  var viewContainer = gManagerWindow.document.getElementById("view-port");
  var effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
               [[{type: "application/x-moz-file", data: fileurl1.file}],
                [{type: "application/x-moz-file", data: fileurl2.file}]],
               "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
});

// Simulates dropping a file and a url onto the manager (weird, but should still work)
add_test(function test_drop_file_and_url() {
  var url = TESTROOT + "addons/browser_dragdrop1.xpi";
  var fileurl = get_addon_file_url("browser_dragdrop2.xpi");

  checkInstallConfirmation("Drag Drop test 1", "Drag Drop test 2");

  var viewContainer = gManagerWindow.document.getElementById("view-port");
  var effect = EventUtils.synthesizeDrop(viewContainer, viewContainer,
               [[{type: "text/x-moz-url", data: url}],
                [{type: "application/x-moz-file", data: fileurl.file}]],
               "copy", gManagerWindow);
  is(effect, "copy", "Drag should be accepted");
});
