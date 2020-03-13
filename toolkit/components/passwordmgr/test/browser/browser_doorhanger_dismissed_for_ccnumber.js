"use strict";

const TEST_ORIGIN = "https://example.com";
const BASIC_FORM_PAGE_PATH = DIRECTORY_PATH + "form_basic.html";

add_task(async function test_doorhanger_dismissal_un() {
  let url = TEST_ORIGIN + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function test_un_value_as_ccnumber(browser) {
      // If the username field has a credit card number and if
      // the password field is a three digit numberic value,
      // we automatically dismiss the save logins prompt on submission.

      await changeContentFormValues(browser, {
        "#form-basic-password": "123",
        // We are interested in the state of the doorhanger created and don't want a
        // false positive from the password-edited handling
        "#form-basic-username": "4111111111111111",
      });

      let processedPromise = listenForTestNotification("FormSubmit");
      await SpecialPowers.spawn(browser, [], async () => {
        content.document.getElementById("form-basic-submit").click();
      });
      info("Waiting for FormSubmit");
      await processedPromise;

      let notif = getCaptureDoorhanger("password-save");
      ok(notif, "got notification popup");
      ok(notif.dismissed, "notification popup was automatically dismissed");
      await cleanupDoorhanger(notif);
    }
  );
});

add_task(async function test_doorhanger_dismissal_pw() {
  let url = TEST_ORIGIN + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function test_pw_value_as_ccnumber(browser) {
      // If the password field has a credit card number and if
      // the password field is also tagged autocomplete="cc-number",
      // we automatically dismiss the save logins prompt on submission.

      await changeContentFormValues(browser, {
        "#form-basic-password": "4111111111111111",
        "#form-basic-username": "aaa",
      });
      await SpecialPowers.spawn(browser, [], async () => {
        content.document
          .getElementById("form-basic-password")
          .setAttribute("autocomplete", "cc-number");
      });

      let processedPromise = listenForTestNotification("FormSubmit");
      await SpecialPowers.spawn(browser, [], async () => {
        content.document.getElementById("form-basic-submit").click();
      });
      await processedPromise;

      let notif = getCaptureDoorhanger("password-save");
      ok(notif, "got notification popup");
      ok(notif.dismissed, "notification popup was automatically dismissed");
      await cleanupDoorhanger(notif);
    }
  );
});

add_task(async function test_doorhanger_shown_on_un_with_invalid_ccnumber() {
  let url = TEST_ORIGIN + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function test_un_with_invalid_cc_number(browser) {
      // If the username field has a CC number that is invalid,
      // we show the doorhanger to save logins like we usually do.

      await changeContentFormValues(browser, {
        "#form-basic-password": "411",
        "#form-basic-username": "1234123412341234",
      });

      let processedPromise = listenForTestNotification("FormSubmit");
      await SpecialPowers.spawn(browser, [], async () => {
        content.document.getElementById("form-basic-submit").click();
      });
      await processedPromise;

      let notif = await getCaptureDoorhangerThatMayOpen("password-save");
      ok(notif, "got notification popup");
      ok(
        !notif.dismissed,
        "notification popup was not automatically dismissed"
      );
      await cleanupDoorhanger(notif);
    }
  );
});

add_task(async function test_doorhanger_dismissal_on_change() {
  let url = TEST_ORIGIN + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function test_change_in_pw(browser) {
      let nsLoginInfo = new Components.Constructor(
        "@mozilla.org/login-manager/loginInfo;1",
        Ci.nsILoginInfo,
        "init"
      );
      let login = new nsLoginInfo(
        TEST_ORIGIN,
        TEST_ORIGIN,
        null,
        "4111111111111111",
        "111", // password looks like a card security code
        "form-basic-username",
        "form-basic-password"
      );
      Services.logins.addLogin(login);

      await changeContentFormValues(browser, {
        "#form-basic-password": "222", // password looks like a card security code
        "#form-basic-username": "4111111111111111",
      });

      let processedPromise = listenForTestNotification("FormSubmit");
      await SpecialPowers.spawn(browser, [], async () => {
        content.document.getElementById("form-basic-submit").click();
      });
      await processedPromise;

      let notif = getCaptureDoorhanger("password-change");
      ok(notif, "got notification popup");
      ok(notif.dismissed, "notification popup was automatically dismissed");
      await cleanupDoorhanger(notif);
    }
  );
});
