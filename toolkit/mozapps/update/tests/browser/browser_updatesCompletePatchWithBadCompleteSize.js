add_task(async function testCompletePatchWithBadCompleteSize() {
  SpecialPowers.pushPrefEnv({set: [[PREF_APP_UPDATE_DOWNLOADPROMPTMAXATTEMPTS, 2]]});

  let updateParams = "completePatchOnly=1&invalidCompleteSize=1";

  await runUpdateTest(updateParams, 1, [
    {
      // if we fail maxBackgroundErrors download attempts, then we want to
      // first show the user an update available prompt.
      notificationId: "update-available",
      button: "button"
    },
    {
      notificationId: "update-available",
      button: "button"
    },
    {
      // if we have only an invalid patch, then something's wrong and we don't
      // have an automatic way to fix it, so show the manual update
      // doorhanger.
      notificationId: "update-manual",
      button: "button",
      beforeClick() {
        checkWhatsNewLink("update-manual-whats-new");
      },
      async cleanup() {
        await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        is(gBrowser.selectedBrowser.currentURI.spec,
           URL_MANUAL_UPDATE, "Landed on manual update page.");
        gBrowser.removeTab(gBrowser.selectedTab);
      }
    },
  ]);
});
