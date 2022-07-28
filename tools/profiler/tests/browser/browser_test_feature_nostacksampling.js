/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test the No Stack Sampling feature.
 */
add_task(async function test_profile_feature_nostacksampling() {
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  await startProfiler({ features: ["js", "nostacksampling"] });

  const url = BASE_URL + "do_work_500ms.html";
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await SpecialPowers.spawn(
      contentBrowser,
      [],
      () => Services.appinfo.processID
    );

    // Wait 500ms so that the tab finishes executing.
    await wait(500);

    // Check that we can get no stacks when the feature is turned on.
    {
      const {
        parentThread,
        contentThread,
      } = await stopProfilerNowAndGetThreads(contentPid);
      Assert.equal(
        parentThread.samples.data.length,
        0,
        "Stack samples were recorded from the parent process' main thread" +
          "when the No Stack Sampling feature was turned on."
      );
      Assert.equal(
        contentThread.samples.data.length,
        0,
        "Stack samples were recorded from the content process' main thread" +
          "when the No Stack Sampling feature was turned on."
      );
    }

    // Flush out any straggling allocation markers that may have not been collected
    // yet by starting and stopping the profiler once.
    await startProfiler({ features: ["js"] });

    // Now reload the tab with a clean run.
    gBrowser.reload();
    await wait(500);

    // Check that stack samples were recorded.
    {
      const {
        parentThread,
        contentThread,
      } = await waitSamplingAndStopProfilerAndGetThreads(contentPid);
      Assert.greater(
        parentThread.samples.data.length,
        0,
        "No Stack samples were recorded from the parent process' main thread" +
          "when the No Stack Sampling feature was not turned on."
      );

      Assert.greater(
        contentThread.samples.data.length,
        0,
        "No Stack samples were recorded from the content process' main thread" +
          "when the No Stack Sampling feature was not turned on."
      );
    }
  });
});
