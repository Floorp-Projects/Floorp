/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for doorhanger background check for updates
// with an unsupported update.
add_task(async function doorhanger_bc_check_unsupported() {
  let params = { checkAttempts: 1, queryString: "&unsupported=1" };
  await runDoorhangerUpdateTest(params, [
    {
      notificationId: "update-unsupported",
      button: "button",
      pageURLs: { manual: gDetailsURL },
    },
    async function doorhanger_unsupported_persist() {
      is(
        PanelUI.notificationPanel.state,
        "closed",
        "The window's doorhanger is closed."
      );
      ok(
        PanelUI.menuButton.hasAttribute("badge-status"),
        "The window has a badge."
      );
      is(
        PanelUI.menuButton.getAttribute("badge-status"),
        "update-unsupported",
        "The correct badge is showing for the background window"
      );

      // Test persistence of the badge when the client has restarted by
      // resetting the UpdateListener.
      UpdateListener.reset();
      is(
        PanelUI.notificationPanel.state,
        "closed",
        "The window's doorhanger is closed."
      );
      ok(
        !PanelUI.menuButton.hasAttribute("badge-status"),
        "The window does not have a badge."
      );
      UpdateListener.init();
      is(
        PanelUI.notificationPanel.state,
        "closed",
        "The window's doorhanger is closed."
      );
      ok(
        PanelUI.menuButton.hasAttribute("badge-status"),
        "The window has a badge."
      );
      is(
        PanelUI.menuButton.getAttribute("badge-status"),
        "update-unsupported",
        "The correct badge is showing for the background window."
      );
    },
  ]);

  params = {
    checkAttempts: 1,
    queryString: "&invalidCompleteSize=1&promptWaitTime=0",
  };
  await runDoorhangerUpdateTest(params, [
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: { state: STATE_PENDING },
    },
    async function doorhanger_unsupported_removed() {
      // Test that finding an update removes the app.update.unsupported.url
      // preference.
      let unsupportedURL = Services.prefs.getCharPref(
        PREF_APP_UPDATE_UNSUPPORTED_URL,
        null
      );
      ok(
        !unsupportedURL,
        "The " + PREF_APP_UPDATE_UNSUPPORTED_URL + " preference was removed."
      );
    },
  ]);
});
