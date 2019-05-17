/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_sp_patch_partialApplyFailure_complete() {
  let patchProps = {type: "partial",
                    state: STATE_PENDING};
  let patches = getLocalPatchString(patchProps);
  patchProps = {selected: "false"};
  patches += getLocalPatchString(patchProps);
  let updateProps = {isCompleteUpdate: "false",
                     promptWaitTime: "0"};
  let updates = getLocalUpdateString(updateProps, patches);

  let params = {updates};
  await runDoorhangerUpdateTest(params, [
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: {state: STATE_PENDING},
    },
  ]);
});
