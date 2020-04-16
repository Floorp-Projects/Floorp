/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const ADDON_INSTALL_ID = "addon-webext-permissions";

let fileurl1 = get_addon_file_url("browser_dragdrop1.xpi");
let fileurl2 = get_addon_file_url("browser_dragdrop2.xpi");

function promiseInstallNotification(aBrowser) {
  return new Promise(resolve => {
    function popupshown(event) {
      let notification = PopupNotifications.getNotification(
        ADDON_INSTALL_ID,
        aBrowser
      );
      if (!notification) {
        return;
      }

      if (gBrowser.selectedBrowser !== aBrowser) {
        return;
      }

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      ok(true, `Got ${ADDON_INSTALL_ID} popup for browser`);
      event.target.firstChild.secondaryButton.click();
      resolve();
    }

    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

function CheckBrowserInPid(browser, expectedPid, message) {
  return SpecialPowers.spawn(browser, [{ expectedPid, message }], arg => {
    is(Services.appinfo.processID, arg.expectedPid, arg.message);
  });
}

async function testOpenedAndDraggedXPI(aBrowser) {
  // Get the current pid for browser for comparison later.
  let browserPid = await SpecialPowers.spawn(aBrowser, [], () => {
    return Services.appinfo.processID;
  });

  // No process switch for XPI file:// URI in the urlbar.
  let promiseNotification = promiseInstallNotification(aBrowser);
  let urlbar = gURLBar;
  urlbar.value = fileurl1.spec;
  urlbar.focus();
  EventUtils.synthesizeKey("KEY_Enter");
  await promiseNotification;
  await CheckBrowserInPid(
    aBrowser,
    browserPid,
    "Check that browser has not switched process."
  );

  // No process switch for XPI file:// URI dragged to tab.
  let tab = gBrowser.getTabForBrowser(aBrowser);
  promiseNotification = promiseInstallNotification(aBrowser);
  let effect = EventUtils.synthesizeDrop(
    tab,
    tab,
    [[{ type: "text/uri-list", data: fileurl1.spec }]],
    "move"
  );
  is(effect, "move", "Drag should be accepted");
  await promiseNotification;
  await CheckBrowserInPid(
    aBrowser,
    browserPid,
    "Check that browser has not switched process."
  );

  // No process switch for two XPI file:// URIs dragged to tab.
  promiseNotification = promiseInstallNotification(aBrowser);
  let promiseNewTab = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );
  effect = EventUtils.synthesizeDrop(
    tab,
    tab,
    [
      [{ type: "text/uri-list", data: fileurl1.spec }],
      [{ type: "text/uri-list", data: fileurl2.spec }],
    ],
    "move"
  );
  is(effect, "move", "Drag should be accepted");
  // When drag'n'dropping two XPIs, one is loaded in the current tab while the
  // other one is loaded in a new tab.
  let { target: newTab } = await promiseNewTab;
  // This is the prompt for the first XPI in the current tab.
  await promiseNotification;

  let promiseSecondNotification = promiseInstallNotification(
    newTab.linkedBrowser
  );

  // We switch to the second tab and wait for the prompt for the second XPI.
  BrowserTestUtils.switchTab(gBrowser, newTab);
  await promiseSecondNotification;

  BrowserTestUtils.removeTab(newTab);

  await CheckBrowserInPid(
    aBrowser,
    browserPid,
    "Check that browser has not switched process."
  );
}

// Test for bug 1175267.
add_task(async function() {
  await BrowserTestUtils.withNewTab(
    "http://example.com",
    testOpenedAndDraggedXPI
  );
  await BrowserTestUtils.withNewTab("about:robots", testOpenedAndDraggedXPI);
});
