/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog background check for updates with the
// "notify during download" feature turned on.
add_task(async function aboutDialog_backgroundCheck_downloading_notify() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_APP_UPDATE_NOTIFYDURINGDOWNLOAD, true]],
  });

  let downloadInfo = [];
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED)) {
    downloadInfo[0] = { patchType: "partial", bitsResult: "0" };
  } else {
    downloadInfo[0] = { patchType: "partial", internalResult: "0" };
  }

  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let params = {
    queryString: "&useSlowDownloadMar=1&invalidCompleteSize=1",
    backgroundUpdate: true,
    waitForUpdateState: STATE_DOWNLOADING,
  };
  await runAboutDialogUpdateTest(params, [
    async function aboutDialog_downloading_notification() {
      is(
        PanelUI.notificationPanel.state,
        "closed",
        "The window's doorhanger is closed."
      );
      ok(
        PanelUI.menuButton.hasAttribute("badge-status"),
        "The window has a badge."
      );
      is(
        PanelUI.menuButton.getAttribute("badge-status"),
        "update-downloading",
        "The downloading badge is showing for the background window"
      );
    },
    {
      panelId: "downloading",
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      continueFile: CONTINUE_DOWNLOAD,
      downloadInfo,
    },
    {
      panelId: "apply",
      checkActiveUpdate: { state: STATE_PENDING },
      continueFile: null,
    },
  ]);

  await SpecialPowers.popPrefEnv();
});
