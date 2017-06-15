add_task(async function testUpdatesBackgroundWindow() {
  SpecialPowers.pushPrefEnv({set: [
    [PREF_APP_UPDATE_STAGING_ENABLED, false],
    [PREF_APP_UPDATE_AUTO, false]
  ]});

  let updateParams = "promptWaitTime=0";
  let extraWindow = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(extraWindow);

  await runUpdateTest(updateParams, 1, [
    async function() {
      await BrowserTestUtils.waitForCondition(() => PanelUI.menuButton.hasAttribute("badge-status"),
                                              "Background window has a badge.");
      is(PanelUI.notificationPanel.state, "closed",
         "The doorhanger is not showing for the background window");
      is(PanelUI.menuButton.getAttribute("badge-status"), "update-available",
         "The badge is showing for the background window");
      let popupShownPromise = BrowserTestUtils.waitForEvent(PanelUI.notificationPanel, "popupshown");
      await BrowserTestUtils.closeWindow(extraWindow);
      await SimpleTest.promiseFocus(window);
      await popupShownPromise;

      checkWhatsNewLink("update-available-whats-new");
      let buttonEl = getNotificationButton(window, "update-available", "button");
      buttonEl.click();
    },
    {
      notificationId: "update-restart",
      button: "secondarybutton",
      cleanup() {
        AppMenuNotifications.removeNotification(/.*/);
      }
    },
  ]);
});

