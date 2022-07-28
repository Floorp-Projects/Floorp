/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(10);

/**
 * Test the JS Allocations feature. This is done as a browser test to ensure that
 * we realistically try out how the JS allocations are running. This ensures that
 * we are collecting allocations for the content process and the parent process.
 */
add_task(async function test_profile_feature_jsallocations() {
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  await startProfiler({ features: ["js", "jsallocations"] });

  const url = BASE_URL + "do_work_500ms.html";
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await SpecialPowers.spawn(
      contentBrowser,
      [],
      () => Services.appinfo.processID
    );

    // Wait 500ms so that the tab finishes executing.
    await wait(500);

    // Check that we can get some allocations when the feature is turned on.
    {
      const {
        parentThread,
        contentThread,
      } = await waitSamplingAndStopProfilerAndGetThreads(contentPid);
      Assert.greater(
        getPayloadsOfType(parentThread, "JS allocation").length,
        0,
        "Allocations were recorded for the parent process' main thread when the " +
          "JS Allocation feature was turned on."
      );
      Assert.greater(
        getPayloadsOfType(contentThread, "JS allocation").length,
        0,
        "Allocations were recorded for the content process' main thread when the " +
          "JS Allocation feature was turned on."
      );
    }

    await startProfiler({ features: ["js"] });
    // Now reload the tab with a clean run.
    gBrowser.reload();
    await wait(500);

    // Check that no allocations were recorded, and allocation tracking was correctly
    // turned off.
    {
      const {
        parentThread,
        contentThread,
      } = await waitSamplingAndStopProfilerAndGetThreads(contentPid);
      Assert.equal(
        getPayloadsOfType(parentThread, "JS allocation").length,
        0,
        "No allocations were recorded for the parent processes' main thread when " +
          "JS allocation was not turned on."
      );

      Assert.equal(
        getPayloadsOfType(contentThread, "JS allocation").length,
        0,
        "No allocations were recorded for the content processes' main thread when " +
          "JS allocation was not turned on."
      );
    }
  });
});
