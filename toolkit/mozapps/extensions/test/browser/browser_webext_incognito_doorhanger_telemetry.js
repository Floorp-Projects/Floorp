/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

const privateBrowsingNotificationPref =
  "extensions.privatebrowsing.notification";

function promisePreferenceChanged(pref) {
  return new Promise(resolve => {
    let listener = () => {
      Services.prefs.removeObserver(pref, listener);
      resolve();
    };

    Services.prefs.addObserver(pref, listener);
  });
}

function assertDoorhangerTelemetry(action) {
  TelemetryTestUtils.assertEvents(
    [
      [
        "addonsManager",
        "action",
        "doorhanger",
        null,
        { action, view: "privateBrowsing" },
      ],
    ],
    {
      category: "addonsManager",
      method: "action",
    }
  );
}

async function checkDoorhanger(win) {
  await BrowserTestUtils.waitForCondition(
    () => win.AppMenuNotifications.activeNotification,
    "Wait for the AppMenuNotification to be active"
  );

  is(
    win.AppMenuNotifications.activeNotification.id,
    "addon-private-browsing",
    "Got the expected AppMenuNotification as active"
  );

  await BrowserTestUtils.waitForCondition(
    () => win.PanelUI.notificationPanel.state === "open",
    "Wait doorhanger panel to be open"
  );

  let notifications = [...win.PanelUI.notificationPanel.children].filter(
    n => !n.hidden
  );
  is(
    notifications.length,
    1,
    "Only one notification is expected to be visible"
  );

  let doorhanger = notifications[0];
  is(
    doorhanger.id,
    "appMenu-addon-private-browsing-notification",
    "Got the addon-private-browsing doorhanger"
  );

  return doorhanger;
}

add_task(async function test_privateBrowsing_doorhanger_manage() {
  SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.allowPrivateBrowsingByDefault", false],
      [privateBrowsingNotificationPref, false],
      ["extensions.screenshots.disabled", true],
    ],
  });

  Services.telemetry.clearEvents();

  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let doorhanger = await checkDoorhanger(privateWindow);

  let onPrefChanged = promisePreferenceChanged(privateBrowsingNotificationPref);

  // Wait the about:addons tab to be fully loaded (otherwise shutdown leak are going
  // to make the test to fail in debug builds).
  const onceAboutAddonsLoaded = new Promise(resolve => {
    Services.obs.addObserver(function observer(subject, topic) {
      Services.obs.removeObserver(observer, topic);
      resolve();
    }, "EM-loaded");
  });

  // Click the primary action, which is expected to open about:addons.
  doorhanger.button.click();

  await Promise.all([onPrefChanged, onceAboutAddonsLoaded]);

  is(
    PanelUI.notificationPanel.state,
    "closed",
    "Expect the doorhanger to be closed"
  );
  is(
    Services.prefs.getBoolPref(privateBrowsingNotificationPref),
    true,
    "The expected preference has been set to true when the notification is dismissed"
  );

  assertDoorhangerTelemetry("manage");

  await BrowserTestUtils.closeWindow(privateWindow);
});

add_task(async function test_privateBrowsing_doorhanger_dismissed() {
  SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.allowPrivateBrowsingByDefault", false],
      [privateBrowsingNotificationPref, false],
    ],
  });

  Services.telemetry.clearEvents();

  let privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let doorhanger = await checkDoorhanger(privateWindow);

  let onPrefChanged = promisePreferenceChanged(privateBrowsingNotificationPref);

  // Click the secondary action, which is expected to just dismiss the notification.
  doorhanger.secondaryButton.click();

  await onPrefChanged;

  is(
    PanelUI.notificationPanel.state,
    "closed",
    "Expect the doorhanger to be closed"
  );
  is(
    Services.prefs.getBoolPref(privateBrowsingNotificationPref),
    true,
    "The expected preference has been set to true when the notification is dismissed"
  );

  assertDoorhangerTelemetry("dismiss");

  await BrowserTestUtils.closeWindow(privateWindow);
});
