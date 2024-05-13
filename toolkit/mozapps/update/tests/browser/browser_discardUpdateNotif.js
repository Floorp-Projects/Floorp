/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test if the hamburger menu in about:preferences will remove the "update-restart" badge
// after the user declines the in-progress update when turning off automatic update
add_task(async function aboutPrefs_foregroundCheck_discardUpdateNotif() {
  await SpecialPowers.pushPrefEnv({
    set: [["app.update.badgeWaitTime", 0]],
  });

  let downloadInfo = [];
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED)) {
    downloadInfo[0] = { patchType: "partial", bitsResult: "0" };
  } else {
    downloadInfo[0] = { patchType: "partial", internalResult: "0" };
  }

  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let params = { queryString: "&promptWaitTime=0" };

  await runAboutPrefsUpdateTest(params, [
    {
      panelId: "checkingForUpdates",
      checkActiveUpdate: null,
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "downloading",
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      continueFile: CONTINUE_DOWNLOAD,
      downloadInfo,
    },
    async function aboutDialog_restart_notification() {
      is(
        PanelUI.isNotificationPanelOpen,
        true,
        "An update avaliable doorhanger notification exists"
      );
      let dismissButton = getNotificationButton(
        window,
        AppMenuNotifications.activeNotification.id,
        "secondaryButton"
      );
      dismissButton.click();
      await TestUtils.waitForCondition(
        () => PanelUI.menuButton.hasAttribute("badge-status"),
        "Waiting for update badge",
        undefined,
        200
      ).catch(e => {
        // Instead of throwing let the check below fail the test.
        logTestInfo(e);
      });
      ok(
        PanelUI.menuButton.hasAttribute("badge-status"),
        "The window has a badge."
      );
      is(
        PanelUI.menuButton.getAttribute("badge-status"),
        "update-restart",
        "The update restart badge is showing on the hamburger menu"
      );
    },
  ]);
  let manualDesktopCheckbox = content.document.getElementById("manualDesktop");
  manualDesktopCheckbox.click();
  is(
    manualDesktopCheckbox.selected,
    true,
    "Manual update option should be selected"
  );
  //The "accept" button is actually the "discard update" button on the dialog
  await BrowserTestUtils.promiseAlertDialog("accept");

  let notif_cleared_test = async () => {
    await TestUtils.waitForCondition(
      () => !PanelUI.menuButton.hasAttribute("badge-status"),
      "Waiting for the update badge to be cleared",
      undefined,
      200
    ).catch(e => {
      // Instead of throwing let the check below fail the test.
      logTestInfo(e);
    });
    is(
      PanelUI.menuButton.getAttribute("badge-status"),
      null,
      "The window does not have a badge."
    );
  };
  await notif_cleared_test();
});
