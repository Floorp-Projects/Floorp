/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_downloadOptIn_staging() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Tests the app.update.promptWaitTime pref
      [PREF_APP_UPDATE_PROMPTWAITTIME, 0],
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
    ],
  });
  await UpdateUtils.setAppUpdateAutoEnabled(false);

  let params = { checkAttempts: 1, queryString: "&invalidCompleteSize=1" };
  await runDoorhangerUpdateTest(params, [
    {
      notificationId: "update-available",
      button: "button",
      checkActiveUpdate: null,
      pageURLs: { whatsNew: gDefaultWhatsNewURL },
    },
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: { state: STATE_APPLIED },
    },
  ]);
});
