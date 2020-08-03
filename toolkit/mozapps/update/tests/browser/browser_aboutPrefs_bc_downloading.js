/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences background check for updates
// with about:preferences opened during downloading.
add_task(async function aboutPrefs_backgroundCheck_downloading() {
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
  await runAboutPrefsUpdateTest(params, [
    {
      panelId: "downloading",
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      continueFile: CONTINUE_DOWNLOAD,
      downloadInfo,
    },
    async tab => {
      // Once the state is pending but langpacks aren't complete the about
      // dialog should still be showing downloading.
      TestUtils.waitForCondition(() => {
        return readStatusFile() == STATE_PENDING;
      });

      let updateDeckId = await SpecialPowers.spawn(
        tab.linkedBrowser,
        [],
        () => {
          return content.document.getElementById("updateDeck").selectedPanel.id;
        }
      );

      is(updateDeckId, "downloading", "UI should still show as downloading.");

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
      checkActiveUpdate: { state: STATE_PENDING },
      continueFile: null,
    },
  ]);
});
