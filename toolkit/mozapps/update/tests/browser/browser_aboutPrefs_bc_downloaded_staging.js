/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for about:preferences background check for updates
// with the update downloaded and about:preferences opened during staging.
add_task(async function aboutPrefs_backgroundCheck_downloaded_staged() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_APP_UPDATE_STAGING_ENABLED, true]],
  });

  let lankpackCall = mockLangpackInstall();

  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let params = {
    queryString: "&invalidCompleteSize=1",
    backgroundUpdate: true,
    waitForUpdateState: STATE_PENDING,
  };
  await runAboutPrefsUpdateTest(params, [
    {
      panelId: "applying",
      checkActiveUpdate: { state: STATE_PENDING },
      continueFile: CONTINUE_STAGING,
    },
    async tab => {
      // Once the state is pending but langpacks aren't complete the about
      // dialog should still be showing downloading.
      TestUtils.waitForCondition(() => {
        return readStatusFile() == STATE_APPLIED;
      });

      let updateDeckId = await SpecialPowers.spawn(
        tab.linkedBrowser,
        [],
        () => {
          return content.document.getElementById("updateDeck").selectedPanel.id;
        }
      );

      is(updateDeckId, "applying", "UI should still show as applying.");

      let { appVersion, resolve } = await lankpackCall;
      is(
        appVersion,
        Services.appinfo.version,
        "Should see the right app version."
      );
      resolve();
    },
    {
      panelId: "apply",
      checkActiveUpdate: { state: STATE_APPLIED },
      continueFile: null,
    },
  ]);
});
