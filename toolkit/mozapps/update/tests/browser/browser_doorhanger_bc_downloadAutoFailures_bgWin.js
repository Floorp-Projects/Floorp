/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_downloadAutoFailures_bgWin() {
  function getBackgroundWindowHandler(destroyWindow) {
    return async function() {
      await TestUtils.waitForCondition(
        () => PanelUI.menuButton.hasAttribute("badge-status"),
        "Background window has a badge.",
        undefined,
        200
      ).catch(e => {
        // Instead of throwing let the check below fail the test.
        logTestInfo(e);
      });
      ok(
        PanelUI.menuButton.hasAttribute("badge-status"),
        "PanelUI.menuButton should have a 'badge-status' attribute"
      );
      is(
        PanelUI.notificationPanel.state,
        "closed",
        "The doorhanger is not showing for the background window"
      );
      is(
        PanelUI.menuButton.getAttribute("badge-status"),
        "update-available",
        "The badge is showing for the background window"
      );

      let buttonEl = getNotificationButton(
        extraWindow,
        "update-available",
        "button"
      );
      buttonEl.click();

      if (destroyWindow) {
        // The next popup may be shown during closeWindow or promiseFocus
        // calls.
        let waitForPopupShown = new Promise(resolve => {
          window.addEventListener(
            "popupshown",
            () => {
              executeSoon(resolve);
            },
            { once: true }
          );
        });
        await BrowserTestUtils.closeWindow(extraWindow);
        await SimpleTest.promiseFocus(window);
        await waitForPopupShown;
      }
    };
  }

  const maxBackgroundErrors = 5;
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_APP_UPDATE_BACKGROUNDMAXERRORS, maxBackgroundErrors]],
  });

  let extraWindow = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(extraWindow);

  let params = { checkAttempts: 1, queryString: "&badURL=1", popupShown: true };
  await runDoorhangerUpdateTest(params, [
    getBackgroundWindowHandler(false),
    getBackgroundWindowHandler(true),
    {
      // If the update process is unable to install the update show the manual
      // update doorhanger.
      notificationId: "update-manual",
      button: "button",
      checkActiveUpdate: null,
      pageURLs: { manual: URL_MANUAL_UPDATE },
    },
  ]);
});
