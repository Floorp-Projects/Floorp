"use strict";

const EXPECTED_SUPPORT_URL = Services.urlFormatter.formatURLPref("app.support.baseURL") +
                             "insecure-password";

add_task(function* test_clickInsecureFieldWarning() {
  let url = "https://example.com" + DIRECTORY_PATH + "form_cross_origin_insecure_action.html";

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url,
  }, function*(browser) {
    let popup = document.getElementById("PopupAutoComplete");
    ok(popup, "Got popup");

    let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");

    // Focus the username field to open the popup.
    yield ContentTask.spawn(browser, null, function openAutocomplete() {
      content.document.getElementById("form-basic-username").focus();
    });

    yield promiseShown;
    ok(promiseShown, "autocomplete shown");

    let warningItem = document.getAnonymousElementByAttribute(popup, "type", "insecureWarning");
    ok(warningItem, "Got warning richlistitem");

    yield BrowserTestUtils.waitForCondition(() => !warningItem.collapsed, "Wait for warning to show");

    info("Clicking on warning");
    let supportTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, EXPECTED_SUPPORT_URL);
    EventUtils.synthesizeMouseAtCenter(warningItem, {});
    let supportTab = yield supportTabPromise;
    ok(supportTab, "Support tab opened");
    yield BrowserTestUtils.removeTab(supportTab);
  });
});
