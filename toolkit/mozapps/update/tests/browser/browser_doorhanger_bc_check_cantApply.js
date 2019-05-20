/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_check_cantApply() {
  lockWriteTestFile();

  let params = {checkAttempts: 1,
                queryString: "&promptWaitTime=0"};
  await runDoorhangerUpdateTest(params, [
    {
      notificationId: "update-manual",
      button: "button",
      checkActiveUpdate: null,
      pageURLs: {whatsNew: gDetailsURL,
                 manual: URL_MANUAL_UPDATE},
    },
  ]);
});
