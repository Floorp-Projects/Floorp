/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test the native allocations feature. This is done as a browser test to ensure
 * that we realistically try out how the native allocations are running. This
 * ensures that we are collecting allocations for the content process and the
 * parent process.
 */
add_task(async function test_profile_feature_nativeallocations() {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  info("Start the profiler to test the native allocations.");
  startProfiler({
    features: ["threads", "leaf", "nativeallocations"],
  });

  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  if (env.get("XPCOM_MEM_BLOAT_LOG")) {
    info("Native allocations do not currently work with the bloat log.");
    // Stop the profiler and exit. Note that this happens after we have already
    // turned on the profiler so that we can still test this code path.
    Services.profiler.StopProfiler();
    return;
  }

  info("Launch a new tab to kick off a content process.");

  const url = BASE_URL + "do_work_500ms.html";
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await ContentTask.spawn(
      contentBrowser,
      null,
      () => Services.appinfo.processID
    );

    info("Wait 100ms so that we know some work has been done.");
    await wait(100);

    info(
      "Check that we can get some allocations when the feature is turned on."
    );
    {
      const { parentThread, contentThread } = await stopProfilerAndGetThreads(
        contentPid
      );
      Assert.greater(
        getPayloadsOfType(parentThread, "Native allocation").length,
        0,
        "Allocations were recorded for the parent process' main thread when the " +
          "Native Allocation feature was turned on."
      );
      Assert.greater(
        getPayloadsOfType(contentThread, "Native allocation").length,
        0,
        "Allocations were recorded for the content process' main thread when the " +
          "Native Allocation feature was turned on."
      );
    }

    info(
      "Flush out any straggling allocation markers that may have not been " +
        "collected yet by starting and stopping the profiler once."
    );
    startProfiler({ features: ["threads", "leaf"] });
    await stopProfilerAndGetThreads(contentPid);

    info("Now reload the tab with a clean run.");
    gBrowser.reload();
    await wait(100);
    startProfiler({ features: ["threads", "leaf"] });

    info(
      "Check that no allocations were recorded, and allocation tracking was " +
        "correctly turned off."
    );

    {
      const { parentThread, contentThread } = await stopProfilerAndGetThreads(
        contentPid
      );
      Assert.equal(
        getPayloadsOfType(parentThread, "Native allocation").length,
        0,
        "No allocations were recorded for the parent processes' main thread when " +
          "Native allocation was not turned on."
      );

      Assert.equal(
        getPayloadsOfType(contentThread, "Native allocation").length,
        0,
        "No allocations were recorded for the content processes' main thread when " +
          "Native allocation was not turned on."
      );
    }
  });
});
