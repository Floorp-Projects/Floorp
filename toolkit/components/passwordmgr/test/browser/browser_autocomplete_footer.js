"use strict";

const TEST_HOSTNAME = "https://example.com";
const BASIC_FORM_PAGE_PATH = DIRECTORY_PATH + "form_basic.html";
const PWMGR_DLG = "chrome://passwordmgr/content/passwordManager.xul";

function loginList() {
  return [
    LoginTestUtils.testData.formLogin({
      hostname: "https://example.com",
      formSubmitURL: "https://example.com",
      username: "username",
      password: "password",
    }),
    LoginTestUtils.testData.formLogin({
      hostname: "https://example.com",
      formSubmitURL: "https://example.com",
      username: "username2",
      password: "password2",
    }),
  ];
}

/**
 * Initialize logins and set prefs needed for the test.
 */
add_task(async function test_initialize() {
  Services.prefs.setBoolPref("signon.showAutoCompleteFooter", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("signon.showAutoCompleteFooter");
  });

  for (let login of loginList()) {
    Services.logins.addLogin(login);
  }
});

add_task(async function test_autocomplete_footer() {
  let url = TEST_HOSTNAME + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url,
  }, async function(browser) {
    let popup = document.getElementById("PopupAutoComplete");
    ok(popup, "Got popup");

    let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");

    await SimpleTest.promiseFocus(browser);
    info("content window focused");

    // Focus the username field to open the popup.
    await ContentTask.spawn(browser, null, function openAutocomplete() {
      content.document.getElementById("form-basic-username").focus();
    });

    await promiseShown;
    ok(promiseShown, "autocomplete shown");

    let footer = popup.querySelector(`[originaltype="loginsFooter"]`);
    ok(footer, "Got footer richlistitem");

    await TestUtils.waitForCondition(() => {
      return !EventUtils.isHidden(footer);
    }, "Waiting for footer to become visible");

    EventUtils.synthesizeMouseAtCenter(footer, {});
    await TestUtils.waitForCondition(() => {
      return Services.wm.getMostRecentWindow("Toolkit:PasswordManager") !== null;
    }, "Waiting for the password manager dialog to open");
    info("Login dialog was opened");

    let window = Services.wm.getMostRecentWindow("Toolkit:PasswordManager");
    await TestUtils.waitForCondition(() => {
      return window.document.getElementById("filter").value == "example.com";
    }, "Waiting for the search string to filter logins");

    window.close();
    popup.hidePopup();
  });
});
