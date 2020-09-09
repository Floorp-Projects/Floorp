"use strict";

async function focusWindow(win) {
  if (Services.focus.activeWindow == win) {
    return;
  }
  let promise = new Promise(resolve => {
    win.addEventListener(
      "focus",
      function() {
        resolve();
      },
      { capture: true, once: true }
    );
  });
  win.focus();
  await promise;
}

function getDialogDoc() {
  // Trudge through all the open windows, until we find the one
  // that has either commonDialog.xhtml or selectDialog.xhtml loaded.
  // var enumerator = Services.wm.getEnumerator("navigator:browser");
  for (let { docShell } of Services.wm.getEnumerator(null)) {
    var containedDocShells = docShell.getAllDocShellsInSubtree(
      docShell.typeChrome,
      docShell.ENUMERATE_FORWARDS
    );
    for (let childDocShell of containedDocShells) {
      // Get the corresponding document for this docshell
      // We don't want it if it's not done loading.
      if (childDocShell.busyFlags != Ci.nsIDocShell.BUSY_FLAGS_NONE) {
        continue;
      }
      var childDoc = childDocShell.contentViewer.DOMDocument;
      if (
        childDoc.location.href !=
          "chrome://global/content/commonDialog.xhtml" &&
        childDoc.location.href != "chrome://global/content/selectDialog.xhtml"
      ) {
        continue;
      }

      // We're expecting the dialog to be focused. If it's not yet, try later.
      // (In particular, this is needed on Linux to reliably check focused elements.)
      if (Services.focus.focusedWindow != childDoc.defaultView) {
        continue;
      }

      return childDoc;
    }
  }

  return null;
}

async function waitForAuthPrompt() {
  let promptDoc = await TestUtils.waitForCondition(() => {
    return getAuthPrompt();
  });
  info("Got prompt: " + promptDoc);
  return promptDoc;
}

function getAuthPrompt() {
  let doc = getDialogDoc();
  if (!doc) {
    return false; // try again in a bit
  }
  return doc;
}

async function loadAccessRestrictedURL(browser, url, username, password) {
  let browserLoaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.loadURI(browser, url);

  let promptDoc = await waitForAuthPrompt();
  let dialogUI = promptDoc.defaultView.Dialog.ui;
  ok(dialogUI, "Got expected HTTP auth dialog Dialog.ui");

  // fill and submit the dialog form
  dialogUI.loginTextbox.value = username;
  dialogUI.password1Textbox.value = password;
  promptDoc.getElementById("commonDialog").acceptDialog();
  await SimpleTest.promiseFocus(browser.ownerGlobal);
  await browserLoaded;
}

const PRIVATE_BROWSING_CAPTURE_PREF = "signon.privateBrowsingCapture.enabled";
let nsLoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);
let login = new nsLoginInfo(
  "https://example.com",
  "https://example.com",
  null,
  "notifyu1",
  "notifyp1",
  "user",
  "pass"
);
const form1Url = `https://example.com/${DIRECTORY_PATH}subtst_privbrowsing_1.html`;
const form2Url = `https://example.com/${DIRECTORY_PATH}form_password_change.html`;
const authUrl = `https://example.com/${DIRECTORY_PATH}authenticate.sjs`;

let normalWin;
let privateWin;

// XXX: Note that tasks are currently run in sequence. Some tests may assume the state
// resulting from successful or unsuccessful logins in previous tasks

add_task(async function test_setup() {
  normalWin = await BrowserTestUtils.openNewBrowserWindow({ private: false });
  privateWin = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  Services.logins.removeAllLogins();
});

add_task(async function test_normal_popup_notification_1() {
  info("test 1: run outside of private mode, popup notification should appear");
  await focusWindow(normalWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: normalWin.gBrowser,
      url: form1Url,
    },
    async function(browser) {
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {
          "#user": "notifyu1",
          "#pass": "notifyp1",
        }
      );
      is(fieldValues.username, "notifyu1", "Checking submitted username");
      is(fieldValues.password, "notifyp1", "Checking submitted password");

      let notif = getCaptureDoorhanger(
        "password-save",
        PopupNotifications,
        browser
      );
      ok(notif, "got notification popup");
      if (notif) {
        await TestUtils.waitForCondition(
          () => !notif.dismissed,
          "notification should not be dismissed"
        );
        await cleanupDoorhanger(notif);
      }
    }
  );
});

