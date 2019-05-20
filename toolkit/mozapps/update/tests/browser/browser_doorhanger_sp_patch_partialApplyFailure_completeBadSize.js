/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_sp_patch_partialApplyFailure_completeBadSize() {
  // Because of the way the test is simulating failure it has to pretend it has
  // already retried.
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_DOWNLOAD_MAXATTEMPTS, 0],
    ],
  });

  let patchProps = {type: "partial",
                    state: STATE_PENDING};
  let patches = getLocalPatchString(patchProps);
  patchProps = {size: "1234",
                selected: "false"};
  patches += getLocalPatchString(patchProps);
  let updateProps = {isCompleteUpdate: "false"};
  let updates = getLocalUpdateString(updateProps, patches);

  let params = {updates};
  await runDoorhangerUpdateTest(params, [
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
