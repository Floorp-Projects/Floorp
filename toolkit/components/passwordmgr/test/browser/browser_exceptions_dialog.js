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
  Assert.equal(countDisabledHosts(dialog), 1, "Verify disabled host added");
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
  Assert.equal(countDisabledHosts(dialog), 0, "Verify disabled host removed");
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

  Assert.ok(!btnBlock.hidden, "Block button is visible");
  Assert.ok(btnCookieSession.hidden, "Cookie session button is not visible");
  Assert.ok(btnAllow.hidden, "Allow button is not visible");
  Assert.ok(btnHttpsOnlyOff.hidden, "HTTPS-Only session button is not visible");
  Assert.ok(
    btnHttpsOnlyOffTmp.hidden,
    "HTTPS-Only session button is not visible"
  );
  Assert.ok(btnBlock.disabled, "Block button is initially disabled");
  Assert.ok(
    btnCookieSession.disabled,
    "Cookie session button is initially disabled"
  );
  Assert.ok(btnAllow.disabled, "Allow button is initially disabled");
  Assert.ok(
    btnHttpsOnlyOff.disabled,
    "HTTPS-Only off-button is initially disabled"
  );
  Assert.ok(
    btnHttpsOnlyOffTmp.disabled,
    "HTTPS-Only temporary off-button is initially disabled"
  );

  EventUtils.sendString(LOGIN_HOST, dialog);

  Assert.ok(
    !btnBlock.disabled,
    "Block button is enabled after entering text in the URL input"
  );
  Assert.ok(
    btnCookieSession.disabled,
    "Cookie session button is still disabled after entering text in the URL input"
  );
  Assert.ok(
    btnAllow.disabled,
    "Allow button is still disabled after entering text in the URL input"
  );
  Assert.ok(
    btnHttpsOnlyOff.disabled,
    "HTTPS-Only off-button is still disabled after entering text in the URL input"
  );
  Assert.ok(
    btnHttpsOnlyOffTmp.disabled,
    "HTTPS-Only session off-button is still disabled after entering text in the URL input"
  );

  Assert.equal(
    countDisabledHosts(dialog),
    0,
    "No blocked hosts should be present before hitting the Enter/Return key"
  );
  EventUtils.sendKey("return", dialog);

  Assert.equal(
    countDisabledHosts(dialog),
    1,
    "Verify the blocked host was added"
  );
  Assert.ok(
    btnBlock.disabled,
    "Block button is disabled after submitting to the list"
  );
  await BrowserTestUtils.closeWindow(dialog);
});