add_task(async function test_private_popup_notification_2() {
  info(
    "test 2: run inside of private mode, dismissed popup notification should appear"
  );

  const capturePrefValue = Services.prefs.getBoolPref(
    PRIVATE_BROWSING_CAPTURE_PREF
  );
  ok(
    capturePrefValue,
    `Expect ${PRIVATE_BROWSING_CAPTURE_PREF} to default to true`
  );

  // clear existing logins for parity with the previous test
  Services.logins.removeAllLogins();
  await focusWindow(privateWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: privateWin.gBrowser,
      url: form1Url,
    },
    async function(browser) {
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {
          "#user": "notifyu1",
          "#pass": "notifyp1",
        }
      );
      is(fieldValues.username, "notifyu1", "Checking submitted username");
      is(fieldValues.password, "notifyp1", "Checking submitted password");

      let notif = getCaptureDoorhanger(
        "password-save",
        PopupNotifications,
        browser
      );
      ok(notif, "Expected notification popup");
      if (notif) {
        await TestUtils.waitForCondition(
          () => notif.dismissed,
          "notification should be dismissed"
        );

        let { panel } = privateWin.PopupNotifications;
        let promiseShown = BrowserTestUtils.waitForEvent(panel, "popupshown");
        notif.anchorElement.click();
        await promiseShown;

        let notificationElement = panel.childNodes[0];
        let toggleCheckbox = notificationElement.querySelector(
          "#password-notification-visibilityToggle"
        );

        ok(!toggleCheckbox.hidden, "Toggle should be visible upon 1st opening");

        info("Hiding popup.");
        let promiseHidden = BrowserTestUtils.waitForEvent(panel, "popuphidden");
        panel.hidePopup();
        await promiseHidden;

        info("Clicking on anchor to reshow popup.");
        promiseShown = BrowserTestUtils.waitForEvent(panel, "popupshown");
        notif.anchorElement.click();
        await promiseShown;

        ok(toggleCheckbox.hidden, "Toggle should be hidden upon 2nd opening");

        await cleanupDoorhanger(notif);
      }
    }
  );
  is(Services.logins.getAllLogins().length, 0, "No logins were saved");
});

add_task(async function test_private_popup_notification_no_capture_pref_2b() {
  info(
    "test 2b: run inside of private mode, with capture pref off," +
      "popup notification should not appear"
  );

  const capturePrefValue = Services.prefs.getBoolPref(
    PRIVATE_BROWSING_CAPTURE_PREF
  );
  Services.prefs.setBoolPref(PRIVATE_BROWSING_CAPTURE_PREF, false);

  // clear existing logins for parity with the previous test
  Services.logins.removeAllLogins();

  await focusWindow(privateWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: privateWin.gBrowser,
      url: form1Url,
    },
    async function(browser) {
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {
          "#user": "notifyu1",
          "#pass": "notifyp1",
        }
      );
      is(fieldValues.username, "notifyu1", "Checking submitted username");
      is(fieldValues.password, "notifyp1", "Checking submitted password");

      let notif = getCaptureDoorhanger(
        "password-save",
        PopupNotifications,
        browser
      );
      // restore the pref to its original value
      Services.prefs.setBoolPref(
        PRIVATE_BROWSING_CAPTURE_PREF,
        capturePrefValue
      );

      ok(!notif, "Expected no notification popup");
      if (notif) {
        await cleanupDoorhanger(notif);
      }
    }
  );
  is(Services.logins.getAllLogins().length, 0, "No logins were saved");
});

add_task(async function test_normal_popup_notification_3() {
  info(
    "test 3: run with a login, outside of private mode, " +
      "match existing username/password: no popup notification should appear"
  );

  Services.logins.removeAllLogins();
  Services.logins.addLogin(login);
  let allLogins = Services.logins.getAllLogins();
  // Sanity check the HTTP login exists.
  is(allLogins.length, 1, "Should have the HTTP login");
  let timeLastUsed = allLogins[0].timeLastUsed;
  let loginGuid = allLogins[0].guid;

  await focusWindow(normalWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: normalWin.gBrowser,
      url: form1Url,
    },
    async function(browser) {
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {
          "#user": "notifyu1",
          "#pass": "notifyp1",
        }
      );
      is(fieldValues.username, "notifyu1", "Checking submitted username");
      is(fieldValues.password, "notifyp1", "Checking submitted password");

      let notif = getCaptureDoorhanger("any", PopupNotifications, browser);
      ok(!notif, "got no notification popup");
      if (notif) {
        await cleanupDoorhanger(notif);
      }
    }
  );
  allLogins = Services.logins.getAllLogins();
  is(
    allLogins[0].guid,
    loginGuid,
    "Sanity-check we are comparing the same login record"
  );
  ok(
    allLogins[0].timeLastUsed > timeLastUsed,
    "The timeLastUsed timestamp has been updated"
  );
});

