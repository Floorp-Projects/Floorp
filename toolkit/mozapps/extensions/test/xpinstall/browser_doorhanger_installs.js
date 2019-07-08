/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const SECUREROOT =
  "https://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/";
const PROGRESS_NOTIFICATION = "addon-progress";

const CHROMEROOT = extractChromeRoot(gTestPath);

AddonTestUtils.initMochitest(this);
AddonTestUtils.hookAMTelemetryEvents();

// Assert on the expected "addonsManager.action" telemetry events (and optional filter events to verify
// by using a given actionType).
function assertActionAMTelemetryEvent(
  expectedActionEvents,
  assertMessage,
  { actionType } = {}
) {
  const events = AddonTestUtils.getAMTelemetryEvents().filter(
    ({ method, extra }) => {
      return (
        method === "action" &&
        (!actionType ? true : extra && extra.action === actionType)
      );
    }
  );

  Assert.deepEqual(events, expectedActionEvents, assertMessage);
}

function waitForTick() {
  return new Promise(resolve => executeSoon(resolve));
}

function getObserverTopic(aNotificationId) {
  let topic = aNotificationId;
  if (topic == "xpinstall-disabled") {
    topic = "addon-install-disabled";
  } else if (topic == "addon-progress") {
    topic = "addon-install-started";
  } else if (topic == "addon-install-restart") {
    topic = "addon-install-complete";
  } else if (topic == "addon-installed") {
    topic = "webextension-install-notify";
  }
  return topic;
}

async function waitForProgressNotification(
  aPanelOpen = false,
  aExpectedCount = 1,
  wantDisabled = true
) {
  let notificationId = PROGRESS_NOTIFICATION;
  info("Waiting for " + notificationId + " notification");

  let topic = getObserverTopic(notificationId);

  let observerPromise = new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      // Ignore the progress notification unless that is the notification we want
      if (
        notificationId != PROGRESS_NOTIFICATION &&
        aTopic == getObserverTopic(PROGRESS_NOTIFICATION)
      ) {
        return;
      }
      Services.obs.removeObserver(observer, topic);
      resolve();
    }, topic);
  });

  let panelEventPromise;
  if (aPanelOpen) {
    panelEventPromise = Promise.resolve();
  } else {
    panelEventPromise = new Promise(resolve => {
      PopupNotifications.panel.addEventListener(
        "popupshowing",
        function() {
          resolve();
        },
        { once: true }
      );
    });
  }

  await observerPromise;
  await panelEventPromise;
  await waitForTick();

  info("Saw a notification");
  ok(PopupNotifications.isPanelOpen, "Panel should be open");
  is(
    PopupNotifications.panel.childNodes.length,
    aExpectedCount,
    "Should be the right number of notifications"
  );
  if (PopupNotifications.panel.childNodes.length) {
    let nodes = Array.from(PopupNotifications.panel.childNodes);
    let notification = nodes.find(
      n => n.id == notificationId + "-notification"
    );
    ok(notification, `Should have seen the right notification`);
    is(
      notification.button.hasAttribute("disabled"),
      wantDisabled,
      "The install button should be disabled?"
    );
  }

  return PopupNotifications.panel;
}

