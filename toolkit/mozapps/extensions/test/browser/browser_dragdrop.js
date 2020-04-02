/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ABOUT_ADDONS_URL = "chrome://mozapps/content/extensions/aboutaddons.html";

const dragService = Cc["@mozilla.org/widget/dragservice;1"].getService(
  Ci.nsIDragService
);

// Test that the drag-drop-addon-installer component installs add-ons and is
// included in about:addons. There is an issue with EventUtils.synthesizeDrop
// where it throws an exception when you give it an subbrowser so we test
// the component directly.

async function checkInstallConfirmation(...names) {
  let notificationCount = 0;
  let observer = {
    observe(aSubject, aTopic, aData) {
      let installInfo = aSubject.wrappedJSObject;
      isnot(
        installInfo.browser,
        null,
        "Notification should have non-null browser"
      );

      is(
        installInfo.installs.length,
        1,
        "Got one AddonInstall instance as expected"
      );

      Assert.deepEqual(
        installInfo.installs[0].installTelemetryInfo,
        { source: "about:addons", method: "drag-and-drop" },
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

function getDragOverTarget(win) {
  return win.document.querySelector("categories-box");
}

function getDropTarget(win) {
  return win.document.querySelector("drag-drop-addon-installer");
}

function withTestPage(fn) {
  return BrowserTestUtils.withNewTab(
    { url: ABOUT_ADDONS_URL, gBrowser },
    async browser => {
      let win = browser.contentWindow;
      await win.customElements.whenDefined("drag-drop-addon-installer");
      await fn(browser);
    }
  );
}

function initDragSession({ dragData, dropEffect }) {
  let dropAction;
  switch (dropEffect) {
    case null:
    case undefined:
    case "move":
      dropAction = _EU_Ci.nsIDragService.DRAGDROP_ACTION_MOVE;
      break;
    case "copy":
      dropAction = _EU_Ci.nsIDragService.DRAGDROP_ACTION_COPY;
      break;
    case "link":
      dropAction = _EU_Ci.nsIDragService.DRAGDROP_ACTION_LINK;
      break;
    default:
      throw new Error(`${dropEffect} is an invalid drop effect value`);
  }

  const dataTransfer = new DataTransfer();
  dataTransfer.dropEffect = dropEffect;

  for (let i = 0; i < dragData.length; i++) {
    const item = dragData[i];
    for (let j = 0; j < item.length; j++) {
      dataTransfer.mozSetDataAt(item[j].type, item[j].data, i);
    }
  }

  dragService.startDragSessionForTests(dropAction);
  const session = dragService.getCurrentSession();
  session.dataTransfer = dataTransfer;

  return session;
}

async function simulateDragAndDrop(win, dragData) {
  const dropTarget = getDropTarget(win);
  const dragOverTarget = getDragOverTarget(win);
  const dropEffect = "move";

  const session = initDragSession({ dragData, dropEffect });

  info("Simulate drag over and wait for the drop target to be visible");

  EventUtils.synthesizeDragOver(
    dragOverTarget,
    dragOverTarget,
    dragData,
    dropEffect,
    win
  );

  // This make sure that the fake dataTransfer has still
  // the expected drop effect after the synthesizeDragOver call.
  session.dataTransfer.dropEffect = "move";

  await BrowserTestUtils.waitForCondition(
    () => !dropTarget.hidden,
    "Wait for the drop target element to be visible"
  );

  info("Simulate drop dragData on drop target");

  EventUtils.synthesizeDropAfterDragOver(
    null,
    session.dataTransfer,
    dropTarget,
    win,
    { _domDispatchOnly: true }
  );

  dragService.endDragSession(true);
}

// Simulates dropping a URL onto the manager
add_task(async function test_drop_url() {
  for (let fileType of ["xpi", "zip"]) {
    await withTestPage(async browser => {
      const url = TESTROOT + `addons/browser_dragdrop1.${fileType}`;
      const promise = checkInstallConfirmation("Drag Drop test 1");

      await simulateDragAndDrop(browser.contentWindow, [
        [{ type: "text/x-moz-url", data: url }],
      ]);

      await promise;
    });
  }
});

// Simulates dropping a file onto the manager
add_task(async function test_drop_file() {
  for (let fileType of ["xpi", "zip"]) {
    await withTestPage(async browser => {
      let fileurl = get_addon_file_url(`browser_dragdrop1.${fileType}`);
      let promise = checkInstallConfirmation("Drag Drop test 1");

      await simulateDragAndDrop(browser.contentWindow, [
        [{ type: "application/x-moz-file", data: fileurl.file }],
      ]);

      await promise;
    });
  }
});

// Simulates dropping two urls onto the manager
add_task(async function test_drop_multiple_urls() {
  await withTestPage(async browser => {
    let url1 = TESTROOT + "addons/browser_dragdrop1.xpi";
    let url2 = TESTROOT2 + "addons/browser_dragdrop2.zip";
    let promise = checkInstallConfirmation(
      "Drag Drop test 1",
      "Drag Drop test 2"
    );

    await simulateDragAndDrop(browser.contentWindow, [
      [{ type: "text/x-moz-url", data: url1 }],
      [{ type: "text/x-moz-url", data: url2 }],
    ]);

    await promise;
  });
}).skip(); // TODO(rpl): this fails because mozSetDataAt throws IndexSizeError.

// Simulates dropping two files onto the manager
add_task(async function test_drop_multiple_files() {
  await withTestPage(async browser => {
    let fileurl1 = get_addon_file_url("browser_dragdrop1.zip");
    let fileurl2 = get_addon_file_url("browser_dragdrop2.xpi");
    let promise = checkInstallConfirmation(
      "Drag Drop test 1",
      "Drag Drop test 2"
    );

    await simulateDragAndDrop(browser.contentWindow, [
      [{ type: "application/x-moz-file", data: fileurl1.file }],
      [{ type: "application/x-moz-file", data: fileurl2.file }],
    ]);

    await promise;
  });
}).skip(); // TODO(rpl): this fails because mozSetDataAt throws IndexSizeError.

// Simulates dropping a file and a url onto the manager (weird, but should still work)
add_task(async function test_drop_file_and_url() {
  await withTestPage(async browser => {
    let url = TESTROOT + "addons/browser_dragdrop1.xpi";
    let fileurl = get_addon_file_url("browser_dragdrop2.zip");
    let promise = checkInstallConfirmation(
      "Drag Drop test 1",
      "Drag Drop test 2"
    );

    await simulateDragAndDrop(browser.contentWindow, [
      [{ type: "text/x-moz-url", data: url }],
      [{ type: "application/x-moz-file", data: fileurl.file }],
    ]);

    await promise;
  });
}).skip(); // TODO(rpl): this fails because mozSetDataAt throws IndexSizeError.

// Test that drag-and-drop of an incompatible addon generates
// an error.
add_task(async function test_drop_incompat_file() {
  await withTestPage(async browser => {
    let url = `${TESTROOT}/addons/browser_dragdrop_incompat.xpi`;

    let panelPromise = promisePopupNotificationShown("addon-install-failed");
    await simulateDragAndDrop(browser.contentWindow, [
      [{ type: "text/x-moz-url", data: url }],
    ]);

    let panel = await panelPromise;
    ok(panel, "Got addon-install-failed popup");
    panel.button.click();
  });
});
