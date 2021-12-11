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

      await openACPopup(popup, browser, "#form-basic-username");

      let footer = popup.querySelector(`[originaltype="loginsFooter"]`);
      ok(footer, "Got footer richlistitem");

      await TestUtils.waitForCondition(() => {
        return !EventUtils.isHidden(footer);
      }, "Waiting for footer to become visible");

      let openingFunc = () => EventUtils.synthesizeMouseAtCenter(footer, {});
      let passwordManager = await openPasswordManager(openingFunc, false);

      info("Password Manager was opened");

      ok(
        !passwordManager.filterValue,
        "Search string should not be set to filter logins"
      );

      // open_management
      await LoginTestUtils.telemetry.waitForEventCount(1);

      // Check event telemetry recorded when opening management UI
      TelemetryTestUtils.assertEvents(
        [["pwmgr", "open_management", "autocomplete"]],
        { category: "pwmgr", method: "open_management" },
        { clear: true, process: "content" }
      );

      await passwordManager.close();
      await closePopup(popup);
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

      await openACPopup(popup, browser, "#form-basic-username");

      let footer = popup.querySelector(`[originaltype="loginsFooter"]`);
      ok(footer, "Got footer richlistitem");

      await TestUtils.waitForCondition(() => {
        return !EventUtils.isHidden(footer);
      }, "Waiting for footer to become visible");

      await EventUtils.synthesizeKey("KEY_ArrowDown");
      await EventUtils.synthesizeKey("KEY_ArrowDown");
      await EventUtils.synthesizeKey("KEY_ArrowDown");
      let openingFunc = () => EventUtils.synthesizeKey("KEY_Enter");

      let passwordManager = await openPasswordManager(openingFunc, false);
      info("Login dialog was opened");

      ok(
        !passwordManager.filterValue,
        "Search string should not be set to filter logins"
      );

      // Check event telemetry recorded when opening management UI
      TelemetryTestUtils.assertEvents(
        [["pwmgr", "open_management", "autocomplete"]],
        { category: "pwmgr", method: "open_management" },
        { clear: true, process: "content" }
      );

      await passwordManager.close();
      await closePopup(popup);
    }
  );
});