function acceptAppMenuNotificationWhenShown(
  id,
  { dismiss = false, checkIncognito = false } = {}
) {
  const { AppMenuNotifications } = ChromeUtils.import(
    "resource://gre/modules/AppMenuNotifications.jsm"
  );
  return new Promise(resolve => {
    function appMenuPopupHidden() {
      PanelUI.panel.removeEventListener("popuphidden", appMenuPopupHidden);
      is(
        PanelUI.menuButton.getAttribute("badge-status"),
        false,
        "badge is not set after addon-installed"
      );
      resolve();
    }
    function appMenuPopupShown() {
      PanelUI.panel.removeEventListener("popupshown", appMenuPopupShown);
      PanelUI.menuButton.click();
    }
    function popupshown() {
      let notification = AppMenuNotifications.activeNotification;
      if (!notification) {
        return;
      }

      is(notification.id, id, `${id} notification shown`);
      ok(PanelUI.isNotificationPanelOpen, "notification panel open");

      PanelUI.notificationPanel.removeEventListener("popupshown", popupshown);

      let allowPrivate = Services.prefs.getBoolPref(
        "extensions.allowPrivateBrowsingByDefault",
        true
      );
      let checkbox = document.getElementById("addon-incognito-checkbox");
      is(
        checkbox.hidden,
        allowPrivate && !checkIncognito,
        "checkbox visibility is correct"
      );
      is(checkbox.checked, false, "checkbox is not initially checked");
      if (checkIncognito) {
        checkbox.checked = true;
      }

      if (dismiss) {
        // Dismiss the panel by clicking on the appMenu button.
        PanelUI.panel.addEventListener("popupshown", appMenuPopupShown);
        PanelUI.panel.addEventListener("popuphidden", appMenuPopupHidden);
        PanelUI.menuButton.click();
        return;
      }

      // Dismiss the panel by clicking the primary button.
      let popupnotificationID = PanelUI._getPopupId(notification);
      let popupnotification = document.getElementById(popupnotificationID);

      popupnotification.button.click();
      resolve();
    }
    PanelUI.notificationPanel.addEventListener("popupshown", popupshown);
  });
}

async function waitForNotification(aId, aExpectedCount = 1) {
  info("Waiting for " + aId + " notification");

  let topic = getObserverTopic(aId);

  let observerPromise;
  if (aId !== "addon-webext-permissions") {
    observerPromise = new Promise(resolve => {
      Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
        // Ignore the progress notification unless that is the notification we want
        if (
          aId != PROGRESS_NOTIFICATION &&
          aTopic == getObserverTopic(PROGRESS_NOTIFICATION)
        ) {
          return;
        }
        Services.obs.removeObserver(observer, topic);
        resolve();
      }, topic);
    });
  }

  let panelEventPromise = new Promise(resolve => {
    PopupNotifications.panel.addEventListener(
      "PanelUpdated",
      function eventListener(e) {
        // Skip notifications that are not the one that we are supposed to be looking for
        if (!e.detail.includes(aId)) {
          return;
        }
        PopupNotifications.panel.removeEventListener(
          "PanelUpdated",
          eventListener
        );
        resolve();
      }
    );
  });

  await observerPromise;
  await panelEventPromise;
  await waitForTick();

  info("Saw a " + aId + " notification");
  ok(PopupNotifications.isPanelOpen, "Panel should be open");
  is(
    PopupNotifications.panel.childNodes.length,
    aExpectedCount,
    "Should be the right number of notifications"
  );
  if (PopupNotifications.panel.childNodes.length) {
    let nodes = Array.from(PopupNotifications.panel.childNodes);
    let notification = nodes.find(n => n.id == aId + "-notification");
    ok(notification, "Should have seen the " + aId + " notification");
  }

  return PopupNotifications.panel;
}

function waitForNotificationClose() {
  if (!PopupNotifications.isPanelOpen) {
    return Promise.resolve();
  }
  return new Promise(resolve => {
    info("Waiting for notification to close");
    PopupNotifications.panel.addEventListener(
      "popuphidden",
      function() {
        resolve();
      },
      { once: true }
    );
  });
}

async function waitForInstallDialog(id = "addon-webext-permissions") {
  let panel = await waitForNotification(id);
  return panel.childNodes[0];
}

function removeTabAndWaitForNotificationClose() {
  let closePromise = waitForNotificationClose();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  return closePromise;
}

function acceptInstallDialog(installDialog) {
  installDialog.button.click();
}

async function waitForSingleNotification(aCallback) {
  while (PopupNotifications.panel.childNodes.length != 1) {
    await new Promise(resolve => executeSoon(resolve));

    info("Waiting for single notification");
    // Notification should never close while we wait
    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
  }
}

function setupRedirect(aSettings) {
  var url =
    "https://example.com/browser/toolkit/mozapps/extensions/test/xpinstall/redirect.sjs?mode=setup";
  for (var name in aSettings) {
    url += "&" + name + "=" + aSettings[name];
  }

  var req = new XMLHttpRequest();
  req.open("GET", url, false);
  req.send(null);
}