add_task(async function test_private_popup_notification_3b() {
  info(
    "test 3b: run with a login, in private mode," +
      " match existing username/password: no popup notification should appear"
  );

  Services.logins.removeAllLogins();
  Services.logins.addLogin(login);
  let allLogins = Services.logins.getAllLogins();
  // Sanity check the HTTP login exists.
  is(allLogins.length, 1, "Should have the HTTP login");
  let timeLastUsed = allLogins[0].timeLastUsed;
  let loginGuid = allLogins[0].guid;

  await focusWindow(privateWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: privateWin.gBrowser,
      url: form1Url,
    },
    async function(browser) {
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {
          "#user": "notifyu1",
          "#pass": "notifyp1",
        }
      );
      is(fieldValues.username, "notifyu1", "Checking submitted username");
      is(fieldValues.password, "notifyp1", "Checking submitted password");

      let notif = getCaptureDoorhanger("any", PopupNotifications, browser);

      ok(!notif, "got no notification popup");
      if (notif) {
        await cleanupDoorhanger(notif);
      }
    }
  );
  allLogins = Services.logins.getAllLogins();
  is(
    allLogins[0].guid,
    loginGuid,
    "Sanity-check we are comparing the same login record"
  );
  is(
    allLogins[0].timeLastUsed,
    timeLastUsed,
    "The timeLastUsed timestamp has not been updated"
  );
});

add_task(async function test_normal_new_password_4() {
  info(
    "test 4: run with a login, outside of private mode," +
      " add a new password: popup notification should appear"
  );
  Services.logins.removeAllLogins();
  Services.logins.addLogin(login);
  let allLogins = Services.logins.getAllLogins();
  // Sanity check the HTTP login exists.
  is(allLogins.length, 1, "Should have the HTTP login");
  let timeLastUsed = allLogins[0].timeLastUsed;
  let loginGuid = allLogins[0].guid;

  await focusWindow(normalWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: normalWin.gBrowser,
      url: form2Url,
    },
    async function(browser) {
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {
          "#pass": "notifyp1",
          "#newpass": "notifyp2",
        }
      );
      is(fieldValues.password, "notifyp1", "Checking submitted password");
      let notif = getCaptureDoorhanger(
        "password-change",
        PopupNotifications,
        browser
      );
      ok(notif, "got notification popup");
      if (notif) {
        await TestUtils.waitForCondition(
          () => !notif.dismissed,
          "notification should not be dismissed"
        );
        await cleanupDoorhanger(notif);
      }
    }
  );
  // We put up a doorhanger, but didn't interact with it, so we expect the login timestamps
  // to be unchanged
  allLogins = Services.logins.getAllLogins();
  is(
    allLogins[0].guid,
    loginGuid,
    "Sanity-check we are comparing the same login record"
  );
  is(
    allLogins[0].timeLastUsed,
    timeLastUsed,
    "The timeLastUsed timestamp was not updated"
  );
});

add_task(async function test_private_new_password_5() {
  info(
    "test 5: run with a login, in private mode," +
      "add a new password: popup notification should appear"
  );

  const capturePrefValue = Services.prefs.getBoolPref(
    PRIVATE_BROWSING_CAPTURE_PREF
  );
  ok(
    capturePrefValue,
    `Expect ${PRIVATE_BROWSING_CAPTURE_PREF} to default to true`
  );

  let allLogins = Services.logins.getAllLogins();
  // Sanity check the HTTP login exists.
  is(allLogins.length, 1, "Should have the HTTP login");
  let timeLastUsed = allLogins[0].timeLastUsed;
  let loginGuid = allLogins[0].guid;

  await focusWindow(privateWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: privateWin.gBrowser,
      url: form2Url,
    },
    async function(browser) {
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {
          "#pass": "notifyp1",
          "#newpass": "notifyp2",
        }
      );
      is(fieldValues.password, "notifyp1", "Checking submitted password");
      let notif = getCaptureDoorhanger(
        "password-change",
        PopupNotifications,
        browser
      );
      ok(notif, "Expected notification popup");
      if (notif) {
        await TestUtils.waitForCondition(
          () => !notif.dismissed,
          "notification should not be dismissed"
        );
        await cleanupDoorhanger(notif);
      }
    }
  );
  // We put up a doorhanger, but didn't interact with it, so we expect the login timestamps
  // to be unchanged
  allLogins = Services.logins.getAllLogins();
  is(
    allLogins[0].guid,
    loginGuid,
    "Sanity-check we are comparing the same login record"
  );
  is(
    allLogins[0].timeLastUsed,
    timeLastUsed,
    "The timeLastUsed timestamp has not been updated"
  );
});

