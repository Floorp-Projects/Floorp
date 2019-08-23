/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function countDpiPrefReadsInThread(thread) {
  let count = 0;
  for (let payload of getPayloadsOfType(thread, "PreferenceRead")) {
    if (payload.prefName === "layout.css.dpi") {
      count++;
    }
  }
  return count;
}

/**
 * Test the PreferenceRead feature.
 */
add_task(async function test_profile_feature_preferencereads() {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfiler({ features: ["threads", "preferencereads"] });

  const url = BASE_URL + "fixed_height.html";
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await ContentTask.spawn(
      contentBrowser,
      null,
      () => Services.appinfo.processID
    );

    // Wait 100ms so that the tab finishes executing.
    await wait(100);

    // Check that some PreferenceRead profile markers were generated when the
    // feature is enabled.
    {
      const { contentThread } = await stopProfilerAndGetThreads(contentPid);

      const timesReadDpiInContent = countDpiPrefReadsInThread(contentThread);

      Assert.greater(
        timesReadDpiInContent,
        0,
        "PreferenceRead profile markers for layout.css.dpi were recorded " +
          "when the PreferenceRead feature was turned on."
      );
    }

    startProfiler({ features: ["threads"] });
    // Now reload the tab with a clean run.
    gBrowser.reload();
    await wait(100);

    // Check that no PreferenceRead markers were recorded when the feature
    // is turned off.
    {
      const { parentThread, contentThread } = await stopProfilerAndGetThreads(
        contentPid
      );
      Assert.equal(
        getPayloadsOfType(parentThread, "PreferenceRead").length,
        0,
        "No PreferenceRead profile markers for layout.css.dpi were recorded " +
          "when the PreferenceRead feature was turned on."
      );

      Assert.equal(
        getPayloadsOfType(contentThread, "PreferenceRead").length,
        0,
        "No PreferenceRead profile markers for layout.css.dpi were recorded " +
          "when the PreferenceRead feature was turned on."
      );
    }
  });
});
