/**
 * Test "passwordmgr-form-submission-detected" should be notified
 * regardless of whehter the password saving is enabled.
 */

async function waitForFormSubmissionDetected() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subject, topic) {
      Services.obs.removeObserver(
        observer,
        "passwordmgr-form-submission-detected"
      );
      resolve();
    }, "passwordmgr-form-submission-detected");
  });
}

add_task(async function test_login_save_disable() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.rememberSignons", false]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        "https://example.com/browser/toolkit/components/" +
        "passwordmgr/test/browser/form_basic.html",
    },
    async function (browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      await changeContentFormValues(browser, {
        "#form-basic-username": "username",
        "#form-basic-password": "password",
      });

      let promise = waitForFormSubmissionDetected();
      await SpecialPowers.spawn(browser, [], async function () {
        let doc = this.content.document;
        doc.getElementById("form-basic").submit();
      });

      await promise;
      Assert.ok(true, "Test completed");
    }
  );
});

add_task(async function test_login_save_enable() {
  await SpecialPowers.pushPrefEnv({
    set: [["signon.rememberSignons", true]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url:
        "https://example.com/browser/toolkit/components/" +
        "passwordmgr/test/browser/form_basic.html",
    },
    async function (browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);

      await changeContentFormValues(browser, {
        "#form-basic-username": "username",
        "#form-basic-password": "password",
      });

      // When login saving is enabled, we should receive both FormSubmit
      // event and "passwordmgr-form-submission-detected" event
      let p1 = waitForFormSubmissionDetected();
      let p2 = listenForTestNotification("ShowDoorhanger");
      await SpecialPowers.spawn(browser, [], async function () {
        let doc = this.content.document;
        doc.getElementById("form-basic").submit();
      });

      await Promise.all([p1, p2]);
      Assert.ok(true, "Test completed");
    }
  );
});
