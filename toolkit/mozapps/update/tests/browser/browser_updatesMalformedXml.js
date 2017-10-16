add_task(async function testMalformedXml() {
  const updateDetailsUrl = "http://example.com/details";
  const maxBackgroundErrors = 10;
  SpecialPowers.pushPrefEnv({set: [
    [PREF_APP_UPDATE_BACKGROUNDMAXERRORS, maxBackgroundErrors],
    [PREF_APP_UPDATE_URL_DETAILS, updateDetailsUrl]
  ]});

  let updateParams = "xmlMalformed=1";

  await runUpdateTest(updateParams, maxBackgroundErrors, [
    {
      // if we fail 10 check attempts, then we want to just show the user a manual update
      // workflow.
      notificationId: "update-manual",
      button: "button",
      beforeClick() {
        checkWhatsNewLink("update-manual-whats-new", updateDetailsUrl);
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
