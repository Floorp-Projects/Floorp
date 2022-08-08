/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(10);

const kContentPref = "font.size.variable.x-western";

function countPrefReadsInThread(pref, thread) {
  let count = 0;
  for (let payload of getPayloadsOfType(thread, "PreferenceRead")) {
    if (payload.prefName === pref) {
      count++;
    }
  }
  return count;
}

async function waitForPaintAfterLoad() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return new Promise(function(resolve) {
      function listener() {
        if (content.document.readyState == "complete") {
          content.requestAnimationFrame(() => content.setTimeout(resolve, 0));
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
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  await startProfiler({ features: ["js", "preferencereads"] });

  const url = BASE_URL + "single_frame.html";
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await SpecialPowers.spawn(
      contentBrowser,
      [],
      () => Services.appinfo.processID
    );

    await waitForPaintAfterLoad();

    // Ensure we read a pref in the content process.
    await SpecialPowers.spawn(contentBrowser, [kContentPref], pref => {
      Services.prefs.getIntPref(pref);
    });

    // Check that some PreferenceRead profile markers were generated when the
    // feature is enabled.
    {
      const { contentThread } = await stopProfilerNowAndGetThreads(contentPid);

      Assert.greater(
        countPrefReadsInThread(kContentPref, contentThread),
        0,
        `PreferenceRead profile markers for ${kContentPref} were recorded ` +
          "when the PreferenceRead feature was turned on."
      );
    }

    await startProfiler({ features: ["js"] });
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

    // Ensure we read a pref in the content process.
    await SpecialPowers.spawn(contentBrowser, [kContentPref], pref => {
      Services.prefs.getIntPref(pref);
    });

    // Check that no PreferenceRead markers were recorded when the feature
    // is turned off.
    {
      const {
        parentThread,
        contentThread,
      } = await stopProfilerNowAndGetThreads(contentPid);
      Assert.equal(
        getPayloadsOfType(parentThread, "PreferenceRead").length,
        0,
        "No PreferenceRead profile were recorded " +
          "when the PreferenceRead feature was turned off."
      );

      Assert.equal(
        getPayloadsOfType(contentThread, "PreferenceRead").length,
        0,
        "No PreferenceRead profile were recorded " +
          "when the PreferenceRead feature was turned off."
      );
    }
  });
});
