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

function openPopup(popup, browser) {
  return new Promise(async (resolve) => {
    let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");

    await SimpleTest.promiseFocus(browser);
    info("content window focused");

    // Focus the username field to open the popup.
    await ContentTask.spawn(browser, null, function openAutocomplete() {
      content.document.getElementById("form-basic-username").focus();
    });

    let shown = await promiseShown;
    ok(shown, "autocomplete popup shown");
    resolve(shown);
  });
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

add_task(async function test_autocomplete_footer_onclick() {
  let url = TEST_HOSTNAME + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url,
  }, async function footer_onclick(browser) {
    let popup = document.getElementById("PopupAutoComplete");
    ok(popup, "Got popup");

    await openPopup(popup, browser);

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

add_task(async function test_autocomplete_footer_keydown() {
  let url = TEST_HOSTNAME + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab({
    gBrowser,
    url,
  }, async function footer_enter_keydown(browser) {
    let popup = document.getElementById("PopupAutoComplete");
    ok(popup, "Got popup");

    await openPopup(popup, browser);

    let footer = popup.querySelector(`[originaltype="loginsFooter"]`);
    ok(footer, "Got footer richlistitem");

    await TestUtils.waitForCondition(() => {
      return !EventUtils.isHidden(footer);
    }, "Waiting for footer to become visible");

    await EventUtils.synthesizeKey("KEY_ArrowDown");
    await EventUtils.synthesizeKey("KEY_ArrowDown");
    await EventUtils.synthesizeKey("KEY_ArrowDown");
    await EventUtils.synthesizeKey("KEY_Enter");

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
