/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(10);

function countDpiPrefReadsInThread(thread) {
  let count = 0;
  for (let payload of getPayloadsOfType(thread, "PreferenceRead")) {
    if (payload.prefName === "layout.css.dpi") {
      count++;
    }
  }
  return count;
}

async function waitForPaintAfterLoad() {
  return ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    return new Promise(function(resolve) {
      function listener() {
        if (content.document.readyState == "complete") {
          content.requestAnimationFrame(() => setTimeout(resolve, 0));
        }
      }
      if (content.document.readyState != "complete") {
        content.document.addEventListener("readystatechange", listener);
      } else {
        listener();
      }
    });
  });
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

    await waitForPaintAfterLoad();

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
    await ContentTask.spawn(contentBrowser, null, () => {
      return new Promise(resolve => {
        addEventListener("pageshow", () => resolve(), {
          capturing: true,
          once: true,
        });
        content.location.reload();
      });
    });

    await waitForPaintAfterLoad();

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
