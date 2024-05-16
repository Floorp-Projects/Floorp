/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test downloads 2 updates sequentially, and ensures that the doorhanger
 * and badge do what they are supposed to do. However, the first update has a
 * long promptWaitTime, and the second has a short one and the badge wait time
 * is set to 0. This should result in this behavior:
 *   First thing after the first download, the badge should be displayed, but
 *     not the doorhanger.
 *   Then download the next update.
 *   While that update stages, the badge should be hidden to prevent restarting
 *     to update while the update is staging.
 *   Once the staging completes, the doorhanger should be shown. Despite the
 *     long promptWaitTime of the initial update, this patch's short wait time
 *     means that the doorhanger should be shown soon rather than in a long
 *     time.
 */

const FIRST_UPDATE_VERSION = "999998.0";
const SECOND_UPDATE_VERSION = "999999.0";
const LONG_PROMPT_WAIT_TIME_SEC = 10 * 60 * 60; // 10 hours

function prepareToDownloadVersion(version, promptWaitTime) {
  setUpdateURL(
    URL_HTTP_UPDATE_SJS +
      `?detailsURL=${gDetailsURL}&promptWaitTime=${promptWaitTime}&appVersion=${version}`
  );
}

add_task(async function doorhanger_bc_multiUpdate() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
      [PREF_APP_UPDATE_BADGEWAITTIME, 0],
    ],
  });

  let params = {
    checkAttempts: 1,
    queryString: `&promptWaitTime=${LONG_PROMPT_WAIT_TIME_SEC}`,
    version: FIRST_UPDATE_VERSION,
    slowStaging: true,
  };
  await runDoorhangerUpdateTest(params, [
    async () => {
      await continueFileHandler(CONTINUE_STAGING);

      try {
        await TestUtils.waitForCondition(
          () =>
            PanelUI.menuButton.getAttribute("badge-status") == "update-restart",
          "Waiting for update restart badge to return after staging"
        );
      } catch (ex) {}

      is(
        PanelUI.menuButton.getAttribute("badge-status"),
        "update-restart",
        "Should have restart badge"
      );

      prepareToDownloadVersion(SECOND_UPDATE_VERSION, 0);
      let updateSwapped = waitForEvent("update-swap");
      await gAUS.checkForBackgroundUpdates();
      await updateSwapped;
      // The badge should be hidden while we swap from one update to the other
      // to prevent restarting to update while staging is occurring. But since
      // it will be waiting on the same event we are waiting on, wait an
      // additional tick to let the other update-swap listeners run.
      await TestUtils.waitForTick();

      is(
        PanelUI.menuButton.getAttribute("badge-status"),
        null,
        "Should not have restart badge during staging"
      );

      await continueFileHandler(CONTINUE_STAGING);
    },
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: { state: STATE_APPLIED },
    },
  ]);
});
