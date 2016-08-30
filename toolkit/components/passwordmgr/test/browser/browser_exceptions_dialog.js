
"use strict";

const LOGIN_HOST = "http://example.com";

function openExceptionsDialog() {
  return window.openDialog(
    "chrome://browser/content/preferences/permissions.xul",
    "Toolkit:PasswordManagerExceptions", "",
    {
      blockVisible: true,
      sessionVisible: false,
      allowVisible: false,
      hideStatusColumn: true,
      prefilledHost: "",
      permissionType: "login-saving"
    }
  );
}

function countDisabledHosts(dialog) {
  let doc = dialog.document;
  let rejectsTree = doc.getElementById("permissionsTree");

  return rejectsTree.view.rowCount;
}

function promiseStorageChanged(expectedData) {
  function observer(subject, data) {
    return data == expectedData && subject.QueryInterface(Ci.nsISupportsString).data == LOGIN_HOST;
  }

  return TestUtils.topicObserved("passwordmgr-storage-changed", observer);
}

add_task(function* test_disable() {
  let dialog = openExceptionsDialog();
  let promiseChanged = promiseStorageChanged("hostSavingDisabled");

  yield BrowserTestUtils.waitForEvent(dialog, "load");
  Services.logins.setLoginSavingEnabled(LOGIN_HOST, false);
  yield promiseChanged;
  is(countDisabledHosts(dialog), 1, "Verify disabled host added");
  yield BrowserTestUtils.closeWindow(dialog);
});

add_task(function* test_enable() {
  let dialog = openExceptionsDialog();
  let promiseChanged = promiseStorageChanged("hostSavingEnabled");

  yield BrowserTestUtils.waitForEvent(dialog, "load");
  Services.logins.setLoginSavingEnabled(LOGIN_HOST, true);
  yield promiseChanged;
  is(countDisabledHosts(dialog), 0, "Verify disabled host removed");
  yield BrowserTestUtils.closeWindow(dialog);
});
