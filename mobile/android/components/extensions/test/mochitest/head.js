"use strict";

/* exported AppConstants */

var {AppConstants} = SpecialPowers.Cu.import("resource://gre/modules/AppConstants.jsm", {});

{
  let chromeScript = SpecialPowers.loadChromeScript(
    SimpleTest.getTestFileURL("chrome_cleanup_script.js"));

  SimpleTest.registerCleanupFunction(async () => {
    await new Promise(resolve => setTimeout(resolve, 0));

    chromeScript.sendAsyncMessage("check-cleanup");

    let results = await chromeScript.promiseOneMessage("cleanup-results");
    chromeScript.destroy();

    if (results.extraWindows.length || results.extraTabs.length) {
      ok(false, `Test left extra windows or tabs: ${JSON.stringify(results)}\n`);
    }
  });
}
