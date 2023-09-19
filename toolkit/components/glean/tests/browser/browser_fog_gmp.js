/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Return a web-based URL for a given file based on the testing directory.
 * @param {String} fileName
 *        file that caller wants its web-based url
 */
function GetTestWebBasedURL(fileName) {
  return (
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "https://example.org"
    ) + fileName
  );
}

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["media.eme.enabled", true]],
  });

  await BrowserTestUtils.withNewTab(
    GetTestWebBasedURL("empty_file.html"),
    async function (browser) {
      await SpecialPowers.spawn(browser, [], async function () {
        try {
          let config = [
            {
              initDataTypes: ["webm"],
              videoCapabilities: [{ contentType: 'video/webm; codecs="vp9"' }],
            },
          ];
          let access = await content.navigator.requestMediaKeySystemAccess(
            "org.w3.clearkey",
            config
          );

          content.mediaKeys = await access.createMediaKeys();
          info("got media keys, which should ensure a GMP process exists");
        } catch (ex) {
          ok(false, ex.toString());
        }
      });

      ok(
        (await ChromeUtils.requestProcInfo()).children.some(
          p => p.type == "gmpPlugin"
        ),
        "Found the GMP process."
      );

      Services.fog.testResetFOG();

      is(
        null,
        Glean.testOnlyIpc.aCounter.testGetValue(),
        "Ensure we begin without value."
      );

      await TestUtils.waitForCondition(async () => {
        try {
          await Services.fog.testTriggerMetrics(
            Ci.nsIXULRuntime.PROCESS_TYPE_GMPLUGIN
          );
          return true;
        } catch (e) {
          info(e);
          return false;
        }
      }, "waiting until we can successfully send the TestTriggerMetrics IPC.");

      await Services.fog.testFlushAllChildren();

      is(
        Glean.testOnlyIpc.aCounter.testGetValue(),
        Ci.nsIXULRuntime.PROCESS_TYPE_GMPLUGIN,
        "Ensure the GMP-process-set value shows up in the parent process."
      );
    }
  );
});
