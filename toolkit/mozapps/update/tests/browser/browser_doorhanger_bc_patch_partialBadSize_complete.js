/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_patch_partialBadSize_complete() {
  let params = {
    checkAttempts: 1,
    queryString: "&invalidPartialSize=1&promptWaitTime=0",
  };
  await runDoorhangerUpdateTest(params, [
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: { state: STATE_PENDING },
    },
  ]);
});
