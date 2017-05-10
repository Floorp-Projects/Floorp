/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */

const addonInstallId = "addon-install-confirmation";

let fileurl1 = get_addon_file_url("browser_dragdrop1.xpi");
let fileurl2 = get_addon_file_url("browser_dragdrop2.xpi");

function promiseInstallNotification(aBrowser) {
  return new Promise(resolve => {
    function popupshown(event) {
      if (event.target.getAttribute("popupid") != addonInstallId) {
        return;
      }

      let notification =
        PopupNotifications.getNotification(addonInstallId, aBrowser);
      if (!notification) {
        return;
      }

      PopupNotifications.panel.removeEventListener("popupshown", popupshown);
      ok(true, `Got ${addonInstallId} popup for browser`);
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

function* testOpenedAndDraggedXPI(aBrowser) {
  // Get the current pid for browser for comparison later.
  let browserPid = yield ContentTask.spawn(aBrowser, null, () => {
    return Services.appinfo.processID;
  });

  // No process switch for XPI file:// URI in the urlbar.
  let promiseNotification = promiseInstallNotification(aBrowser);
  let urlbar = document.getElementById("urlbar");
  urlbar.value = fileurl1.spec;
  urlbar.focus();
  EventUtils.synthesizeKey("KEY_Enter", { code: "Enter" });
  yield promiseNotification;
  yield CheckBrowserInPid(aBrowser, browserPid,
                          "Check that browser has not switched process.");

  // No process switch for XPI file:// URI dragged to tab.
  let tab = gBrowser.getTabForBrowser(aBrowser);
  promiseNotification = promiseInstallNotification(aBrowser);
  let effect = EventUtils.synthesizeDrop(tab, tab,
               [[{type: "text/uri-list", data: fileurl1.spec}]],
               "move");
  is(effect, "move", "Drag should be accepted");
  yield promiseNotification;
  yield CheckBrowserInPid(aBrowser, browserPid,
                          "Check that browser has not switched process.");

  // No process switch for two XPI file:// URIs dragged to tab.
  promiseNotification = promiseInstallNotification(aBrowser);
  let promiseTabAndNotification = waitForAnyNewTabAndInstallNotification();
  effect = EventUtils.synthesizeDrop(tab, tab,
           [[{type: "text/uri-list", data: fileurl1.spec}],
            [{type: "text/uri-list", data: fileurl2.spec}]],
           "move");
  is(effect, "move", "Drag should be accepted");
  let [newTab, newTabInstallNotification] = yield promiseTabAndNotification;
  yield promiseNotification;
  if (gBrowser.selectedTab != newTab) {
    yield BrowserTestUtils.switchTab(gBrowser, newTab);
  }
  yield newTabInstallNotification;
  yield BrowserTestUtils.removeTab(newTab);
  yield CheckBrowserInPid(aBrowser, browserPid,
                          "Check that browser has not switched process.");
}

// Test for bug 1175267.
add_task(function* () {
  yield SpecialPowers.pushPrefEnv({
    set: [["xpinstall.customConfirmationUI", true]]
  });

  yield BrowserTestUtils.withNewTab("http://example.com", testOpenedAndDraggedXPI);
  yield BrowserTestUtils.withNewTab("about:robots", testOpenedAndDraggedXPI);
});
