/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_manual_app_update_policy() {
  await setAppUpdateAutoEnabledHelper(false);

  is(
    Services.policies.isAllowed("autoAppUpdateChecking"),
    false,
    "autoAppUpdateChecking should be disabled by policy"
  );
  is(gAUS.manualUpdateOnly, true, "gAUS.manualUpdateOnly should be true");

  let downloadInfo = [{ patchType: "partial", internalResult: "0" }];
  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let params = { queryString: "&invalidCompleteSize=1" };
  await runAboutDialogUpdateTest(params, [
    {
      panelId: "checkingForUpdates",
      checkActiveUpdate: null,
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "downloadAndInstall",
      checkActiveUpdate: null,
      continueFile: null,
    },
    {
      panelId: "downloading",
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      continueFile: CONTINUE_DOWNLOAD,
      downloadInfo,
    },
    {
      panelId: "apply",
      checkActiveUpdate: { state: STATE_PENDING },
      continueFile: null,
    },
  ]);
});
