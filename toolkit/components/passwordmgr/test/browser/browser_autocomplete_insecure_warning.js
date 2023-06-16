"use strict";

const EXPECTED_SUPPORT_URL =
  Services.urlFormatter.formatURLPref("app.support.baseURL") +
  "insecure-password";

add_task(async function test_clickInsecureFieldWarning() {
  let url =
    "https://example.com" +
    DIRECTORY_PATH +
    "form_cross_origin_insecure_action.html";

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url,
    },
    async function (browser) {
      let popup = document.getElementById("PopupAutoComplete");
      Assert.ok(popup, "Got popup");
      await openACPopup(popup, browser, "#form-basic-username");
      await new Promise(requestAnimationFrame);

      let warningItem = popup.querySelector(`[type="insecureWarning"]`);
      Assert.ok(warningItem, "Got warning richlistitem");

      await BrowserTestUtils.waitForCondition(
        () => !warningItem.collapsed,
        "Wait for warning to show"
      );

      info("Clicking on warning");
      let supportTabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        EXPECTED_SUPPORT_URL
      );
      EventUtils.synthesizeMouseAtCenter(warningItem, {});
      let supportTab = await supportTabPromise;
      Assert.ok(supportTab, "Support tab opened");
      await closePopup(popup);
      BrowserTestUtils.removeTab(supportTab);
    }
  );
});
