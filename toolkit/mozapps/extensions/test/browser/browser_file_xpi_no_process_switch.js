/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const ADDON_INSTALL_ID = "addon-install-confirmation";

let fileurl1 = get_addon_file_url("browser_dragdrop1.xpi");
let fileurl2 = get_addon_file_url("browser_dragdrop2.xpi");

function promiseInstallNotification(aBrowser) {
  return new Promise(resolve => {
    function popupshown(event) {
      if (event.target.getAttribute("popupid") != ADDON_INSTALL_ID) {
        return;
      }

      let notification =
        PopupNotifications.getNotification(ADDON_INSTALL_ID, aBrowser);
      if (!notification) {
        return;
      }

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      ok(true, `Got ${ADDON_INSTALL_ID} popup for browser`);
      notification.remove();
      resolve();
    }

    PopupNotifications.panel.addEventListener("popupshown", popupshown);
  });
}

function waitForAnyNewTabAndInstallNotification() {
  return new Promise((resolve) => {
    gBrowser.tabContainer.addEventListener("TabOpen", function(openEvent) {
      let newTab = openEvent.target;
      resolve([newTab, promiseInstallNotification(newTab.linkedBrowser)]);
    }, {once: true});
  });
}

function CheckBrowserInPid(browser, expectedPid, message) {
  return ContentTask.spawn(browser, { expectedPid, message }, (arg) => {
    is(Services.appinfo.processID, arg.expectedPid, arg.message);
  });
}

async function testOpenedAndDraggedXPI(aBrowser) {
  // Get the current pid for browser for comparison later.
  let browserPid = await ContentTask.spawn(aBrowser, null, () => {
    return Services.appinfo.processID;
  });

  // No process switch for XPI file:// URI in the urlbar.
  let promiseNotification = promiseInstallNotification(aBrowser);
  let urlbar = document.getElementById("urlbar");
  urlbar.value = fileurl1.spec;
  urlbar.focus();
  EventUtils.synthesizeKey("KEY_Enter", { code: "Enter" });
  await promiseNotification;
  await CheckBrowserInPid(aBrowser, browserPid,
                          "Check that browser has not switched process.");

  // No process switch for XPI file:// URI dragged to tab.
  let tab = gBrowser.getTabForBrowser(aBrowser);
  promiseNotification = promiseInstallNotification(aBrowser);
  let effect = EventUtils.synthesizeDrop(tab, tab,
               [[{type: "text/uri-list", data: fileurl1.spec}]],
               "move");
  is(effect, "move", "Drag should be accepted");
  await promiseNotification;
  await CheckBrowserInPid(aBrowser, browserPid,
                          "Check that browser has not switched process.");

  // No process switch for two XPI file:// URIs dragged to tab.
  promiseNotification = promiseInstallNotification(aBrowser);
  let promiseTabAndNotification = waitForAnyNewTabAndInstallNotification();
  effect = EventUtils.synthesizeDrop(tab, tab,
           [[{type: "text/uri-list", data: fileurl1.spec}],
            [{type: "text/uri-list", data: fileurl2.spec}]],
           "move");
  is(effect, "move", "Drag should be accepted");
  let [newTab, newTabInstallNotification] = await promiseTabAndNotification;
  await promiseNotification;
  if (gBrowser.selectedTab != newTab) {
    await BrowserTestUtils.switchTab(gBrowser, newTab);
  }
  await newTabInstallNotification;
  await BrowserTestUtils.removeTab(newTab);
  await CheckBrowserInPid(aBrowser, browserPid,
                          "Check that browser has not switched process.");
}

// Test for bug 1175267.
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["xpinstall.customConfirmationUI", true]]
  });

  await BrowserTestUtils.withNewTab("http://example.com", testOpenedAndDraggedXPI);
  await BrowserTestUtils.withNewTab("about:robots", testOpenedAndDraggedXPI);
});
