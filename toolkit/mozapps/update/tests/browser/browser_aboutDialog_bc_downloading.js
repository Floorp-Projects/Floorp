/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog background check for updates
// with the About Dialog opened during downloading.
add_task(async function aboutDialog_backgroundCheck_downloading() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_APP_UPDATE_NOTIFYDURINGDOWNLOAD, false]],
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
    async function aboutDialog_downloading() {
      is(
        PanelUI.notificationPanel.state,
        "closed",
        "The window's doorhanger is closed."
      );
      ok(
        !PanelUI.menuButton.hasAttribute("badge-status"),
        "The window does not have a badge."
      );
    },
    {
      panelId: "downloading",
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      continueFile: CONTINUE_DOWNLOAD,
      downloadInfo,
    },
    async aboutDialog => {
      // Once the state is pending but langpacks aren't complete the about
      // dialog should still be showing downloading.
      TestUtils.waitForCondition(() => {
        return readStatusFile() == STATE_PENDING;
      });

      let updateDeck = aboutDialog.document.getElementById("updateDeck");
      is(
        updateDeck.selectedPanel.id,
        "downloading",
        "UI should still show as downloading."
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
      checkActiveUpdate: { state: STATE_PENDING },
      continueFile: null,
    },
  ]);

  await SpecialPowers.popPrefEnv();
});
