/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* test_save() {
  let tab = gBrowser.addTab("https://example.com/browser/toolkit/components/" +
                            "passwordmgr/test/browser/form_basic.html");
  let browser = tab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);
  gBrowser.selectedTab = tab;

  let promiseShown = BrowserTestUtils.waitForEvent(PopupNotifications.panel,
                                                   "Shown");
  yield ContentTask.spawn(browser, null, function* () {
    content.document.getElementById("form-basic-username").value = "username";
    content.document.getElementById("form-basic-password").value = "password";
    content.document.getElementById("form-basic").submit();
  });
  yield promiseShown;
  let notificationElement = PopupNotifications.panel.childNodes[0];

  let promiseLogin = TestUtils.topicObserved("passwordmgr-storage-changed",
                                             (_, data) => data == "addLogin");
  notificationElement.button.doCommand();
  let [login] = yield promiseLogin;
  login.QueryInterface(Ci.nsILoginInfo);

  Assert.equal(login.username, "username");
  Assert.equal(login.password, "password");

  // Cleanup.
  Services.logins.removeAllLogins();
  gBrowser.removeTab(tab);
});
