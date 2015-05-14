/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://testing-common/LoginTestUtils.jsm", this);

/**
 * All these tests require the experimental login fill UI to be enabled. We also
 * disable autofill for login forms for easier testing of manual fill.
 */
add_task(function* test_initialize() {
  Services.prefs.setBoolPref("signon.ui.experimental", true);
  Services.prefs.setBoolPref("signon.autofillForms", false);
  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("signon.ui.experimental");
    Services.prefs.clearUserPref("signon.autofillForms");
  });
});

/**
 * Tests manual fill when the page has a login form.
 */
add_task(function* test_fill() {
  Services.logins.addLogin(LoginTestUtils.testData.formLogin({
    hostname: "https://example.com",
    formSubmitURL: "https://example.com",
    username: "username",
    password: "password",
  }));

  // The anchor icon may be shown during the initial page load in the new tab,
  // so we have to set up the observers first. When we receive the notification
  // from PopupNotifications.jsm, we check it is the one for the right anchor.
  let anchor = document.getElementById("login-fill-notification-icon");
  let promiseAnchorShown =
      TestUtils.topicObserved("PopupNotifications-updateNotShowing",
                              () => anchor.hasAttribute("showing"));

  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "https://example.com/browser/toolkit/components/" +
         "passwordmgr/test/browser/form_basic.html",
  }, function* (browser) {
    yield promiseAnchorShown;

    let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                     "Shown");
    anchor.click();
    yield promiseShown;

    let list = document.getElementById("login-fill-list");
    Assert.equal(list.childNodes.length, 1,
                 "list.childNodes.length === 1");

    let promiseHidden = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                      "popuphidden");
    list.focus();
    EventUtils.sendMouseEvent({ type: "dblclick" }, list.childNodes[0]);
    yield promiseHidden;

    let result = yield ContentTask.spawn(browser, null, function* () {
      let doc = content.document;
      return {
        username: doc.getElementById("form-basic-username").value,
        password: doc.getElementById("form-basic-password").value,
      }
    });

    Assert.equal(result.username, "username",
                 'result.username === "username"');
    Assert.equal(result.password, "password",
                 'result.password === "password"');
  });

  Services.logins.removeAllLogins();
});
