/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test the JS Allocations feature. This is done as a browser test to ensure that
 * we realistically try out how the JS allocations are running. This ensures that
 * we are collecting allocations for the content process and the parent process.
 */
add_task(async function test_profile_feature_jsallocations() {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfiler({ features: ["threads", "js", "jsallocations"] });

  const url = BASE_URL + "do_work_500ms.html";
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await ContentTask.spawn(
      contentBrowser,
      null,
      () => Services.appinfo.processID
    );

    // Wait 500ms so that the tab finishes executing.
    await wait(500);

    // Check that we can get some allocations when the feature is turned on.
    {
      const { parentThread, contentThread } = await stopProfilerAndGetThreads(
        contentPid
      );
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

    // Flush out any straggling allocation markers that may have not been collected
    // yet by starting and stopping the profiler once.
    startProfiler({ features: ["threads", "js"] });
    await stopProfilerAndGetThreads(contentPid);

    // Now reload the tab with a clean run.
    gBrowser.reload();
    await wait(500);
    startProfiler({ features: ["threads", "js"] });

    // Check that no allocations were recorded, and allocation tracking was correctly
    // turned off.
    {
      const { parentThread, contentThread } = await stopProfilerAndGetThreads(
        contentPid
      );
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

/**
 * Markers are collected only after a periodic sample. This function ensures that
 * at least one periodic sample has been done.
 */
async function doAtLeastOnePeriodicSample() {
  async function getProfileSampleCount() {
    const profile = await Services.profiler.getProfileDataAsync();
    return profile.threads[0].samples.data.length;
  }

  const sampleCount = await getProfileSampleCount();
  // Create an infinite loop until a sample has been collected.
  while (true) {
    if (sampleCount < (await getProfileSampleCount())) {
      return;
    }
  }
}

async function stopProfilerAndGetThreads(contentPid) {
  await doAtLeastOnePeriodicSample();

  const profile = await Services.profiler.getProfileDataAsync();
  Services.profiler.StopProfiler();

  const parentThread = profile.threads[0];
  const contentProcess = profile.processes.find(
    p => p.threads[0].pid == contentPid
  );
  if (!contentProcess) {
    throw new Error("Could not find the content process.");
  }
  const contentThread = contentProcess.threads[0];

  if (!parentThread) {
    throw new Error("The parent thread was not found in the profile.");
  }

  if (!contentThread) {
    throw new Error("The content thread was not found in the profile.");
  }

  return { parentThread, contentThread };
}
