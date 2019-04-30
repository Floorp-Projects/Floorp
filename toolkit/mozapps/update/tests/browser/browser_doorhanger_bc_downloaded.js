/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_downloaded() {
  let updateParams = "&invalidCompleteSize=1&promptWaitTime=0";
  await runDoorhangerUpdateTest(updateParams, 1, [
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: {state: STATE_PENDING},
    },
  ]);
});
