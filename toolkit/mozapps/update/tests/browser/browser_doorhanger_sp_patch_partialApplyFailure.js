/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_sp_patch_partialApplyFailure() {
  let patchProps = {type: "partial",
                    state: STATE_PENDING};
  let patches = getLocalPatchString(patchProps);
  let updateProps = {isCompleteUpdate: "false",
                     checkInterval: "1"};
  let updates = getLocalUpdateString(updateProps, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);

  let updateParams = "";
  await runDoorhangerUpdateTest(updateParams, 0, [
    {
      // If there is only an invalid patch show the manual update doorhanger.
      notificationId: "update-manual",
      button: "button",
      checkActiveUpdate: null,
      pageURLs: {whatsNew: gDefaultWhatsNewURL,
                 manual: URL_MANUAL_UPDATE},
    },
  ]);
});
