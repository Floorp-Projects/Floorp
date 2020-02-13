"use strict";

const TEST_HOSTNAME = "https://example.com";
const BASIC_FORM_PAGE_PATH = DIRECTORY_PATH + "form_basic.html";

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

      let passwordFilledPromise = LoginHelper.passwordEditCaptureEnabled
        ? listenForTestNotification("PasswordEditedOrGenerated")
        : TestUtils.waitForTick();
      await SpecialPowers.spawn(browser, [], async () => {
        content.document
          .getElementById("form-basic-password")
          .setUserInput("123");
      });
      info("Waiting for passwordFilledPromise");
      await passwordFilledPromise;
      // reset doorhanger/notifications, we're only interested in the submit outcome
      await cleanupDoorhanger();
      await cleanupPasswordNotifications();

      let processedPromise = listenForTestNotification("FormSubmit");
      await SpecialPowers.spawn(browser, [], async () => {
        // fill the username here to avoid the caching in LoginManagerChild that will prevent
        // a duplicate message being sent to the parent process. We are interested in the state
        // of the doorhanger created and don't want a false positive from the password-edited handling
        content.document
          .getElementById("form-basic-username")
          .setUserInput("4111111111111111");
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

      let passwordFilledPromise = LoginHelper.passwordEditCaptureEnabled
        ? listenForTestNotification("PasswordEditedOrGenerated")
        : TestUtils.waitForTick();
      await SpecialPowers.spawn(browser, [], async () => {
        content.document
          .getElementById("form-basic-password")
          .setUserInput("4111111111111111");
        content.document
          .getElementById("form-basic-password")
          .setAttribute("autocomplete", "cc-number");
      });
      await passwordFilledPromise;
      // reset doorhanger/notifications, we're only interested in the submit outcome
      await cleanupDoorhanger();
      await cleanupPasswordNotifications();

      let processedPromise = listenForTestNotification("FormSubmit");
      await SpecialPowers.spawn(browser, [], async () => {
        content.document
          .getElementById("form-basic-username")
          .setUserInput("aaa");
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

      let passwordFilledPromise = LoginHelper.passwordEditCaptureEnabled
        ? listenForTestNotification("PasswordEditedOrGenerated")
        : TestUtils.waitForTick();
      await SpecialPowers.spawn(browser, [], async () => {
        content.document
          .getElementById("form-basic-password")
          .setUserInput("411");
      });
      await passwordFilledPromise;
      // reset doorhanger/notifications, we're only interested in the submit outcome
      await cleanupDoorhanger();
      await cleanupPasswordNotifications();

      let processedPromise = listenForTestNotification("FormSubmit");
      await SpecialPowers.spawn(browser, [], async () => {
        content.document
          .getElementById("form-basic-username")
          .setUserInput("1234123412341234");
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

      let passwordFilledPromise = LoginHelper.passwordEditCaptureEnabled
        ? listenForTestNotification("PasswordEditedOrGenerated")
        : TestUtils.waitForTick();
      await SpecialPowers.spawn(browser, [], async () => {
        content.document
          .getElementById("form-basic-password")
          .setUserInput("111");
        // make a temporary username change to defeat the message caching
        content.document
          .getElementById("form-basic-username")
          .setUserInput("changeduser");
      });
      await passwordFilledPromise;
      // reset doorhanger/notifications, we're only interested in the submit outcome
      await cleanupDoorhanger();
      await cleanupPasswordNotifications();

      let processedPromise = listenForTestNotification("FormSubmit");
      await SpecialPowers.spawn(browser, [], async () => {
        // restore the username
        content.document
          .getElementById("form-basic-username")
          .setUserInput("4111111111111111");
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
