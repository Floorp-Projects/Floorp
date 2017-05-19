add_task(async function testCompletePatchApplyFailure() {
  let patches = getLocalPatchString("complete", null, null, null, null, null,
                                    STATE_PENDING);
  let updates = getLocalUpdateString(patches, null, null, null,
                                     Services.appinfo.version, null);

  await runUpdateProcessingTest(updates, [
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
           URL_MANUAL_UPDATE, "Landed on manual update page.")
        gBrowser.removeTab(gBrowser.selectedTab);
      }
    },
  ]);
});
