/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function doorhanger_bc_downloadOptIn() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "app.releaseNotesURL.prompt",
        `${URL_HOST}/%LOCALE%/firefox/%VERSION%/releasenotes/?utm_source=firefox-browser&utm_medium=firefox-desktop&utm_campaign=updateprompt`,
      ],
    ],
  });
  await UpdateUtils.setAppUpdateAutoEnabled(false);

  let version = "9999999.0";

  let params = {
    checkAttempts: 1,
    queryString: "&invalidCompleteSize=1&promptWaitTime=0",
    version,
  };

  let versionString = version;
  switch (UpdateUtils.getUpdateChannel(false)) {
    case "beta":
    case "aurora":
      versionString += "beta";
      break;
  }

  await runDoorhangerUpdateTest(params, [
    {
      // Test that the Learn More link opens the correct release notes page.
      notificationId: "update-available",
      button: n => n.querySelector(".popup-notification-learnmore-link"),
      checkActiveUpdate: null,
      pageURLs: {
        manual: Services.urlFormatter.formatURL(
          `${URL_HOST}/%LOCALE%/firefox/${versionString}/releasenotes/?utm_source=firefox-browser&utm_medium=firefox-desktop&utm_campaign=updateprompt`
        ),
      },
    },
    {
      notificationId: "update-available",
      button: "button",
      checkActiveUpdate: null,
      popupShown: true,
    },
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      checkActiveUpdate: { state: STATE_PENDING },
    },
  ]);
});
