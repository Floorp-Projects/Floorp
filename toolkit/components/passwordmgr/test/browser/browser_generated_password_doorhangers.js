/**
 * Test using the generated passwords produces the right doorhangers/notifications
 */

/* eslint no-shadow:"off" */

"use strict";

// The origin for the test URIs.
const TEST_ORIGIN = "https://example.com";
const FORM_PAGE_PATH =
  "/browser/toolkit/components/passwordmgr/test/browser/form_basic.html";
const passwordInputSelector = "#form-basic-password";

let login1;
async function addOneLogin() {
  login1 = LoginTestUtils.testData.formLogin({
    origin: "https://example.com",
    formActionOrigin: "https://example.com",
    username: "username",
    password: "pass1",
  });
  let storageChangedPromised = TestUtils.topicObserved(
    "passwordmgr-storage-changed",
    (_, data) => data == "addLogin"
  );
  Services.logins.addLogin(login1);
  await storageChangedPromised;
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["signon.generation.available", true],
      ["signon.generation.enabled", true],
    ],
  });
  // assert that there are no logins
  let logins = Services.logins.getAllLogins();
  is(logins.length, 0, "There are no logins");

  // use a single matching login for the following tests
  await addOneLogin();
});

add_task(async function contextfill_generated_password_with_matching_logins() {
  // test that we can fill a generated password when there are matching logins
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN + FORM_PAGE_PATH,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      await ContentTask.spawn(
        browser,
        [passwordInputSelector],
        async function waitForFilledFieldValue(inputSelector) {
          let passwordInput = content.document.querySelector(inputSelector);
          await ContentTaskUtils.waitForCondition(
            () => passwordInput.value == "pass1",
            "Password field got autofilled value"
          );
        }
      );
      await doFillGeneratedPasswordContextMenuItem(
        browser,
        passwordInputSelector
      );
      await ContentTask.spawn(
        browser,
        [passwordInputSelector],
        function checkFinalFieldValue(inputSelector) {
          is(
            content.document.querySelector(inputSelector).value.length,
            15,
            "Password field was filled with generated password"
          );
        }
      );
      // check a dismissed prompt was shown
      let notif = getCaptureDoorhanger("password-save");
      let { panel } = PopupNotifications;
      ok(notif && notif.dismissed, "Dismissed notification was created");

      info("Clicking on anchor to show popup.");
      let promiseShown = BrowserTestUtils.waitForEvent(panel, "popupshown");
      notif.anchorElement.click();
      await promiseShown;

      let notificationElement = panel.childNodes[0];
      let passwordTextbox = notificationElement.querySelector(
        "#password-notification-password"
      );
      is(
        passwordTextbox.value.length,
        15,
        "Doorhanger password field has generated 15-char value"
      );
      notif.remove();
    }
  );
});