var TESTS = [
  async function test_disabledInstall() {
    Services.prefs.setBoolPref("xpinstall.enabled", false);

    let notificationPromise = waitForNotification("xpinstall-disabled");
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "amosigned.xpi",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );
    let panel = await notificationPromise;

    let notification = panel.childNodes[0];
    is(
      notification.button.label,
      "Enable",
      "Should have seen the right button"
    );
    is(
      notification.getAttribute("label"),
      "Software installation is currently disabled. Click Enable and try again."
    );

    let closePromise = waitForNotificationClose();
    // Click on Enable
    EventUtils.synthesizeMouseAtCenter(notification.button, {});
    await closePromise;

    try {
      ok(
        Services.prefs.getBoolPref("xpinstall.enabled"),
        "Installation should be enabled"
      );
    } catch (e) {
      ok(false, "xpinstall.enabled should be set");
    }

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Shouldn't be any pending installs");
  },

  async function test_blockedInstall() {
    let notificationPromise = waitForNotification("addon-install-blocked");
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "amosigned.xpi",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );
    let panel = await notificationPromise;

    let notification = panel.childNodes[0];
    is(
      notification.button.label,
      "Continue to Installation",
      "Should have seen the right button"
    );
    let message = panel.ownerDocument.getElementById(
      "addon-install-blocked-message"
    );
    is(
      message.textContent,
      "You are attempting to install an add-on from example.com. Make sure you trust this site before continuing.",
      "Should have seen the right message"
    );

    let dialogPromise = waitForInstallDialog();
    // Click on Allow
    EventUtils.synthesizeMouse(notification.button, 20, 10, {});
    // Notification should have changed to progress notification
    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    notification = panel.childNodes[0];
    is(
      notification.id,
      "addon-progress-notification",
      "Should have seen the progress notification"
    );

    let installDialog = await dialogPromise;

    notificationPromise = acceptAppMenuNotificationWhenShown("addon-installed");
    installDialog.button.click();
    await notificationPromise;

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let addon = await AddonManager.getAddonByID(
      "amosigned-xpi@tests.mozilla.org"
    );
    addon.uninstall();

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  },

  async function test_permaBlockInstall() {
    let notificationPromise = waitForNotification("addon-install-blocked");
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "amosigned.xpi",
      })
    );
    let target = TESTROOT + "installtrigger.html?" + triggers;

    BrowserTestUtils.openNewForegroundTab(gBrowser, target);
    let notification = (await notificationPromise).firstElementChild;
    let neverAllowBtn = notification.menupopup.firstElementChild;

    neverAllowBtn.click();

    await TestUtils.waitForCondition(
      () => !PopupNotifications.isPanelOpen,
      "Waiting for notification to close"
    );

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let installPerm = Services.perms.testPermission(
      gBrowser.currentURI,
      "install"
    );
    is(
      installPerm,
      Ci.nsIPermissionManager.DENY_ACTION,
      "Addon installation should be blocked for site"
    );

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);

    SitePermissions.remove(NetUtil.newURI(target), "install");
  },

  async function test_permaBlockedInstallNoPrompt() {
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "amosigned.xpi",
      })
    );
    let target = TESTROOT + "installtrigger.html?" + triggers;

    SitePermissions.set(
      NetUtil.newURI(target),
      "install",
      SitePermissions.BLOCK
    );
    await BrowserTestUtils.openNewForegroundTab(gBrowser, target);

    let panelOpened;
    try {
      panelOpened = await TestUtils.waitForCondition(
        () => PopupNotifications.isPanelOpen,
        100,
        10
      );
    } catch (ex) {
      panelOpened = false;
    }
    is(panelOpened, false, "Addon prompt should not open");

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);

    SitePermissions.remove(NetUtil.newURI(target), "install");
  },

  async function test_whitelistedInstall() {
    Services.prefs.setBoolPref(
      "extensions.allowPrivateBrowsingByDefault",
      false
    );
    let originalTab = gBrowser.selectedTab;
    let tab;
    gBrowser.selectedTab = originalTab;
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "amosigned.xpi",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    ).then(newTab => (tab = newTab));
    await progressPromise;
    let installDialog = await dialogPromise;
    await BrowserTestUtils.waitForCondition(
      () => !!tab,
      "tab should be present"
    );

    is(
      gBrowser.selectedTab,
      tab,
      "tab selected in response to the addon-install-confirmation notification"
    );

    let notificationPromise = acceptAppMenuNotificationWhenShown(
      "addon-installed",
      { dismiss: true }
    );
    acceptInstallDialog(installDialog);
    await notificationPromise;

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let addon = await AddonManager.getAddonByID(
      "amosigned-xpi@tests.mozilla.org"
    );

    // Test that the addon does not have permission. Reload it to ensure it would
    // have been set if possible.
    await addon.reload();
    let policy = WebExtensionPolicy.getByID(addon.id);
    ok(
      !policy.privateBrowsingAllowed,
      "private browsing permission was not granted"
    );

    addon.uninstall();

    Services.perms.remove(makeURI("http://example.com/"), "install");

    Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");
    await removeTabAndWaitForNotificationClose();
  },

  async function test_failedDownload() {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let failPromise = waitForNotification("addon-install-failed");
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "missing.xpi",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );
    await progressPromise;
    let panel = await failPromise;

    let notification = panel.childNodes[0];
    is(
      notification.getAttribute("label"),
      "The add-on could not be downloaded because of a connection failure.",
      "Should have seen the right message"
    );

    Services.perms.remove(makeURI("http://example.com/"), "install");
    await removeTabAndWaitForNotificationClose();
  },

  async function test_corruptFile() {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let failPromise = waitForNotification("addon-install-failed");
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "corrupt.xpi",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );
    await progressPromise;
    let panel = await failPromise;

    let notification = panel.childNodes[0];
    is(
      notification.getAttribute("label"),
      "The add-on downloaded from this site could not be installed " +
        "because it appears to be corrupt.",
      "Should have seen the right message"
    );

    Services.perms.remove(makeURI("http://example.com/"), "install");
    await removeTabAndWaitForNotificationClose();
  },

  async function test_incompatible() {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let progressPromise = waitForProgressNotification();
    let failPromise = waitForNotification("addon-install-failed");
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "incompatible.xpi",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );
    await progressPromise;
    let panel = await failPromise;

    let notification = panel.childNodes[0];
    let brandBundle = Services.strings.createBundle(
      "chrome://branding/locale/brand.properties"
    );
    let brandShortName = brandBundle.GetStringFromName("brandShortName");
    let message = `XPI Test could not be installed because it is not compatible with ${brandShortName} ${
      Services.appinfo.version
    }.`;
    is(
      notification.getAttribute("label"),
      message,
      "Should have seen the right message"
    );

    Services.perms.remove(makeURI("http://example.com/"), "install");
    await removeTabAndWaitForNotificationClose();
  },

  async function test_localFile() {
    let cr = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(
      Ci.nsIChromeRegistry
    );
    let path;
    try {
      path = cr.convertChromeURL(makeURI(CHROMEROOT + "corrupt.xpi")).spec;
    } catch (ex) {
      path = CHROMEROOT + "corrupt.xpi";
    }

    let failPromise = new Promise(resolve => {
      Services.obs.addObserver(function observer() {
        Services.obs.removeObserver(observer, "addon-install-failed");
        resolve();
      }, "addon-install-failed");
    });
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    BrowserTestUtils.loadURI(gBrowser, path);
    await failPromise;

    // Wait for the browser code to add the failure notification
    await waitForSingleNotification();

    let notification = PopupNotifications.panel.childNodes[0];
    is(
      notification.id,
      "addon-install-failed-notification",
      "Should have seen the install fail"
    );
    is(
      notification.getAttribute("label"),
      "This add-on could not be installed because it appears to be corrupt.",
      "Should have seen the right message"
    );

    await removeTabAndWaitForNotificationClose();
  },

  async function test_urlBar() {
    Services.prefs.setBoolPref(
      "extensions.allowPrivateBrowsingByDefault",
      false
    );
    let progressPromise = waitForProgressNotification();
    let dialogPromise = waitForInstallDialog();

    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    gURLBar.value = TESTROOT + "amosigned.xpi";
    gURLBar.focus();
    EventUtils.synthesizeKey("KEY_Enter");

    await progressPromise;
    let installDialog = await dialogPromise;

    let notificationPromise = acceptAppMenuNotificationWhenShown(
      "addon-installed",
      { checkIncognito: true }
    );
    installDialog.button.click();
    await notificationPromise;

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let addon = await AddonManager.getAddonByID(
      "amosigned-xpi@tests.mozilla.org"
    );
    // The panel is reloading the addon due to the permission change, we need some way
    // to wait for the reload to finish. addon.startupPromise doesn't do it for
    // us, so we'll just restart again.
    await addon.reload();

    // This addon should have private browsing permission.
    let policy = WebExtensionPolicy.getByID(addon.id);
    ok(policy.privateBrowsingAllowed, "private browsing permission granted");

    // Verify that the expected telemetry event has been collected for the extension allowed on
    // PB windows from the "post install" notification doorhanger.
    assertActionAMTelemetryEvent(
      [
        {
          method: "action",
          object: "doorhanger",
          value: "on",
          extra: {
            action: "privateBrowsingAllowed",
            view: "postInstall",
            addonId: addon.id,
            type: "extension",
          },
        },
      ],
      "Expect telemetry events for privateBrowsingAllowed action",
      { actionType: "privateBrowsingAllowed" }
    );

    addon.uninstall();

    Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");

    await removeTabAndWaitForNotificationClose();
  },

  async function test_wrongHost() {
    let requestedUrl = TESTROOT2 + "enabled.html";
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      requestedUrl
    );
    BrowserTestUtils.loadURI(gBrowser, TESTROOT2 + "enabled.html");
    await loadedPromise;

    let progressPromise = waitForProgressNotification();
    let notificationPromise = waitForNotification("addon-install-failed");
    BrowserTestUtils.loadURI(gBrowser, TESTROOT + "corrupt.xpi");
    await progressPromise;
    let panel = await notificationPromise;

    let notification = panel.childNodes[0];
    is(
      notification.getAttribute("label"),
      "The add-on downloaded from this site could not be installed " +
        "because it appears to be corrupt.",
      "Should have seen the right message"
    );

    await removeTabAndWaitForNotificationClose();
  },

  async function test_renotifyBlocked() {
    let notificationPromise = waitForNotification("addon-install-blocked");
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "amosigned.xpi",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );
    let panel = await notificationPromise;

    let closePromise = waitForNotificationClose();
    // hide the panel (this simulates the user dismissing it)
    panel.hidePopup();
    await closePromise;

    info("Timeouts after this probably mean bug 589954 regressed");

    await new Promise(resolve => executeSoon(resolve));

    notificationPromise = waitForNotification("addon-install-blocked");
    BrowserTestUtils.loadURI(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );
    await notificationPromise;

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 2, "Should be two pending installs");

    await removeTabAndWaitForNotificationClose(gBrowser.selectedTab);

    installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should have cancelled the installs");
  },

  async function test_cancel() {
    let pm = Services.perms;
    pm.add(makeURI("http://example.com/"), "install", pm.ALLOW_ACTION);

    let notificationPromise = waitForNotification(PROGRESS_NOTIFICATION);
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "slowinstall.sjs?file=amosigned.xpi",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );
    let panel = await notificationPromise;

    let notification = panel.childNodes[0];
    // Close the notification
    let anchor = document.getElementById("addons-notification-icon");
    anchor.click();
    // Reopen the notification
    anchor.click();

    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    is(
      PopupNotifications.panel.childNodes.length,
      1,
      "Should be only one notification"
    );
    notification = panel.childNodes[0];
    is(
      notification.id,
      "addon-progress-notification",
      "Should have seen the progress notification"
    );

    // Cancel the download
    let install = notification.notification.options.installs[0];
    let cancelledPromise = new Promise(resolve => {
      install.addListener({
        onDownloadCancelled() {
          install.removeListener(this);
          resolve();
        },
      });
    });
    EventUtils.synthesizeMouseAtCenter(notification.secondaryButton, {});
    await cancelledPromise;

    await new Promise(resolve => executeSoon(resolve));

    ok(!PopupNotifications.isPanelOpen, "Notification should be closed");

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending install");

    Services.perms.remove(makeURI("http://example.com/"), "install");
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  },

  async function test_failedSecurity() {
    Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, false);
    setupRedirect({
      Location: TESTROOT + "amosigned.xpi",
    });

    let notificationPromise = waitForNotification("addon-install-blocked");
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "redirect.sjs?mode=redirect",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      SECUREROOT + "installtrigger.html?" + triggers
    );
    let panel = await notificationPromise;

    let notification = panel.childNodes[0];
    // Click on Allow
    EventUtils.synthesizeMouse(notification.button, 20, 10, {});

    // Notification should have changed to progress notification
    ok(PopupNotifications.isPanelOpen, "Notification should still be open");
    is(
      PopupNotifications.panel.childNodes.length,
      1,
      "Should be only one notification"
    );
    notification = panel.childNodes[0];
    is(
      notification.id,
      "addon-progress-notification",
      "Should have seen the progress notification"
    );

    // Wait for it to fail
    await new Promise(resolve => {
      Services.obs.addObserver(function observer() {
        Services.obs.removeObserver(observer, "addon-install-failed");
        resolve();
      }, "addon-install-failed");
    });

    // Allow the browser code to add the failure notification and then wait
    // for the progress notification to dismiss itself
    await waitForSingleNotification();
    is(
      PopupNotifications.panel.childNodes.length,
      1,
      "Should be only one notification"
    );
    notification = panel.childNodes[0];
    is(
      notification.id,
      "addon-install-failed-notification",
      "Should have seen the install fail"
    );

    Services.prefs.setBoolPref(PREF_INSTALL_REQUIREBUILTINCERTS, true);
    await removeTabAndWaitForNotificationClose();
  },
];

