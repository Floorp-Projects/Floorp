/* -*- js-indent-level: 4; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim:set ts=4 sw=4 sts=4 et: */

"use strict";

{
  const chromeScript = SpecialPowers.loadChromeScript(_ => {
    /* eslint-env mozilla/chrome-script */
    addMessageListener("anyXULAlertsVisible", () => {
      let windows = Services.wm.getEnumerator("alert:alert");
      return windows.hasMoreElements();
    });
  });

  add_setup(async function setupNoPreexistingAlerts() {
    let alertsVisible = await chromeScript.sendQuery("anyXULAlertsVisible");
    ok(
      !alertsVisible,
      "Alerts should not be present at the start of the test."
    );
  });
}
