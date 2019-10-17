/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function countIPCMessagesInThread(thread) {
  let count = 0;
  for (let payload of getPayloadsOfType(thread, "PreferenceRead")) {
    if (payload.prefName === "layout.css.dpi") {
      count++;
    }
  }
  return count;
}

/**
 * Test the IPCMessage feature.
 */
add_task(async function test_profile_feature_ipcmessges() {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }
  Assert.ok(
    !Services.profiler.IsActive(),
    "The profiler is not currently active"
  );

  startProfiler({ features: ["threads", "ipcmessages"] });

  const url = BASE_URL + "fixed_height.html";
  await BrowserTestUtils.withNewTab(url, async contentBrowser => {
    const contentPid = await ContentTask.spawn(
      contentBrowser,
      null,
      () => Services.appinfo.processID
    );

    info("Wait 100ms so that the tab finishes executing.");
    await wait(100);

    info(
      "Check that some PreferenceRead profile markers were generated when " +
        "the feature is enabled."
    );
    {
      const { parentThread, contentThread } = await stopProfilerAndGetThreads(
        contentPid
      );

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

    startProfiler({ features: ["threads"] });
    info("Now reload the tab with a clean run.");
    gBrowser.reload();
    await wait(100);

    info(
      "Check that no PreferenceRead markers were recorded when the " +
        "feature is turned off."
    );
    {
      const { parentThread, contentThread } = await stopProfilerAndGetThreads(
        contentPid
      );
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