var gTestStart = null;

var XPInstallObserver = {
  observe(aSubject, aTopic, aData) {
    var installInfo = aSubject.wrappedJSObject;
    info(
      "Observed " + aTopic + " for " + installInfo.installs.length + " installs"
    );
    installInfo.installs.forEach(function(aInstall) {
      info(
        "Install of " +
          aInstall.sourceURI.spec +
          " was in state " +
          aInstall.state
      );
    });
  },
};

add_task(async function() {
  requestLongerTimeout(4);

  Services.prefs.setBoolPref("extensions.logging.enabled", true);
  Services.prefs.setBoolPref("extensions.strictCompatibility", true);
  Services.prefs.setBoolPref("extensions.install.requireSecureOrigin", false);
  Services.prefs.setIntPref("security.dialog_enable_delay", 0);

  Services.obs.addObserver(XPInstallObserver, "addon-install-started");
  Services.obs.addObserver(XPInstallObserver, "addon-install-blocked");
  Services.obs.addObserver(XPInstallObserver, "addon-install-failed");

  registerCleanupFunction(async function() {
    // Make sure no more test parts run in case we were timed out
    TESTS = [];

    let aInstalls = await AddonManager.getAllInstalls();
    aInstalls.forEach(function(aInstall) {
      aInstall.cancel();
    });

    Services.prefs.clearUserPref("extensions.logging.enabled");
    Services.prefs.clearUserPref("extensions.strictCompatibility");
    Services.prefs.clearUserPref("extensions.install.requireSecureOrigin");
    Services.prefs.clearUserPref("security.dialog_enable_delay");
    Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");

    Services.obs.removeObserver(XPInstallObserver, "addon-install-started");
    Services.obs.removeObserver(XPInstallObserver, "addon-install-blocked");
    Services.obs.removeObserver(XPInstallObserver, "addon-install-failed");
  });

  for (let i = 0; i < TESTS.length; ++i) {
    if (gTestStart) {
      info("Test part took " + (Date.now() - gTestStart) + "ms");
    }

    ok(!PopupNotifications.isPanelOpen, "Notification should be closed");

    let installs = await AddonManager.getAllInstalls();

    is(installs.length, 0, "Should be no active installs");
    info("Running " + TESTS[i].name);
    gTestStart = Date.now();
    await TESTS[i]();

    // Check that no unexpected telemetry events for the privateBrowsingAllowed action has been
    // collected while running the test case.
    assertActionAMTelemetryEvent(
      [],
      "Expect no telemetry events for privateBrowsingAllowed actions",
      { actionType: "privateBrowsingAllowed" }
    );
  }
});
