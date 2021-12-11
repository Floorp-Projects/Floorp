/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests bug 567127 - Add install button to the add-ons manager

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

async function checkInstallConfirmation(...names) {
  let notificationCount = 0;
  let observer = {
    observe(aSubject, aTopic, aData) {
      var installInfo = aSubject.wrappedJSObject;
      isnot(
        installInfo.browser,
        null,
        "Notification should have non-null browser"
      );
      Assert.deepEqual(
        installInfo.installs[0].installTelemetryInfo,
        {
          source: "about:addons",
          method: "install-from-file",
        },
        "Got the expected installTelemetryInfo"
      );
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
      info(
        `Waiting for installs for ${names.filter(n => !results.includes(n))}`
      );

      promise = promisePopupNotificationShown("addon-webext-permissions");
    }
    panel.secondaryButton.click();
  }

  Assert.deepEqual(results.sort(), names.sort(), "Got expected installs");

  is(
    notificationCount,
    names.length,
    `Saw ${names.length} addon-install-started notification`
  );
  Services.obs.removeObserver(observer, "addon-install-started");
}

add_task(async function test_install_from_file() {
  let win = await loadInitialView("extension");

  var filePaths = [
    get_addon_file_url("browser_dragdrop1.xpi"),
    get_addon_file_url("browser_dragdrop2.xpi"),
  ];
  for (let uri of filePaths) {
    ok(uri.file != null, `Should have file for ${uri.spec}`);
    ok(uri.file instanceof Ci.nsIFile, `Should have nsIFile for ${uri.spec}`);
  }
  MockFilePicker.setFiles(filePaths.map(aPath => aPath.file));

  // Set handler that executes the core test after the window opens,
  // and resolves the promise when the window closes
  let pInstallURIClosed = checkInstallConfirmation(
    "Drag Drop test 1",
    "Drag Drop test 2"
  );

  win.document
    .querySelector('#page-options [action="install-from-file"]')
    .click();

  await pInstallURIClosed;

  MockFilePicker.cleanup();
  await closeView(win);
});

add_task(async function test_install_disabled() {
  let win = await loadInitialView("extension");
  let doc = win.document;

  let pageOptionsMenu = doc.querySelector("addon-page-options panel-list");

  function openPageOptions() {
    let opened = BrowserTestUtils.waitForEvent(pageOptionsMenu, "shown");
    pageOptionsMenu.open = true;
    return opened;
  }

  function closePageOptions() {
    let closed = BrowserTestUtils.waitForEvent(pageOptionsMenu, "hidden");
    pageOptionsMenu.open = false;
    return closed;
  }

  await openPageOptions();
  let installButton = doc.querySelector('[action="install-from-file"]');
  ok(!installButton.hidden, "The install button is shown");
  await closePageOptions();

  await SpecialPowers.pushPrefEnv({ set: [[PREF_XPI_ENABLED, false]] });

  await openPageOptions();
  ok(installButton.hidden, "The install button is now hidden");
  await closePageOptions();

  await SpecialPowers.popPrefEnv();

  await openPageOptions();
  ok(!installButton.hidden, "The install button is shown again");
  await closePageOptions();

  await closeView(win);
});
