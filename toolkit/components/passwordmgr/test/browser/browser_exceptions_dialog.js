"use strict";

const LOGIN_HOST = "https://example.com";

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

add_task(async function test_block_button_with_enter_key() {
  // Test ensures that the Enter/Return key does not activate the "Allow" button
  // in the "Saved Logins" exceptions dialog

  let dialog = openExceptionsDialog();

  await BrowserTestUtils.waitForEvent(dialog, "load");
  await new Promise(resolve => waitForFocus(resolve, dialog));
  let btnBlock = dialog.document.getElementById("btnBlock");
  let btnCookieSession = dialog.document.getElementById("btnCookieSession");
  let btnHttpsOnlyOff = dialog.document.getElementById("btnHttpsOnlyOff");
  let btnHttpsOnlyOffTmp = dialog.document.getElementById("btnHttpsOnlyOffTmp");
  let btnAllow = dialog.document.getElementById("btnAllow");

  ok(!btnBlock.hidden, "Block button is visible");
  ok(btnCookieSession.hidden, "Cookie session button is not visible");
  ok(btnAllow.hidden, "Allow button is not visible");
  ok(btnHttpsOnlyOff.hidden, "HTTPS-Only session button is not visible");
  ok(btnHttpsOnlyOffTmp.hidden, "HTTPS-Only session button is not visible");
  ok(btnBlock.disabled, "Block button is initially disabled");
  ok(btnCookieSession.disabled, "Cookie session button is initially disabled");
  ok(btnAllow.disabled, "Allow button is initially disabled");
  ok(btnHttpsOnlyOff.disabled, "HTTPS-Only off-button is initially disabled");
  ok(
    btnHttpsOnlyOffTmp.disabled,
    "HTTPS-Only temporary off-button is initially disabled"
  );

  EventUtils.sendString(LOGIN_HOST, dialog);

  ok(
    !btnBlock.disabled,
    "Block button is enabled after entering text in the URL input"
  );
  ok(
    btnCookieSession.disabled,
    "Cookie session button is still disabled after entering text in the URL input"
  );
  ok(
    btnAllow.disabled,
    "Allow button is still disabled after entering text in the URL input"
  );
  ok(
    btnHttpsOnlyOff.disabled,
    "HTTPS-Only off-button is still disabled after entering text in the URL input"
  );
  ok(
    btnHttpsOnlyOffTmp.disabled,
    "HTTPS-Only session off-button is still disabled after entering text in the URL input"
  );

  is(
    countDisabledHosts(dialog),
    0,
    "No blocked hosts should be present before hitting the Enter/Return key"
  );
  EventUtils.sendKey("return", dialog);

  is(countDisabledHosts(dialog), 1, "Verify the blocked host was added");
  ok(
    btnBlock.disabled,
    "Block button is disabled after submitting to the list"
  );
  await BrowserTestUtils.closeWindow(dialog);
});
