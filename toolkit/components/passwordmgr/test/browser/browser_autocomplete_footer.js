"use strict";

const TEST_ORIGIN = "https://example.com";
const BASIC_FORM_PAGE_PATH = DIRECTORY_PATH + "form_basic.html";

function loginList() {
  return [
    LoginTestUtils.testData.formLogin({
      origin: "https://example.com",
      formActionOrigin: "https://example.com",
      username: "username",
      password: "password",
    }),
    LoginTestUtils.testData.formLogin({
      origin: "https://example.com",
      formActionOrigin: "https://example.com",
      username: "username2",
      password: "password2",
    }),
  ];
}

async function openPopup(popup, browser) {
  let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");

  await SimpleTest.promiseFocus(browser);
  info("content window focused");

  // Focus the username field to open the popup.
  await ContentTask.spawn(browser, null, function openAutocomplete() {
    content.document.getElementById("form-basic-username").focus();
  });

  let shown = await promiseShown;
  ok(shown, "autocomplete popup shown");
  return shown;
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
  let url = TEST_ORIGIN + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function footer_onclick(browser) {
      let popup = document.getElementById("PopupAutoComplete");
      ok(popup, "Got popup");

      await openPopup(popup, browser);

      let footer = popup.querySelector(`[originaltype="loginsFooter"]`);
      ok(footer, "Got footer richlistitem");

      await TestUtils.waitForCondition(() => {
        return !EventUtils.isHidden(footer);
      }, "Waiting for footer to become visible");

      let openingFunc = () => EventUtils.synthesizeMouseAtCenter(footer, {});
      let passwordManager = await openPasswordManager(openingFunc, true);

      info("Password Manager was opened");

      is(
        passwordManager.filterValue,
        "example.com",
        "Search string should be set to filter logins"
      );

      // Check event telemetry recorded when opening management UI
      TelemetryTestUtils.assertEvents(
        [["pwmgr", "open_management", "autocomplete"]],
        { category: "pwmgr", method: "open_management" }
      );

      await passwordManager.close();
      popup.hidePopup();
    }
  );
});

add_task(async function test_autocomplete_footer_keydown() {
  let url = TEST_ORIGIN + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function footer_enter_keydown(browser) {
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
      let openingFunc = () => EventUtils.synthesizeKey("KEY_Enter");

      let passwordManager = await openPasswordManager(openingFunc, true);
      info("Login dialog was opened");

      is(
        passwordManager.filterValue,
        "example.com",
        "Search string should be set to filter logins"
      );

      // Check event telemetry recorded when opening management UI
      TelemetryTestUtils.assertEvents(
        [["pwmgr", "open_management", "autocomplete"]],
        { category: "pwmgr", method: "open_management" }
      );

      await passwordManager.close();
      popup.hidePopup();
    }
  );
});
