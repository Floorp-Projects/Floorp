/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_downloadOptIn_staging() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
    ],
  });
  await UpdateUtils.setAppUpdateAutoEnabled(false);

  let updateParams = "&invalidCompleteSize=1&promptWaitTime=0";
  await runDoorhangerUpdateTest(updateParams, 1, [
    {
      notificationId: "update-available",
      button: "button",
      checkActiveUpdate: null,
      pageURLs: {whatsNew: gDefaultWhatsNewURL},
    },
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: {state: STATE_APPLIED},
    },
  ]);
});
