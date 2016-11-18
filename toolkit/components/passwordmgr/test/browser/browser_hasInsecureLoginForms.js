/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/LoginManagerParent.jsm", this);

const testUrlPath =
      "://example.com/browser/toolkit/components/passwordmgr/test/browser/";

/**
 * Waits for the given number of occurrences of InsecureLoginFormsStateChange
 * on the given browser element.
 */
function waitForInsecureLoginFormsStateChange(browser, count) {
  return BrowserTestUtils.waitForEvent(browser, "InsecureLoginFormsStateChange",
                                       false, () => --count == 0);
}

/**
 * Checks that hasInsecureLoginForms is true for a simple HTTP page and false
 * for a simple HTTPS page.
 */
add_task(function* test_simple() {
  for (let scheme of ["http", "https"]) {
    let tab = gBrowser.addTab(scheme + testUrlPath + "form_basic.html");
    let browser = tab.linkedBrowser;
    yield Promise.all([
      BrowserTestUtils.switchTab(gBrowser, tab),
      BrowserTestUtils.browserLoaded(browser),
      // One event is triggered by pageshow and one by DOMFormHasPassword.
      waitForInsecureLoginFormsStateChange(browser, 2),
    ]);

    Assert.equal(LoginManagerParent.hasInsecureLoginForms(browser),
                 scheme == "http");

    gBrowser.removeTab(tab);
  }
});

/**
 * Checks that hasInsecureLoginForms is true if a password field is present in
 * an HTTP page loaded as a subframe of a top-level HTTPS page, when mixed
 * active content blocking is disabled.
 *
 * When the subframe is navigated to an HTTPS page, hasInsecureLoginForms should
 * be set to false.
 *
 * Moving back in history should set hasInsecureLoginForms to true again.
 */
add_task(function* test_subframe_navigation() {
  yield SpecialPowers.pushPrefEnv({
    "set": [["security.mixed_content.block_active_content", false]],
  });

  // Load the page with the subframe in a new tab.
  let tab = gBrowser.addTab("https" + testUrlPath + "insecure_test.html");
  let browser = tab.linkedBrowser;
  yield Promise.all([
    BrowserTestUtils.switchTab(gBrowser, tab),
    BrowserTestUtils.browserLoaded(browser),
    // Two events are triggered by pageshow and one by DOMFormHasPassword.
    waitForInsecureLoginFormsStateChange(browser, 3),
  ]);

  Assert.ok(LoginManagerParent.hasInsecureLoginForms(browser));

  // Navigate the subframe to a secure page.
  let promiseSubframeReady = Promise.all([
    BrowserTestUtils.browserLoaded(browser, true),
    // One event is triggered by pageshow and one by DOMFormHasPassword.
    waitForInsecureLoginFormsStateChange(browser, 2),
  ]);
  yield ContentTask.spawn(browser, null, function* () {
    content.document.getElementById("test-iframe")
           .contentDocument.getElementById("test-link").click();
  });
  yield promiseSubframeReady;

  Assert.ok(!LoginManagerParent.hasInsecureLoginForms(browser));

  // Navigate back to the insecure page. We only have to wait for the
  // InsecureLoginFormsStateChange event that is triggered by pageshow.
  let promise = waitForInsecureLoginFormsStateChange(browser, 1);
  yield ContentTask.spawn(browser, null, function* () {
    content.document.getElementById("test-iframe")
           .contentWindow.history.back();
  });
  yield promise;

  Assert.ok(LoginManagerParent.hasInsecureLoginForms(browser));

  gBrowser.removeTab(tab);
});
