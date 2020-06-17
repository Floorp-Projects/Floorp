/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog foreground check for updates
// with an automatic download.
add_task(async function aboutDialog_foregroundCheck_downloadAuto() {
  let downloadInfo = [];
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED)) {
    downloadInfo[0] = { patchType: "partial", bitsResult: "0" };
  } else {
    downloadInfo[0] = { patchType: "partial", internalResult: "0" };
  }

  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let params = { queryString: "&invalidCompleteSize=1&promptWaitTime=0" };
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "checkingForUpdates",
      checkActiveUpdate: null,
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "downloading",
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      continueFile: CONTINUE_DOWNLOAD,
      downloadInfo,
    },
    async function aboutDialog_restart_notification() {
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
        "update-restart",
        "The restart badge is showing for the background window"
      );
    },
    {
      panelId: "apply",
      checkActiveUpdate: { state: STATE_PENDING },
      continueFile: null,
    },
  ]);
});
