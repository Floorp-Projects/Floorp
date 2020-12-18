/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test downloads 2 updates sequentially, and ensures that the doorhanger
 * and badge do what they are supposed to do:
 *   First thing after the first download, the doorhanger should be displayed.
 *   Then download the next update.
 *   While that update stages, the badge should be hidden to prevent restarting
 *     to update while the update is staging.
 *   Once the staging completes, the badge should return. The doorhanger should
 *     not be shown at this time, because it has already been shown this
 *     session.
 */

const FIRST_UPDATE_VERSION = "999998.0";
const SECOND_UPDATE_VERSION = "999999.0";

function prepareToDownloadVersion(version) {
  setUpdateURL(
    URL_HTTP_UPDATE_SJS +
      `?detailsURL=${gDetailsURL}&promptWaitTime=0&appVersion=${version}`
  );
}

add_task(async function doorhanger_bc_multiUpdate() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_APP_UPDATE_STAGING_ENABLED, true]],
  });

  let params = {
    checkAttempts: 1,
    queryString: "&promptWaitTime=0",
    version: FIRST_UPDATE_VERSION,
    slowStaging: true,
  };
  await runDoorhangerUpdateTest(params, [
    () => {
      return continueFileHandler(CONTINUE_STAGING);
    },
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: { state: STATE_APPLIED },
    },
    async () => {
      is(
        PanelUI.menuButton.getAttribute("badge-status"),
        "update-restart",
        "Should have restart badge"
      );

      prepareToDownloadVersion(SECOND_UPDATE_VERSION);
      let updateSwapped = waitForEvent("update-swap");
      gAUS.checkForBackgroundUpdates();
      await updateSwapped;
      // The badge should be hidden while we swap from one update to the other
      // to prevent restarting to update while staging is occurring. But since
      // it will be waiting on the same event we are waiting on, wait an
      // additional tick to let the other update-swap listeners run.
      await TestUtils.waitForTick();

      is(
        PanelUI.menuButton.getAttribute("badge-status"),
        "",
        "Should not have restart badge during staging"
      );

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
        "Restart badge should be restored after staging completes"
      );
      is(
        PanelUI.notificationPanel.state,
        "closed",
        "Should not open a second doorhanger"
      );
    },
  ]);
});
