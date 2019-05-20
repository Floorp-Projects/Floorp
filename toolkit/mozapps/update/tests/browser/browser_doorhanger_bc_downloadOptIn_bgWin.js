/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_downloadOptIn_bgWin() {
  function getBackgroundWindowHandler() {
    return async function() {
      await TestUtils.waitForCondition(() =>
        PanelUI.menuButton.hasAttribute("badge-status"),
        "Background window has a badge.", undefined, 200
      ).catch(e => {
        // Instead of throwing let the check below fail the test.
        logTestInfo(e);
      });
      ok(PanelUI.menuButton.hasAttribute("badge-status"),
         "PanelUI.menuButton should have a 'badge-status' attribute");
      is(PanelUI.notificationPanel.state, "closed",
         "The doorhanger is not showing for the background window");
      is(PanelUI.menuButton.getAttribute("badge-status"), "update-available",
         "The badge is showing for the background window");
      let popupShownPromise =
        BrowserTestUtils.waitForEvent(PanelUI.notificationPanel, "popupshown");
      await BrowserTestUtils.closeWindow(extraWindow);
      await SimpleTest.promiseFocus(window);
      await popupShownPromise;

      checkWhatsNewLink(window, "update-available-whats-new",
                        gDefaultWhatsNewURL);
      let buttonEl =
        getNotificationButton(window, "update-available", "button");
      buttonEl.click();
    };
  }

  await UpdateUtils.setAppUpdateAutoEnabled(false);

  let extraWindow = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(extraWindow);

  let params = {checkAttempts: 1,
                queryString: "&promptWaitTime=0"};
  await runDoorhangerUpdateTest(params, [
    getBackgroundWindowHandler(),
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: {state: STATE_PENDING},
    },
  ]);
});
