/* -*- js-indent-level: 4; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim:set ts=4 sw=4 sts=4 et: */

"use strict";

{
  const chromeScript = SpecialPowers.loadChromeScript(_ => {
    /* eslint-env mozilla/chrome-script */
    const { BrowserTestUtils } = ChromeUtils.importESModule(
      "resource://testing-common/BrowserTestUtils.sys.mjs"
    );

    addMessageListener("anyXULAlertsVisible", () => {
      let windows = Services.wm.getEnumerator("alert:alert");
      return windows.hasMoreElements();
    });

    addMessageListener("waitXULAlertWindowsClosed", () => {
      let windows = Services.wm.getEnumerator("alert:alert");

      let closed = Array.from(windows, win => {
        return BrowserTestUtils.domWindowClosed(win).catch(error => {
          info("Error waiting for alert window closure: " + error.message);
        });
      });

      return Promise.allSettled(closed);
    });
  });

  add_setup(async function setupNoPreexistingAlerts() {
    let alertsVisible = await chromeScript.sendQuery("anyXULAlertsVisible");
    ok(
      !alertsVisible,
      "Alerts should not be present at the start of the test."
    );
  });

  SimpleTest.registerCleanupFunction(async () => {
    // Some tests rely on inspecting the side effect of XUL alerts creating an
    // `alert:alert` window, whose lifetime relative to the XUL alert is an
    // implementation detail. Wait for all alert windows to close to prevent them
    // from incorrectly being inspected in following tests.
    await chromeScript.sendQuery("waitXULAlertWindowsClosed");
  });
}
