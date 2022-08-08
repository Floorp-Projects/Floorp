/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(10);

async function waitForLoad() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return new Promise(function(resolve) {
      if (content.document.readyState !== "complete") {
        content.document.addEventListener("readystatechange", () => {
          if (content.document.readyState === "complete") {
            resolve();
          }
        });
      } else {
        resolve();
      }
    });
  });
}

/**
 * Test the IPCMessages feature.
 */
add_task(async function test_profile_feature_ipcmessges() {
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  const url = BASE_URL + "simple.html";

  info("Open a tab while profiling IPC messages.");
  await startProfiler({ features: ["js", "ipcmessages"] });
  info("Started the profiler sucessfully! Now, let's open a tab.");

  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    info("We opened a tab!");
    const contentPid = await SpecialPowers.spawn(
      contentBrowser,
      [],
      () => Services.appinfo.processID
    );
    info("Now let's wait until it's fully loaded.");
    await waitForLoad();

    info(
      "Check that some IPC profile markers were generated when " +
        "the feature is enabled."
    );
    {
      const {
        parentThread,
        contentThread,
      } = await waitSamplingAndStopProfilerAndGetThreads(contentPid);

      Assert.greater(
        getPayloadsOfType(parentThread, "IPC").length,
        0,
        "IPC profile markers were recorded for the parent process' main " +
          "thread when the IPCMessages feature was turned on."
      );

      Assert.greater(
        getPayloadsOfType(contentThread, "IPC").length,
        0,
        "IPC profile markers were recorded for the content process' main " +
          "thread when the IPCMessages feature was turned on."
      );
    }
  });

  info("Now open a tab without profiling IPC messages.");
  await startProfiler({ features: ["js"] });

  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await SpecialPowers.spawn(
      contentBrowser,
      [],
      () => Services.appinfo.processID
    );
    await waitForLoad();

    info(
      "Check that no IPC profile markers were recorded when the " +
        "feature is turned off."
    );
    {
      const {
        parentThread,
        contentThread,
      } = await waitSamplingAndStopProfilerAndGetThreads(contentPid);
      Assert.equal(
        getPayloadsOfType(parentThread, "IPC").length,
        0,
        "No IPC profile markers were recorded for the parent process' main " +
          "thread when the IPCMessages feature was turned off."
      );

      Assert.equal(
        getPayloadsOfType(contentThread, "IPC").length,
        0,
        "No IPC profile markers were recorded for the content process' main " +
          "thread when the IPCMessages feature was turned off."
      );
    }
  });
});
