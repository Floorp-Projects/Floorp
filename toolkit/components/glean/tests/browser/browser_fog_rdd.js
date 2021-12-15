/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Glean's here on `window`, but eslint doesn't know that. bug 1715542.
/* global Glean:false */

/**
 * Return a web-based URL for a given file based on the testing directory.
 * @param {String} fileName
 *        file that caller wants its web-based url
 */
function GetTestWebBasedURL(fileName) {
  return (
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "http://example.org"
    ) + fileName
  );
}

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["media.rdd-process.enabled", true]],
  });

  let url = GetTestWebBasedURL("small-shot.ogg");
  info(
    `Opening ${url} in a new tab to trigger the creation of the RDD process`
  );
  let tab = BrowserTestUtils.addTab(gBrowser, url);

  await TestUtils.waitForCondition(
    async () =>
      (await ChromeUtils.requestProcInfo()).children.some(p => p.type == "rdd"),
    "waiting to find RDD process."
  );

  Services.fog.testResetFOG();

  is(
    undefined,
    Glean.testOnlyIpc.aCounter.testGetValue(),
    "Ensure we begin without value."
  );

  await TestUtils.waitForCondition(async () => {
    try {
      await Services.fog.testTriggerRDDMetrics();
      return true;
    } catch (e) {
      return false;
    }
  }, "waiting until we can successfully send the TestTriggerMetrics IPC.");

  await Services.fog.testFlushAllChildren();

  is(
    45327, // See dom/media/ipc/RDDParent.cpp's RecvTestTriggerMetrics().
    Glean.testOnlyIpc.aCounter.testGetValue(),
    "Ensure the RDD-process-set value shows up in the parent process."
  );

  BrowserTestUtils.removeTab(tab);
});
