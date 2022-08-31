/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { ExtensionPermissions } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPermissions.jsm"
);
const { Management } = ChromeUtils.import(
  "resource://gre/modules/Extension.jsm"
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
  wantDisabled = true,
  expectedAnchorID = "addons-notification-icon",
  win = window
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
      win.PopupNotifications.panel.addEventListener(
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
  ok(win.PopupNotifications.isPanelOpen, "Panel should be open");
  is(
    win.PopupNotifications.panel.childNodes.length,
    aExpectedCount,
    "Should be the right number of notifications"
  );
  if (win.PopupNotifications.panel.childNodes.length) {
    let nodes = Array.from(win.PopupNotifications.panel.childNodes);
    let notification = nodes.find(
      n => n.id == notificationId + "-notification"
    );
    ok(notification, `Should have seen the right notification`);
    is(
      notification.button.hasAttribute("disabled"),
      wantDisabled,
      "The install button should be disabled?"
    );

    let n = win.PopupNotifications.getNotification(PROGRESS_NOTIFICATION);
    is(
      n?.anchorElement?.id || n?.anchorElement?.parentElement?.id,
      expectedAnchorID,
      "expected the right anchor ID"
    );
  }

  return win.PopupNotifications.panel;
}

function acceptAppMenuNotificationWhenShown(
  id,
  extensionId,
  {
    dismiss = false,
    checkIncognito = false,
    incognitoChecked = false,
    incognitoHidden = false,
    global = window,
  } = {}
) {
  const { AppMenuNotifications, PanelUI, document } = global;
  return new Promise(resolve => {
    let permissionChangePromise = null;
    function appMenuPopupHidden() {
      PanelUI.panel.removeEventListener("popuphidden", appMenuPopupHidden);
      ok(
        !PanelUI.menuButton.hasAttribute("badge-status"),
        "badge is not set after addon-installed"
      );
      resolve(permissionChangePromise);
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

      let checkbox = document.getElementById("addon-incognito-checkbox");
      is(checkbox.hidden, incognitoHidden, "checkbox visibility is correct");
      is(checkbox.checked, incognitoChecked, "checkbox is marked as expected");

      // If we're unchecking or checking the incognito property, this will
      // trigger an update in ExtensionPermission, let's wait for it before
      // returning from this promise.
      if (incognitoChecked != checkIncognito) {
        permissionChangePromise = new Promise(resolve => {
          const listener = (type, change) => {
            if (extensionId == change.extensionId) {
              // Let's make sure we received the right message
              let { permissions } = checkIncognito
                ? change.added
                : change.removed;
              ok(permissions.includes("internal:privateBrowsingAllowed"));
              resolve();
            }
          };
          Management.once("change-permissions", listener);
        });
      }

      checkbox.checked = checkIncognito;

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
      resolve(permissionChangePromise);
    }
    PanelUI.notificationPanel.addEventListener("popupshown", popupshown);
  });
}

async function waitForNotification(
  aId,
  aExpectedCount = 1,
  expectedAnchorID = "addons-notification-icon",
  win = window
) {
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
    win.PopupNotifications.panel.addEventListener(
      "PanelUpdated",
      function eventListener(e) {
        // Skip notifications that are not the one that we are supposed to be looking for
        if (!e.detail.includes(aId)) {
          return;
        }
        win.PopupNotifications.panel.removeEventListener(
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
  ok(win.PopupNotifications.isPanelOpen, "Panel should be open");
  is(
    win.PopupNotifications.panel.childNodes.length,
    aExpectedCount,
    "Should be the right number of notifications"
  );
  if (win.PopupNotifications.panel.childNodes.length) {
    let nodes = Array.from(win.PopupNotifications.panel.childNodes);
    let notification = nodes.find(n => n.id == aId + "-notification");
    ok(notification, "Should have seen the " + aId + " notification");

    let n = win.PopupNotifications.getNotification(aId);
    is(
      n?.anchorElement?.id || n?.anchorElement?.parentElement?.id,
      expectedAnchorID,
      "expected the right anchor ID"
    );
  }
  await SimpleTest.promiseFocus(win.PopupNotifications.window);

  return win.PopupNotifications.panel;
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
    SpecialPowers.pushPrefEnv({
      set: [["xpinstall.enabled", false]],
    });
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
      "Software installation is currently disabled. Click Enable and try again.",
      "notification label is correct"
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
    await SpecialPowers.popPrefEnv();
  },

  async function test_blockedInstall() {
    SpecialPowers.pushPrefEnv({
      set: [["extensions.postDownloadThirdPartyPrompt", false]],
    });

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

    notificationPromise = acceptAppMenuNotificationWhenShown(
      "addon-installed",
      "amosigned-xpi@tests.mozilla.org"
    );

    installDialog.button.click();
    await notificationPromise;

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let addon = await AddonManager.getAddonByID(
      "amosigned-xpi@tests.mozilla.org"
    );
    await addon.uninstall();

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await SpecialPowers.popPrefEnv();
  },

  async function test_blockedInstallDomain() {
    SpecialPowers.pushPrefEnv({
      set: [
        ["extensions.postDownloadThirdPartyPrompt", true],
        ["extensions.install_origins.enabled", true],
      ],
    });

    let progressPromise = waitForProgressNotification();
    let notificationPromise = waitForNotification("addon-install-failed");
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: TESTROOT2 + "webmidi_permission.xpi",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );
    await progressPromise;
    let panel = await notificationPromise;

    let notification = panel.childNodes[0];
    is(
      notification.getAttribute("label"),
      "The add-on WebMIDI test addon can not be installed from this location.",
      "Should have seen the right message"
    );

    await removeTabAndWaitForNotificationClose();
    await SpecialPowers.popPrefEnv();
  },

  async function test_allowedInstallDomain() {
    SpecialPowers.pushPrefEnv({
      set: [
        ["extensions.postDownloadThirdPartyPrompt", true],
        ["extensions.install_origins.enabled", true],
      ],
    });

    let notificationPromise = waitForNotification("addon-install-blocked");
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: TESTROOT + "webmidi_permission.xpi",
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

    // Next we get the permissions prompt, which also warns of the unsigned state of the addon
    notificationPromise = waitForNotification("addon-webext-permissions");
    // Click on Allow on the 3rd party panel
    notification.button.click();
    panel = await notificationPromise;
    notification = panel.childNodes[0];

    is(notification.button.label, "Add", "Should have seen the right button");

    is(
      notification.id,
      "addon-webext-permissions-notification",
      "Should have seen the permissions panel"
    );
    let singlePerm = panel.ownerDocument.getElementById(
      "addon-webext-perm-single-entry"
    );
    is(
      singlePerm.textContent,
      "Access MIDI devices",
      "Should have seen the right permission text"
    );

    notificationPromise = acceptAppMenuNotificationWhenShown(
      "addon-installed",
      "webmidi@test.mozilla.org",
      { incognitoHidden: false, checkIncognito: true }
    );

    // Click on Allow on the permissions panel
    notification.button.click();

    await notificationPromise;

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let addon = await AddonManager.getAddonByID("webmidi@test.mozilla.org");
    await TestUtils.topicObserved("webextension-sitepermissions-startup");

    // This addon should have a site permission with private browsing.
    let uri = Services.io.newURI(addon.siteOrigin);
    let pbPrincipal = Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {
        privateBrowsingId: 1,
      }
    );
    let permission = Services.perms.testExactPermissionFromPrincipal(
      pbPrincipal,
      "midi"
    );
    is(
      permission,
      Services.perms.ALLOW_ACTION,
      "api access in private browsing granted"
    );

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
            type: "sitepermission",
          },
        },
      ],
      "Expect telemetry events for privateBrowsingAllowed action",
      { actionType: "privateBrowsingAllowed" }
    );

    await addon.uninstall();

    // Verify the permission has not been retained.
    let { permissions } = await ExtensionPermissions.get(
      "webmidi@test.mozilla.org"
    );
    ok(
      !permissions.includes("internal:privateBrowsingAllowed"),
      "permission is not set after uninstall"
    );

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await SpecialPowers.popPrefEnv();
  },

  async function test_blockedPostDownload() {
    SpecialPowers.pushPrefEnv({
      set: [["extensions.postDownloadThirdPartyPrompt", true]],
    });

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

    let installDialog = await dialogPromise;

    notificationPromise = acceptAppMenuNotificationWhenShown(
      "addon-installed",
      "amosigned-xpi@tests.mozilla.org"
    );

    installDialog.button.click();
    await notificationPromise;

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let addon = await AddonManager.getAddonByID(
      "amosigned-xpi@tests.mozilla.org"
    );
    await addon.uninstall();

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await SpecialPowers.popPrefEnv();
  },

  async function test_recommendedPostDownload() {
    SpecialPowers.pushPrefEnv({
      set: [["extensions.postDownloadThirdPartyPrompt", true]],
    });

    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "recommended.xpi",
      })
    );
    BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );

    let installDialog = await waitForInstallDialog();

    let notificationPromise = acceptAppMenuNotificationWhenShown(
      "addon-installed",
      "{811d77f1-f306-4187-9251-b4ff99bad60b}"
    );

    installDialog.button.click();
    await notificationPromise;

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let addon = await AddonManager.getAddonByID(
      "{811d77f1-f306-4187-9251-b4ff99bad60b}"
    );
    await addon.uninstall();

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await SpecialPowers.popPrefEnv();
  },

  async function test_priviledgedNo3rdPartyPrompt() {
    SpecialPowers.pushPrefEnv({
      set: [["extensions.postDownloadThirdPartyPrompt", true]],
    });
    AddonManager.checkUpdateSecurity = false;
    registerCleanupFunction(() => {
      AddonManager.checkUpdateSecurity = true;
    });

    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "privileged.xpi",
      })
    );

    let installDialogPromise = waitForInstallDialog();

    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );

    let notificationPromise = acceptAppMenuNotificationWhenShown(
      "addon-installed",
      "test@tests.mozilla.org",
      { incognitoHidden: true }
    );

    (await installDialogPromise).button.click();
    await notificationPromise;

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let addon = await AddonManager.getAddonByID("test@tests.mozilla.org");
    await addon.uninstall();

    await BrowserTestUtils.removeTab(tab);
    await SpecialPowers.popPrefEnv();
    AddonManager.checkUpdateSecurity = true;
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

    let installPerm = PermissionTestUtils.testPermission(
      gBrowser.currentURI,
      "install"
    );
    is(
      installPerm,
      Ci.nsIPermissionManager.DENY_ACTION,
      "Addon installation should be blocked for site"
    );

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);

    PermissionTestUtils.remove(target, "install");
  },

  async function test_permaBlockedInstallNoPrompt() {
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: "amosigned.xpi",
      })
    );
    let target = TESTROOT + "installtrigger.html?" + triggers;

    PermissionTestUtils.add(target, "install", Services.perms.DENY_ACTION);
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

    PermissionTestUtils.remove(target, "install");
  },

  async function test_whitelistedInstall() {
    let originalTab = gBrowser.selectedTab;
    let tab;
    gBrowser.selectedTab = originalTab;
    PermissionTestUtils.add(
      "http://example.com/",
      "install",
      Services.perms.ALLOW_ACTION
    );

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
      "amosigned-xpi@tests.mozilla.org",
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

    await addon.uninstall();

    PermissionTestUtils.remove("http://example.com/", "install");

    await removeTabAndWaitForNotificationClose();
  },

  async function test_failedDownload() {
    PermissionTestUtils.add(
      "http://example.com/",
      "install",
      Services.perms.ALLOW_ACTION
    );

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

    PermissionTestUtils.remove("http://example.com/", "install");
    await removeTabAndWaitForNotificationClose();
  },

  async function test_corruptFile() {
    PermissionTestUtils.add(
      "http://example.com/",
      "install",
      Services.perms.ALLOW_ACTION
    );

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

    PermissionTestUtils.remove("http://example.com/", "install");
    await removeTabAndWaitForNotificationClose();
  },

  async function test_incompatible() {
    PermissionTestUtils.add(
      "http://example.com/",
      "install",
      Services.perms.ALLOW_ACTION
    );

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
    let message = `XPI Test could not be installed because it is not compatible with ${brandShortName} ${Services.appinfo.version}.`;
    is(
      notification.getAttribute("label"),
      message,
      "Should have seen the right message"
    );

    PermissionTestUtils.remove("http://example.com/", "install");
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
      "amosigned-xpi@tests.mozilla.org",
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

    await addon.uninstall();

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
    PermissionTestUtils.add(
      "http://example.com/",
      "install",
      Services.perms.ALLOW_ACTION
    );

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

    PermissionTestUtils.remove("http://example.com/", "install");
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  },

  async function test_failedSecurity() {
    SpecialPowers.pushPrefEnv({
      set: [
        [PREF_INSTALL_REQUIREBUILTINCERTS, false],
        ["extensions.postDownloadThirdPartyPrompt", false],
      ],
    });

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

    await removeTabAndWaitForNotificationClose();
    await SpecialPowers.popPrefEnv();
  },

  async function test_incognito_checkbox() {
    // Grant permission up front.
    const permissionName = "internal:privateBrowsingAllowed";
    let incognitoPermission = {
      permissions: [permissionName],
      origins: [],
    };
    await ExtensionPermissions.add(
      "amosigned-xpi@tests.mozilla.org",
      incognitoPermission
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
      "amosigned-xpi@tests.mozilla.org",
      { incognitoChecked: true }
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
    await AddonTestUtils.promiseWebExtensionStartup(
      "amosigned-xpi@tests.mozilla.org"
    );

    // This addon should no longer have private browsing permission.
    let policy = WebExtensionPolicy.getByID(addon.id);
    ok(!policy.privateBrowsingAllowed, "private browsing permission removed");

    // Verify that the expected telemetry event has been collected for the extension allowed on
    // PB windows from the "post install" notification doorhanger.
    assertActionAMTelemetryEvent(
      [
        {
          method: "action",
          object: "doorhanger",
          value: "off",
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

    await addon.uninstall();

    await removeTabAndWaitForNotificationClose();
  },

  async function test_incognito_checkbox_new_window() {
    let win = await BrowserTestUtils.openNewBrowserWindow();
    await SimpleTest.promiseFocus(win);
    // Grant permission up front.
    const permissionName = "internal:privateBrowsingAllowed";
    let incognitoPermission = {
      permissions: [permissionName],
      origins: [],
    };
    await ExtensionPermissions.add(
      "amosigned-xpi@tests.mozilla.org",
      incognitoPermission
    );

    let panelEventPromise = new Promise(resolve => {
      win.PopupNotifications.panel.addEventListener(
        "PanelUpdated",
        function eventListener(e) {
          if (e.detail.includes("addon-webext-permissions")) {
            win.PopupNotifications.panel.removeEventListener(
              "PanelUpdated",
              eventListener
            );
            resolve();
          }
        }
      );
    });

    win.gBrowser.selectedTab = BrowserTestUtils.addTab(
      win.gBrowser,
      "about:blank"
    );
    await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    win.gURLBar.value = TESTROOT + "amosigned.xpi";
    win.gURLBar.focus();
    EventUtils.synthesizeKey("KEY_Enter", {}, win);

    await panelEventPromise;
    await waitForTick();

    let panel = win.PopupNotifications.panel;
    let installDialog = panel.childNodes[0];

    let notificationPromise = acceptAppMenuNotificationWhenShown(
      "addon-installed",
      "amosigned-xpi@tests.mozilla.org",
      { incognitoChecked: true, global: win }
    );
    acceptInstallDialog(installDialog);
    await notificationPromise;

    let installs = await AddonManager.getAllInstalls();
    is(installs.length, 0, "Should be no pending installs");

    let addon = await AddonManager.getAddonByID(
      "amosigned-xpi@tests.mozilla.org"
    );
    // The panel is reloading the addon due to the permission change, we need some way
    // to wait for the reload to finish. addon.startupPromise doesn't do it for
    // us, so we'll just restart again.
    await AddonTestUtils.promiseWebExtensionStartup(
      "amosigned-xpi@tests.mozilla.org"
    );

    // This addon should no longer have private browsing permission.
    let policy = WebExtensionPolicy.getByID(addon.id);
    ok(!policy.privateBrowsingAllowed, "private browsing permission removed");

    // Verify that the expected telemetry event has been collected for the extension allowed on
    // PB windows from the "post install" notification doorhanger.
    assertActionAMTelemetryEvent(
      [
        {
          method: "action",
          object: "doorhanger",
          value: "off",
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

    await addon.uninstall();

    await BrowserTestUtils.closeWindow(win);
  },

  async function test_blockedInstallDomain_with_unified_extensions() {
    SpecialPowers.pushPrefEnv({
      set: [["extensions.unifiedExtensions.enabled", true]],
    });

    let win = await BrowserTestUtils.openNewBrowserWindow();
    await SimpleTest.promiseFocus(win);
    await new Promise(resolve => {
      win.requestIdleCallback(resolve);
    });
    await TestUtils.waitForCondition(
      () => win.gUnifiedExtensions._initialized,
      "Wait gUnifiedExtensions to have been initialized"
    );

    let progressPromise = waitForProgressNotification(
      false,
      1,
      true,
      "unified-extensions-button",
      win
    );
    let notificationPromise = waitForNotification(
      "addon-install-failed",
      1,
      "unified-extensions-button",
      win
    );
    let triggers = encodeURIComponent(
      JSON.stringify({
        XPI: TESTROOT2 + "webmidi_permission.xpi",
      })
    );
    await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      TESTROOT + "installtrigger.html?" + triggers
    );
    await progressPromise;
    await notificationPromise;

    await BrowserTestUtils.closeWindow(win);
    await SpecialPowers.popPrefEnv();
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

  SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.logging.enabled", true],
      ["extensions.strictCompatibility", true],
      ["extensions.install.requireSecureOrigin", false],
      ["security.dialog_enable_delay", 0],
      // These tests currently depends on InstallTrigger.install.
      ["extensions.InstallTrigger.enabled", true],
      ["extensions.InstallTriggerImpl.enabled", true],
      // Relax the user input requirements while running this test.
      ["xpinstall.userActivation.required", false],
    ],
  });

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