add_task(async function test_normal_with_login_6() {
  info(
    "test 6: run with a login, outside of private mode, " +
      "submit with an existing password (from test 4): popup notification should appear"
  );

  await focusWindow(normalWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: normalWin.gBrowser,
      url: form2Url,
    },
    async function(browser) {
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {
          "#pass": "notifyp1",
          "#newpass": "notifyp2",
        }
      );
      is(fieldValues.password, "notifyp1", "Checking submitted password");
      let notif = getCaptureDoorhanger(
        "password-change",
        PopupNotifications,
        browser
      );
      ok(notif, "got notification popup");
      if (notif) {
        await TestUtils.waitForCondition(
          () => !notif.dismissed,
          "notification should not be dismissed"
        );
        await cleanupDoorhanger(notif);
      }
      Services.logins.removeLogin(login);
    }
  );
});

add_task(async function test_normal_autofilled_7() {
  info("test 7: verify that the user/pass pair was autofilled");
  Services.logins.addLogin(login);

  // Sanity check the HTTP login exists.
  is(Services.logins.getAllLogins().length, 1, "Should have the HTTP login");

  await focusWindow(normalWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: normalWin.gBrowser,
      url: "about:blank",
    },
    async function(browser) {
      // Add the observer before loading the form page
      let formFilled = listenForTestNotification("FormProcessed");
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      await BrowserTestUtils.loadURI(browser, form1Url);
      await formFilled;

      // the form should have been autofilled, so submit without updating field values
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {}
      );
      is(fieldValues.username, "notifyu1", "Checking submitted username");
      is(fieldValues.password, "notifyp1", "Checking submitted password");
    }
  );
});

add_task(async function test_private_not_autofilled_8() {
  info("test 8: verify that the user/pass pair was not autofilled");
  // Sanity check the HTTP login exists.
  is(Services.logins.getAllLogins().length, 1, "Should have the HTTP login");

  let formFilled = listenForTestNotification("FormProcessed");

  await focusWindow(privateWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: privateWin.gBrowser,
      url: form1Url,
    },
    async function(browser) {
      await formFilled;
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {}
      );
      ok(!fieldValues.username, "Checking submitted username");
      ok(!fieldValues.password, "Checking submitted password");
    }
  );
});

// Disabled for Bug 1523777
// add_task(async function test_private_autocomplete_9() {
//   info("test 9: verify that the user/pass pair was available for autocomplete");
//   // Sanity check the HTTP login exists.
//   is(Services.logins.getAllLogins().length, 1, "Should have the HTTP login");

//   await focusWindow(privateWin);
//   await BrowserTestUtils.withNewTab({
//     gBrowser: privateWin.gBrowser,
//     url: form1Url,
//   }, async function(browser) {
//     let popup = document.getElementById("PopupAutoComplete");
//     ok(popup, "Got popup");

//     let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");

//     // focus the user field. This should trigger the autocomplete menu
//     await ContentTask.spawn(browser, null, async function() {
//       content.document.getElementById("user").focus();
//     });
//     await promiseShown;
//     ok(promiseShown, "autocomplete shown");

//     let promiseFormInput = ContentTask.spawn(browser, null, async function() {
//       let doc = content.document;
//       await new Promise(resolve => {
//         doc.getElementById("form").addEventListener("input", resolve, { once: true });
//       });
//     });
//     info("sending keys");
//     // select the item and hit enter to fill the form
//     await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
//     await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
//     await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
//     await promiseFormInput;

//     let fieldValues = await submitFormAndGetResults(browser, "formsubmit.sjs", {});
//     is(fieldValues.username, "notifyu1", "Checking submitted username");
//     is(fieldValues.password, "notifyp1", "Checking submitted password");
//   });
// });

add_task(async function test_normal_autofilled_10() {
  info(
    "test 10: verify that the user/pass pair does get autofilled in non-private window"
  );
  // Sanity check the HTTP login exists.
  is(Services.logins.getAllLogins().length, 1, "Should have the HTTP login");

  let formFilled = listenForTestNotification("FormProcessed");

  await focusWindow(normalWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: normalWin.gBrowser,
      url: form1Url,
    },
    async function(browser) {
      await formFilled;
      let fieldValues = await submitFormAndGetResults(
        browser,
        "formsubmit.sjs",
        {}
      );
      is(fieldValues.username, "notifyu1", "Checking submitted username");
      is(fieldValues.password, "notifyp1", "Checking submitted password");
    }
  );
});

