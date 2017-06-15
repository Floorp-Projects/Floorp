add_task(async function testPartialPatchApplyFailureWithCompleteValidationFailure() {
  // because of the way we're simulating failure, we have to just pretend we've already
  // retried.
  SpecialPowers.pushPrefEnv({set: [[PREF_APP_UPDATE_DOWNLOADPROMPTMAXATTEMPTS, 0]]});

  let patchProps = {type: "partial",
                    state: STATE_PENDING};
  let patches = getLocalPatchString(patchProps);
  patchProps = {size: "1234",
                selected: "false"};
  patches += getLocalPatchString(patchProps);
  let updateProps = {isCompleteUpdate: "false"};
  let updates = getLocalUpdateString(updateProps, patches);

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
