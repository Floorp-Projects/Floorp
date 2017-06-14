add_task(async function testBackgroundWindowFailures() {
  const maxBackgroundErrors = 5;
  SpecialPowers.pushPrefEnv({set: [
    [PREF_APP_UPDATE_BACKGROUNDMAXERRORS, maxBackgroundErrors],
    [PREF_APP_UPDATE_DOWNLOADPROMPTMAXATTEMPTS, 2]
  ]});

  let updateParams = "badURL=1";
  let extraWindow = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(extraWindow);

  function getBackgroundWindowHandler(destroyWindow) {
    return async function() {
      await BrowserTestUtils.waitForCondition(() => PanelUI.menuButton.hasAttribute("badge-status"),
                                              "Background window has a badge.");

      is(PanelUI.notificationPanel.state, "closed",
         "The doorhanger is not showing for the background window");
      is(PanelUI.menuButton.getAttribute("badge-status"), "update-available",
         "The badge is showing for the background window");

      checkWhatsNewLink("update-available-whats-new");
      let buttonEl = getNotificationButton(extraWindow, "update-available", "button");
      buttonEl.click();

      if (destroyWindow) {
        await BrowserTestUtils.closeWindow(extraWindow);
        await SimpleTest.promiseFocus(window);
      }
    };
  }

  await runUpdateTest(updateParams, 1, [
    getBackgroundWindowHandler(false),
    getBackgroundWindowHandler(true),
    {
      notificationId: "update-manual",
      button: "button",
      async cleanup() {
        await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        is(gBrowser.selectedBrowser.currentURI.spec,
           URL_MANUAL_UPDATE, "Landed on manual update page.");
        gBrowser.removeTab(gBrowser.selectedTab);
      }
    },
  ]);
});
