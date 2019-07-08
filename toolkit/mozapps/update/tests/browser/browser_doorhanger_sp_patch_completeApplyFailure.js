/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_sp_patch_completeApplyFailure() {
  let patchProps = { state: STATE_PENDING };
  let patches = getLocalPatchString(patchProps);
  let updateProps = { checkInterval: "1" };
  let updates = getLocalUpdateString(updateProps, patches);

  let params = { updates };
  await runDoorhangerUpdateTest(params, [
    {
      // If the update process is unable to install the update show the manual
      // update doorhanger.
      notificationId: "update-manual",
      button: "button",
      checkActiveUpdate: null,
      pageURLs: { whatsNew: gDefaultWhatsNewURL, manual: URL_MANUAL_UPDATE },
    },
  ]);
});
