/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_downloaded_disableBITS() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_APP_UPDATE_BITS_ENABLED, true]],
  });

  let params = {
    checkAttempts: 1,
    queryString: "&promptWaitTime=0&disableBITS=true",
  };
  await runDoorhangerUpdateTest(params, [
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: { state: STATE_PENDING },
    },
  ]);

  let patch = getPatchOfType("partial").QueryInterface(
    Ci.nsIWritablePropertyBag
  );
  ok(
    !patch.getProperty("bitsId"),
    "The selected patch should not have a bitsId property"
  );
});
