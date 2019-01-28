add_task(async function testBasicPrompt() {
  SpecialPowers.pushPrefEnv({set: [[PREF_APP_UPDATE_SERVICE_ENABLED, false]]});
  lockWriteTestFile();

  let updateParams = "promptWaitTime=0";

  await runUpdateTest(updateParams, 1, [
    {
      notificationId: "update-manual",
      button: "button",
      async cleanup() {
        await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        is(gBrowser.selectedBrowser.currentURI.spec,
           URL_MANUAL_UPDATE, "Landed on manual update page.");
        gBrowser.removeTab(gBrowser.selectedTab);
      },
    },
  ]);
});
