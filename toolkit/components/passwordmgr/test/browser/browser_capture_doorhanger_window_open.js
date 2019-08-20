/*
 * Test capture popup notifications in content opened by window.open
 */

let nsLoginInfo = new Components.Constructor(
  "@mozilla.org/login-manager/loginInfo;1",
  Ci.nsILoginInfo,
  "init"
);
let login1 = new nsLoginInfo(
  "http://mochi.test:8888",
  "http://mochi.test:8888",
  null,
  "notifyu1",
  "notifyp1",
  "user",
  "pass"
);
let login2 = new nsLoginInfo(
  "http://mochi.test:8888",
  "http://mochi.test:8888",
  null,
  "notifyu2",
  "notifyp2",
  "user",
  "pass"
);

function withTestTabUntilStorageChange(aPageFile, aTaskFn) {
  function storageChangedObserved(subject, data) {
    // Watch for actions triggered from a doorhanger (not cleanup tasks with removeLogin)
    if (data == "removeLogin") {
      return false;
    }
    return true;
  }

  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    storageChangedObserved
  );
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "http://mochi.test:8888" + DIRECTORY_PATH + aPageFile,
    },
    async function(browser) {
      ok(true, "loaded " + aPageFile);
      info("running test case task");
      await aTaskFn();
      info("waiting for storage change");
      await storageChangedPromised;
    }
  );
}

add_task(async function setup() {
  await SimpleTest.promiseFocus(window);
});

add_task(async function test_saveChromeHiddenAutoClose() {
  let notifShownPromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  // query arguments are: username, password, features, auto-close (delimited by '|')
  let url =
    "subtst_notifications_11.html?notifyu1|notifyp1|" +
    "menubar=no,toolbar=no,location=no|autoclose";
  await withTestTabUntilStorageChange(url, async function() {
    info("waiting for popupshown");
    await notifShownPromise;
    // the popup closes and the doorhanger should appear in the opener
    let popup = getCaptureDoorhanger("password-save");
    ok(popup, "got notification popup");
    await checkDoorhangerUsernamePassword("notifyu1", "notifyp1");
    // Sanity check, no logins should exist yet.
    let logins = Services.logins.getAllLogins();
    is(logins.length, 0, "Should not have any logins yet");

    clickDoorhangerButton(popup, REMEMBER_BUTTON);
  });
  // Check result of clicking Remember
  let logins = Services.logins.getAllLogins();
  is(logins.length, 1, "Should only have 1 login");
  let login = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  is(login.timesUsed, 1, "Check times used on new entry");
  is(login.username, "notifyu1", "Check the username used on the new entry");
  is(login.password, "notifyp1", "Check the password used on the new entry");
});

add_task(async function test_changeChromeHiddenAutoClose() {
  let notifShownPromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  let url =
    "subtst_notifications_11.html?notifyu1|pass2|menubar=no,toolbar=no,location=no|autoclose";
  await withTestTabUntilStorageChange(url, async function() {
    info("waiting for popupshown");
    await notifShownPromise;
    let popup = getCaptureDoorhanger("password-change");
    ok(popup, "got notification popup");
    await checkDoorhangerUsernamePassword("notifyu1", "pass2");
    clickDoorhangerButton(popup, CHANGE_BUTTON);
  });

  // Check to make sure we updated the password, timestamps and use count for
  // the login being changed with this form.
  let logins = Services.logins.getAllLogins();
  is(logins.length, 1, "Should only have 1 login");
  let login = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  is(login.username, "notifyu1", "Check the username");
  is(login.password, "pass2", "Check password changed");
  is(login.timesUsed, 2, "check .timesUsed incremented on change");
  ok(login.timeCreated < login.timeLastUsed, "timeLastUsed bumped");
  ok(
    login.timeLastUsed == login.timePasswordChanged,
    "timeUsed == timeChanged"
  );

  login1.password = "pass2";
  Services.logins.removeLogin(login1);
  login1.password = "notifyp1";
});

add_task(async function test_saveChromeVisibleSameWindow() {
  // This test actually opens a new tab in the same window with default browser settings.
  let url = "subtst_notifications_11.html?notifyu2|notifyp2||";
  let notifShownPromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  await withTestTabUntilStorageChange(url, async function() {
    await notifShownPromise;
    let popup = getCaptureDoorhanger("password-save");
    ok(popup, "got notification popup");
    await checkDoorhangerUsernamePassword("notifyu2", "notifyp2");
    clickDoorhangerButton(popup, REMEMBER_BUTTON);
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });

  // Check result of clicking Remember
  let logins = Services.logins.getAllLogins();
  is(logins.length, 1, "Should only have 1 login now");
  let login = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  is(login.username, "notifyu2", "Check the username used on the new entry");
  is(login.password, "notifyp2", "Check the password used on the new entry");
  is(login.timesUsed, 1, "Check times used on new entry");
});

add_task(async function test_changeChromeVisibleSameWindow() {
  let url = "subtst_notifications_11.html?notifyu2|pass2||";
  let notifShownPromise = BrowserTestUtils.waitForEvent(
    PopupNotifications.panel,
    "popupshown"
  );
  await withTestTabUntilStorageChange(url, async function() {
    await notifShownPromise;
    let popup = getCaptureDoorhanger("password-change");
    ok(popup, "got notification popup");
    await checkDoorhangerUsernamePassword("notifyu2", "pass2");
    clickDoorhangerButton(popup, CHANGE_BUTTON);
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });

  // Check to make sure we updated the password, timestamps and use count for
  // the login being changed with this form.
  let logins = Services.logins.getAllLogins();
  is(logins.length, 1, "Should have 1 login");
  let login = logins[0].QueryInterface(Ci.nsILoginMetaInfo);
  is(login.username, "notifyu2", "Check the username");
  is(login.password, "pass2", "Check password changed");
  is(login.timesUsed, 2, "check .timesUsed incremented on change");
  ok(login.timeCreated < login.timeLastUsed, "timeLastUsed bumped");
  ok(
    login.timeLastUsed == login.timePasswordChanged,
    "timeUsed == timeChanged"
  );

  // cleanup
  login2.password = "pass2";
  Services.logins.removeLogin(login2);
  login2.password = "notifyp2";
});
