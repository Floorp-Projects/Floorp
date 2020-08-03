/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog background check for updates
// with the About Dialog opened during downloading and stages the update.
add_task(async function aboutDialog_backgroundCheck_downloading_staging() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_APP_UPDATE_STAGING_ENABLED, true]],
  });

  let downloadInfo = [];
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED)) {
    downloadInfo[0] = { patchType: "partial", bitsResult: "0" };
  } else {
    downloadInfo[0] = { patchType: "partial", internalResult: "0" };
  }

  let lankpackCall = mockLangpackInstall();

  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let params = {
    queryString: "&useSlowDownloadMar=1&invalidCompleteSize=1",
    backgroundUpdate: true,
    waitForUpdateState: STATE_DOWNLOADING,
  };
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "downloading",
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      continueFile: CONTINUE_DOWNLOAD,
      downloadInfo,
    },
    {
      panelId: "applying",
      checkActiveUpdate: { state: STATE_PENDING },
      continueFile: CONTINUE_STAGING,
    },
    async aboutDialog => {
      // Once the state is applied but langpacks aren't complete the about
      // dialog should still be showing applying.
      TestUtils.waitForCondition(() => {
        return readStatusFile() == STATE_APPLIED;
      });

      let updateDeck = aboutDialog.document.getElementById("updateDeck");
      is(
        updateDeck.selectedPanel.id,
        "applying",
        "UI should still show as applying."
      );

      let { appVersion, resolve } = await lankpackCall;
      is(
        appVersion,
        Services.appinfo.version,
        "Should see the right app version."
      );
      resolve();
    },
    {
      panelId: "apply",
      checkActiveUpdate: { state: STATE_APPLIED },
      continueFile: null,
    },
  ]);
});
