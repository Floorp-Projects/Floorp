/**
 * Replacing a dismissed doorhanger with a visible one while it's opening.
 *
 * There are various races between popup notification callbacks to catch with this.
 * This can happen in the real world by blurring an edited login field by clicking on the login doorhanger.
 */

XPCOMUtils.defineLazyServiceGetter(
  this,
  "prompterSvc",
  "@mozilla.org/login-manager/prompter;1",
  Ci.nsILoginManagerPrompter
);

add_task(async function test_replaceDismissedWithVisibleWhileOpening() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com",
    },
    async function load(browser) {
      info("Show a dismissed save doorhanger");
      prompterSvc.promptToSavePassword(
        browser,
        LoginTestUtils.testData.formLogin({}),
        true,
        false,
        null
      );
      let doorhanger = await waitForDoorhanger(browser, "password-save");
      ok(doorhanger, "Got doorhanger");
      EventUtils.synthesizeMouseAtCenter(doorhanger.anchorElement, {});
      await BrowserTestUtils.waitForEvent(
        PopupNotifications.panel,
        "popupshowing"
      );
      await checkDoorhangerUsernamePassword("the username", "the password");
      info(
        "Replace the doorhanger with a non-dismissed one immediately after clicking to open"
      );
      prompterSvc.promptToSavePassword(
        browser,
        LoginTestUtils.testData.formLogin({}),
        true,
        false,
        null
      );
      await Promise.race([
        BrowserTestUtils.waitForCondition(() => {
          if (
            document.getElementById("password-notification-username").value !=
              "the username" ||
            document.getElementById("password-notification-password").value !=
              "the password"
          ) {
            return Promise.reject("Field changed to incorrect values");
          }
          return false;
        }, "See if username/password values change to incorrect values"),
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        new Promise(resolve => setTimeout(resolve, 1000)),
      ]);
    }
  );
});
