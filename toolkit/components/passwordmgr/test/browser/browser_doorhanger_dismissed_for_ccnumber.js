"use strict";

const TEST_HOSTNAME = "https://example.com";
const BASIC_FORM_PAGE_PATH = DIRECTORY_PATH + "form_basic.html";

function getSubmitMessage() {
  info("getSubmitMessage");
  return new Promise((resolve, reject) => {
    Services.mm.addMessageListener(
      "PasswordManager:onFormSubmit",
      function onFormSubmit() {
        Services.mm.removeMessageListener(
          "PasswordManager:onFormSubmit",
          onFormSubmit
        );
        resolve();
      }
    );
  });
}

add_task(async function test_doorhanger_dismissal_un() {
  let url = TEST_HOSTNAME + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function test_un_value_as_ccnumber(browser) {
      // If the username field has a credit card number and if
      // the password field is a three digit numberic value,
      // we automatically dismiss the save logins prompt on submission.

      let processedPromise = getSubmitMessage();
      ContentTask.spawn(browser, null, async () => {
        content.document
          .getElementById("form-basic-username")
          .setUserInput("4111111111111111");
        content.document
          .getElementById("form-basic-password")
          .setUserInput("123");

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

add_task(async function test_doorhanger_dismissal_pw() {
  let url = TEST_HOSTNAME + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function test_pw_value_as_ccnumber(browser) {
      // If the password field has a credit card number and if
      // the password field is also tagged autocomplete="cc-number",
      // we automatically dismiss the save logins prompt on submission.

      let processedPromise = getSubmitMessage();
      ContentTask.spawn(browser, null, async () => {
        content.document
          .getElementById("form-basic-username")
          .setUserInput("aaa");
        content.document
          .getElementById("form-basic-password")
          .setUserInput("4111111111111111");
        content.document
          .getElementById("form-basic-password")
          .setAttribute("autocomplete", "cc-number");

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
  let url = TEST_HOSTNAME + BASIC_FORM_PAGE_PATH;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function test_un_with_invalid_cc_number(browser) {
      // If the username field has a CC number that is invalid,
      // we show the doorhanger to save logins like we usually do.

      let processedPromise = getSubmitMessage();
      ContentTask.spawn(browser, null, async () => {
        content.document.getElementById("form-basic-username").value =
          "1234123412341234";
        content.document.getElementById("form-basic-password").value = "411";

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
  let url = TEST_HOSTNAME + BASIC_FORM_PAGE_PATH;
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
        "https://example.org",
        "https://example.org",
        null,
        "4111111111111111",
        "111",
        "form-basic-username",
        "form-basic-password"
      );
      Services.logins.addLogin(login);

      let processedPromise = getSubmitMessage();
      ContentTask.spawn(browser, null, async () => {
        content.document
          .getElementById("form-basic-password")
          .setUserInput("111");
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
