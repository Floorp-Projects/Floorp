/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_patch_completeBadSize() {
  let params = {checkAttempts: 1,
                queryString: "&completePatchOnly=1&invalidCompleteSize=1"};
  await runDoorhangerUpdateTest(params, [
    {
      // If the update download fails maxBackgroundErrors download attempts then
      // show the update available prompt.
      notificationId: "update-available",
      button: "button",
      checkActiveUpdate: null,
      pageURLs: {whatsNew: gDefaultWhatsNewURL},
    },
    {
      notificationId: "update-available",
      button: "button",
      checkActiveUpdate: null,
      pageURLs: {whatsNew: gDefaultWhatsNewURL},
    },
    {
      // If the update process is unable to install the update show the manual
      // update doorhanger.
      notificationId: "update-manual",
      button: "button",
      checkActiveUpdate: null,
      pageURLs: {whatsNew: gDefaultWhatsNewURL,
                 manual: URL_MANUAL_UPDATE},
    },
  ]);
});
