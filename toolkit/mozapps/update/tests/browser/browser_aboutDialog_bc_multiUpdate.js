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
    async () => {
      prepareToDownloadVersion(SECOND_UPDATE_VERSION);
      await gAUS.checkForBackgroundUpdates();
    },
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
  ]);
});