add_task(async function test_normal_http_basic_auth() {
  info(
    "test normal/basic-auth: verify that we get a doorhanger after basic-auth login"
  );
  Services.logins.removeAllLogins();
  clearHttpAuths();

  await focusWindow(normalWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: normalWin.gBrowser,
      url: "https://example.com",
    },
    async function(browser) {
      await loadAccessRestrictedURL(browser, authUrl, "test", "testpass");
      ok(true, "Auth-required page loaded");

      // verify result in the response document
      let fieldValues = await SpecialPowers.spawn(
        browser,
        [[]],
        async function() {
          let username = content.document.getElementById("user").textContent;
          let password = content.document.getElementById("pass").textContent;
          let ok = content.document.getElementById("ok").textContent;
          return {
            username,
            password,
            ok,
          };
        }
      );
      is(fieldValues.ok, "PASS", "Checking authorization passed");
      is(fieldValues.username, "test", "Checking authorized username");
      is(fieldValues.password, "testpass", "Checking authorized password");

      let notif = getCaptureDoorhanger(
        "password-save",
        PopupNotifications,
        browser
      );
      ok(notif, "got notification popup");
      if (notif) {
        await TestUtils.waitForCondition(
          () => !notif.dismissed,
          "notification should not be dismissed"
        );
        await cleanupDoorhanger(notif);
      }
    }
  );
});

add_task(async function test_private_http_basic_auth() {
  info(
    "test private/basic-auth: verify that we don't get a doorhanger after basic-auth login"
  );
  Services.logins.removeAllLogins();
  clearHttpAuths();

  const capturePrefValue = Services.prefs.getBoolPref(
    PRIVATE_BROWSING_CAPTURE_PREF
  );
  ok(
    capturePrefValue,
    `Expect ${PRIVATE_BROWSING_CAPTURE_PREF} to default to true`
  );

  await focusWindow(privateWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: privateWin.gBrowser,
      url: "https://example.com",
    },
    async function(browser) {
      await loadAccessRestrictedURL(browser, authUrl, "test", "testpass");

      let fieldValues = await getFormSubmitResponseResult(
        browser,
        "authenticate.sjs"
      );
      is(fieldValues.username, "test", "Checking authorized username");
      is(fieldValues.password, "testpass", "Checking authorized password");

      let notif = getCaptureDoorhanger(
        "password-save",
        PopupNotifications,
        browser
      );
      ok(notif, "got notification popup");
      if (notif) {
        await TestUtils.waitForCondition(
          () => notif.dismissed,
          "notification should be dismissed"
        );
        await cleanupDoorhanger(notif);
      }
    }
  );
});

add_task(async function test_private_http_basic_auth_no_capture_pref() {
  info(
    "test private/basic-auth: verify that we don't get a doorhanger after basic-auth login" +
      "with capture pref off"
  );

  const capturePrefValue = Services.prefs.getBoolPref(
    PRIVATE_BROWSING_CAPTURE_PREF
  );
  Services.prefs.setBoolPref(PRIVATE_BROWSING_CAPTURE_PREF, false);

  Services.logins.removeAllLogins();
  clearHttpAuths();

  await focusWindow(privateWin);
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: privateWin.gBrowser,
      url: "https://example.com",
    },
    async function(browser) {
      await loadAccessRestrictedURL(browser, authUrl, "test", "testpass");

      let fieldValues = await getFormSubmitResponseResult(
        browser,
        "authenticate.sjs"
      );
      is(fieldValues.username, "test", "Checking authorized username");
      is(fieldValues.password, "testpass", "Checking authorized password");

      let notif = getCaptureDoorhanger(
        "password-save",
        PopupNotifications,
        browser
      );
      // restore the pref to its original value
      Services.prefs.setBoolPref(
        PRIVATE_BROWSING_CAPTURE_PREF,
        capturePrefValue
      );

      ok(!notif, "got no notification popup");
      if (notif) {
        await cleanupDoorhanger(notif);
      }
    }
  );
});

add_task(async function test_cleanup() {
  await BrowserTestUtils.closeWindow(normalWin);
  await BrowserTestUtils.closeWindow(privateWin);
});
