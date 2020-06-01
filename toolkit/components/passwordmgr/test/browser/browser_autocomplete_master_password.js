/* eslint-disable mozilla/no-arbitrary-setTimeout */
const HOST = "https://example.com";
const URL =
  HOST + "/browser/toolkit/components/passwordmgr/test/browser/form_basic.html";
const TIMEOUT_PREF = "signon.masterPasswordReprompt.timeout_ms";

const BRAND_BUNDLE = Services.strings.createBundle(
  "chrome://branding/locale/brand.properties"
);
const BRAND_FULL_NAME = BRAND_BUNDLE.GetStringFromName("brandFullName");

// Waits for the master password prompt and cancels it when close() is called on the return value.
async function waitForDialog() {
  let [subject] = await TestUtils.topicObserved("common-dialog-loaded");
  let dialog = subject.Dialog;
  let expected = "Password Required - " + BRAND_FULL_NAME;
  is(dialog.args.title, expected, "Check common dialog title");
  return {
    async close(win = window) {
      dialog.ui.button1.click();
      return BrowserTestUtils.waitForEvent(win, "DOMModalDialogClosed");
    },
  };
}

add_task(async function setup() {
  let login = LoginTestUtils.testData.formLogin({
    origin: "https://example.com",
    formActionOrigin: "https://example.com",
    username: "username",
    password: "password",
  });
  Services.logins.addLogin(login);
  LoginTestUtils.masterPassword.enable();

  registerCleanupFunction(function() {
    LoginTestUtils.masterPassword.disable();
  });

  // Set master password prompt timeout to 3s.
  // If this test goes intermittent, you likely have to increase this value.
  await SpecialPowers.pushPrefEnv({ set: [[TIMEOUT_PREF, 3000]] });
});

// Test that autocomplete does not trigger a master password prompt
// for a certain time after it was cancelled.
add_task(async function test_mpAutocompleteTimeout() {
  // Wait for initial master password dialog after opening the tab.
  let dialogShown = waitForDialog();

  await BrowserTestUtils.withNewTab(URL, async function(browser) {
    (await dialogShown).close();

    await SpecialPowers.spawn(browser, [], async function() {
      // Focus the password field to trigger autocompletion.
      content.document.getElementById("form-basic-password").focus();
    });

    // Wait 4s, dialog should not have been shown
    // (otherwise the code below will not work).
    await new Promise(c => setTimeout(c, 4000));

    dialogShown = waitForDialog();
    await SpecialPowers.spawn(browser, [], async function() {
      // Re-focus the password field to trigger autocompletion.
      content.document.getElementById("form-basic-username").focus();
      content.document.getElementById("form-basic-password").focus();
    });
    (await dialogShown).close();
    closePopup(document.getElementById("PopupAutoComplete"));
  });

  // Wait 4s for the timer to pass again and not interfere with the next test.
  await new Promise(c => setTimeout(c, 4000));
});

// Test that autocomplete does not trigger a master password prompt
// if one is already showing.
add_task(async function test_mpAutocompleteUIBusy() {
  // Wait for initial master password dialog after adding the login.
  let dialogShown = waitForDialog();

  let win = await BrowserTestUtils.openNewBrowserWindow();

  Services.tm.dispatchToMainThread(() => {
    try {
      // Trigger a MP prompt in the new window by saving a login
      Services.logins.addLogin(LoginTestUtils.testData.formLogin());
    } catch (e) {
      // Handle throwing from MP cancellation
    }
  });
  let { close } = await dialogShown;

  let windowGlobal =
    gBrowser.selectedBrowser.browsingContext.currentWindowGlobal;
  let loginManagerParent = windowGlobal.getActor("LoginManager");
  let origin = "https://www.example.com";
  let data = {
    actionOrigin: "",
    searchString: "",
    previousResult: null,
    hasBeenTypePassword: true,
    isSecure: false,
    isProbablyANewPasswordField: true,
  };

  function dialogObserver(subject, topic, data) {
    ok(false, "A second dialog shouldn't have been shown");
    Services.obs.removeObserver(dialogObserver, topic);
  }
  Services.obs.addObserver(dialogObserver, "common-dialog-loaded");

  let results = await loginManagerParent.doAutocompleteSearch(origin, data);
  is(results.logins.length, 0, "No results since uiBusy is true");
  await close(win);

  await BrowserTestUtils.closeWindow(win);

  Services.obs.removeObserver(dialogObserver, "common-dialog-loaded");
});
