/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for About Dialog background check for updates
// with the update downloaded and staging has failed when the About Dialog is
// opened.
add_task(
  async function aboutDialog_backgroundCheck_downloaded_stagingFailure() {
    gEnv.set("MOZ_TEST_STAGING_ERROR", "1");
    await SpecialPowers.pushPrefEnv({
      set: [[PREF_APP_UPDATE_STAGING_ENABLED, true]],
    });

    // Since the partial should be successful specify an invalid size for the
    // complete update.
    let params = {
      queryString: "&completePatchOnly=1",
      backgroundUpdate: true,
      waitForUpdateState: STATE_PENDING,
    };
    await runAboutDialogUpdateTest(params, [
      {
        panelId: "apply",
        checkActiveUpdate: { state: STATE_PENDING },
        continueFile: null,
      },
    ]);
  }
);
