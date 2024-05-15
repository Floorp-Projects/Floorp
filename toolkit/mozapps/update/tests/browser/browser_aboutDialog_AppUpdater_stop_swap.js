/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FIRST_UPDATE_VERSION = "999998.0";
const SECOND_UPDATE_VERSION = "999999.0";

function prepareToDownloadVersion(version) {
  setUpdateURL(
    URL_HTTP_UPDATE_SJS +
      `?detailsURL=${gDetailsURL}&promptWaitTime=0&appVersion=${version}`
  );
}

add_task(async function aboutDialog_backgroundCheck_multiUpdate() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_APP_UPDATE_STAGING_ENABLED, true]],
  });

  let params = {
    version: FIRST_UPDATE_VERSION,
    backgroundUpdate: true,
    waitForUpdateState: STATE_PENDING,
  };
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "applying",
      checkActiveUpdate: { state: STATE_PENDING },
      continueFile: CONTINUE_STAGING,
    },
    {
      panelId: "apply",
      checkActiveUpdate: { state: STATE_APPLIED },
      continueFile: null,
    },
    () => {
      prepareToDownloadVersion(SECOND_UPDATE_VERSION);
      gAUS.checkForBackgroundUpdates();
    },
    {
      panelId: "applying",
      checkActiveUpdate: { state: STATE_PENDING },
      // Don't pass a continue file in order to leave us in the staging state.
    },
    aboutDialog => {
      aboutDialog.gAppUpdater._appUpdater.stop();
    },
    {
      panelId: "checkForUpdates",
      checkActiveUpdate: { state: STATE_PENDING },
      expectedStateOverride: Ci.nsIApplicationUpdateService.STATE_STAGING,
    },
  ]);

  // Ideally this would go in a cleanup function. But this needs to happen
  // before any other cleanup functions and for some reason cleanup functions
  // do not always seem to execute in reverse registration order.
  dump("Cleanup: Waiting for staging to finish.\n");
  await continueFileHandler(CONTINUE_STAGING);
  if (gAUS.currentState == Ci.nsIApplicationUpdateService.STATE_STAGING) {
    await gAUS.stateTransition;
  }
});
