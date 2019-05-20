/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_check_malformedXML() {
  const maxBackgroundErrors = 10;
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_BACKGROUNDMAXERRORS, maxBackgroundErrors],
    ],
  });

  let params = {checkAttempts: maxBackgroundErrors,
                queryString: "&xmlMalformed=1"};
  await runDoorhangerUpdateTest(params, [
    {
      // If the update check fails 10 consecutive attempts then the manual
      // update doorhanger.
      notificationId: "update-manual",
      button: "button",
      checkActiveUpdate: null,
      pageURLs: {whatsNew: gDetailsURL,
                 manual: URL_MANUAL_UPDATE},
    },
  ]);
});
