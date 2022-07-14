/* import-globals-from ../../../../../browser/modules/test/browser/head.js */
/* import-globals-from antitracking_head.js */

async function openPageAndRunCode(
  topPage,
  topPageCallback,
  embeddedPage,
  embeddedPageCallback
) {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: topPage,
    waitForLoad: true,
  });
  let browser = gBrowser.getBrowserForTab(tab);

  await topPageCallback();
  await SpecialPowers.spawn(
    browser,
    [{ page: embeddedPage, callback: embeddedPageCallback.toString() }],
    async function(obj) {
      await new content.Promise(resolve => {
        let ifr = content.document.createElement("iframe");
        ifr.onload = function() {
          ifr.contentWindow.postMessage(obj.callback, "*");
        };

        content.addEventListener("message", function msg(event) {
          if (event.data.type == "finish") {
            content.removeEventListener("message", msg);
            resolve();
            return;
          }

          if (event.data.type == "ok") {
            ok(event.data.what, event.data.msg);
            return;
          }

          if (event.data.type == "info") {
            info(event.data.msg);
            return;
          }

          ok(false, "Unknown message");
        });

        content.document.body.appendChild(ifr);
        ifr.src = obj.page;
      });
    }
  );

  await BrowserTestUtils.removeTab(tab);
}

// This function returns a function that spawns an asynchronous task to handle
// the popup and click on the appropriate values. If that task is never executed
// the catch case is reached and we fail the test. If for some reason that catch
// case isn't reached, having an extra event listener at the end of the test
// will cause the test to fail anyway.
// Note: this means that tests that use this callback should probably be in
// their own test file.
function getExpectPopupAndClick(accept) {
  return function() {
    let shownPromise = BrowserTestUtils.waitForEvent(
      PopupNotifications.panel,
      "popupshown"
    );
    shownPromise
      .then(async _ => {
        // This occurs when the promise resolves on the test finishing
        let popupNotifications = PopupNotifications.panel.childNodes;
        if (!popupNotifications.length) {
          ok(false, "Prompt did not show up");
        } else if (accept == "accept") {
          ok(true, "Prompt shows up, clicking accept.");
          await clickMainAction();
        } else if (accept == "reject") {
          ok(true, "Prompt shows up, clicking reject.");
          await clickSecondaryAction();
        } else {
          ok(false, "Unknown accept value for test: " + accept);
          info("Clicking accept so that the test can finish.");
          await clickMainAction();
        }
      })
      .catch(() => {
        ok(false, "Prompt did not show up");
      });
  };
}

// This function spawns an asynchronous task that fails the test if a popup
// appears. If that never happens, the catch case is executed on the test
// cleanup.
// Note: this means that tests that use this callback should probably be in
// their own test file.
function expectNoPopup() {
  let shownPromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  shownPromise
    .then(async _ => {
      // This occurs when the promise resolves on the test finishing
      let popupNotifications = PopupNotifications.panel.childNodes;
      if (!popupNotifications.length) {
        ok(true, "Prompt did not show up");
      } else {
        ok(false, "Prompt shows up");
        info(PopupNotifications.panel);
        await clickSecondaryAction();
      }
    })
    .catch(() => {
      ok(true, "Prompt did not show up");
    });
}

async function requestStorageAccessAndExpectSuccess() {
  SpecialPowers.wrap(document).notifyUserGestureActivation();
  let p = document.requestStorageAccess();
  try {
    await p;
    ok(true, "gain storage access.");
  } catch {
    ok(false, "denied storage access.");
  }
  SpecialPowers.wrap(document).clearUserGestureActivation();
}

async function requestStorageAccessAndExpectFailure() {
  SpecialPowers.wrap(document).notifyUserGestureActivation();
  let p = document.requestStorageAccess();
  try {
    await p;
    ok(false, "gain storage access.");
  } catch {
    ok(true, "denied storage access.");
  }
  SpecialPowers.wrap(document).clearUserGestureActivation();
}

async function cleanUpData() {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
  ok(true, "Deleted all data.");
}

async function setPreferences() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.storage_access.auto_grants", true],
      ["dom.storage_access.auto_grants.delayed", false],
      ["dom.storage_access.enabled", true],
      ["dom.storage_access.max_concurrent_auto_grants", 0],
      ["dom.storage_access.prompt.testing", false],
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      [
        "network.cookie.cookieBehavior.pbmode",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
    ],
  });
}
