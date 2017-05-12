const HOST = "https://example.com";
const URL = HOST + "/browser/toolkit/components/passwordmgr/test/browser/form_basic.html";
const TIMEOUT_PREF = "signon.masterPasswordReprompt.timeout_ms";

// Waits for the master password prompt and cancels it.
function waitForDialog() {
  let dialogShown = TestUtils.topicObserved("common-dialog-loaded");
  return dialogShown.then(function([subject]) {
    let dialog = subject.Dialog;
    is(dialog.args.title, "Password Required");
    dialog.ui.button1.click();
  });
}

// Test that autocomplete does not trigger a master password prompt
// for a certain time after it was cancelled.
add_task(async function test_mpAutocompleteTimeout() {
  let login = LoginTestUtils.testData.formLogin({
    hostname: "https://example.com",
    formSubmitURL: "https://example.com",
    username: "username",
    password: "password",
  });
  Services.logins.addLogin(login);
  LoginTestUtils.masterPassword.enable();

  registerCleanupFunction(function() {
    LoginTestUtils.masterPassword.disable();
    Services.logins.removeAllLogins();
  });

  // Set master password prompt timeout to 3s.
  // If this test goes intermittent, you likely have to increase this value.
  await SpecialPowers.pushPrefEnv({set: [[TIMEOUT_PREF, 3000]]});

  // Wait for initial master password dialog after opening the tab.
  let dialogShown = waitForDialog();

  await BrowserTestUtils.withNewTab(URL, async function(browser) {
    await dialogShown;

    await ContentTask.spawn(browser, null, async function() {
      // Focus the password field to trigger autocompletion.
      content.document.getElementById("form-basic-password").focus();
    });

    // Wait 4s, dialog should not have been shown
    // (otherwise the code below will not work).
    await new Promise((c) => setTimeout(c, 4000));

    dialogShown = waitForDialog();
    await ContentTask.spawn(browser, null, async function() {
      // Re-focus the password field to trigger autocompletion.
      content.document.getElementById("form-basic-username").focus();
      content.document.getElementById("form-basic-password").focus();
    });
    await dialogShown;
  });
});
