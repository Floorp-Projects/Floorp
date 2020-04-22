"use strict";

const LOGIN_HOST = "http://example.com";

function openExceptionsDialog() {
  return window.openDialog(
    "chrome://browser/content/preferences/dialogs/permissions.xhtml",
    "Toolkit:PasswordManagerExceptions",
    "",
    {
      blockVisible: true,
      sessionVisible: false,
      allowVisible: false,
      hideStatusColumn: true,
      prefilledHost: "",
      permissionType: "login-saving",
    }
  );
}

function countDisabledHosts(dialog) {
  return dialog.document.getElementById("permissionsBox").itemCount;
}

function promiseStorageChanged(expectedData) {
  function observer(subject, data) {
    return (
      data == expectedData &&
      subject.QueryInterface(Ci.nsISupportsString).data == LOGIN_HOST
    );
  }

  return TestUtils.topicObserved("passwordmgr-storage-changed", observer);
}

add_task(async function test_disable() {
  let dialog = openExceptionsDialog();
  let promiseChanged = promiseStorageChanged("hostSavingDisabled");

  await BrowserTestUtils.waitForEvent(dialog, "load");
  await new Promise(resolve => {
    waitForFocus(resolve, dialog);
  });
  Services.logins.setLoginSavingEnabled(LOGIN_HOST, false);
  await promiseChanged;
  is(countDisabledHosts(dialog), 1, "Verify disabled host added");
  await BrowserTestUtils.closeWindow(dialog);
});

add_task(async function test_enable() {
  let dialog = openExceptionsDialog();
  let promiseChanged = promiseStorageChanged("hostSavingEnabled");

  await BrowserTestUtils.waitForEvent(dialog, "load");
  await new Promise(resolve => {
    waitForFocus(resolve, dialog);
  });
  Services.logins.setLoginSavingEnabled(LOGIN_HOST, true);
  await promiseChanged;
  is(countDisabledHosts(dialog), 0, "Verify disabled host removed");
  await BrowserTestUtils.closeWindow(dialog);
});
