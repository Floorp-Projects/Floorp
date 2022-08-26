/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function ProcessHasSamplerThread(process) {
  return process.threads.some(t => t.name == "SamplerThread");
}

async function GetPidsWithSamplerThread() {
  let parentProc = await ChromeUtils.requestProcInfo();

  let pids = parentProc.children
    .filter(ProcessHasSamplerThread)
    .map(proc => proc.pid);
  if (ProcessHasSamplerThread(parentProc)) {
    pids.unshift(parentProc.pid);
  }
  return pids;
}

// fnFilterWithContentId: Called with content child pid, returns filters to use.
// E.g.: 123 => ["GeckoMain", "pid:123"], or 123 => ["pid:456"].
async function test_with_filter(fnFilterWithContentId) {
  Assert.ok(!Services.profiler.IsActive());
  info("Clear the previous pages just in case we still some open tabs.");
  await Services.profiler.ClearAllPages();

  info("Open a tab with single_frame.html in it.");
  const url = BASE_URL + "single_frame.html";
  return BrowserTestUtils.withNewTab(url, async function(contentBrowser) {
    const contentPid = await SpecialPowers.spawn(contentBrowser, [], () => {
      return Services.appinfo.processID;
    });

    Assert.deepEqual(
      await GetPidsWithSamplerThread(),
      [],
      "There should be no SamplerThreads before starting the profiler"
    );

    info("Start the profiler to test filters including 'pid:<content>'.");
    await startProfiler({ threads: fnFilterWithContentId(contentPid) });

    let pidsWithSamplerThread = null;
    await TestUtils.waitForCondition(
      async function() {
        let pidsStringBefore = JSON.stringify(pidsWithSamplerThread);
        pidsWithSamplerThread = await GetPidsWithSamplerThread();
        return JSON.stringify(pidsWithSamplerThread) == pidsStringBefore;
      },
      "Wait for sampler threads to stabilize after profiler start",
      /* interval (ms) */ 250,
      /* maxTries */ 10
    );

    info("Capture the profile data.");
    const profile = await waitSamplingAndStopAndGetProfile();

    await TestUtils.waitForCondition(async function() {
      return !(await GetPidsWithSamplerThread()).length;
    }, "Wait for all sampler threads to stop after profiler stop");

    return { contentPid, pidsWithSamplerThread, profile };
  });
}

add_task(async function browser_test_profile_capture_along_with_content_pid() {
  const {
    contentPid,
    pidsWithSamplerThread,
    profile,
  } = await test_with_filter(contentPid => ["GeckoMain", "pid:" + contentPid]);

  Assert.greater(
    pidsWithSamplerThread.length,
    2,
    "There should be lots of SamplerThreads after starting the profiler"
  );

  let contentProcessIndex = profile.processes.findIndex(
    p => p.threads[0].pid == contentPid
  );
  Assert.notEqual(
    contentProcessIndex,
    -1,
    "The content process should be present"
  );

  // Note: Some threads may not be registered, so we can't expect that many. But
  // 10 is much more than the default 4.
  Assert.greater(
    profile.processes[contentProcessIndex].threads.length,
    10,
    "The content process should have many threads"
  );

  Assert.equal(
    profile.threads.length,
    1,
    "The parent process should have only one thread"
  );
  Assert.equal(
    profile.threads[0].name,
    "GeckoMain",
    "The parent process should have the main thread"
  );
});

add_task(async function browser_test_profile_capture_along_with_other_pid() {
  const parentPid = Services.appinfo.processID;
  const {
    contentPid,
    pidsWithSamplerThread,
    profile,
  } = await test_with_filter(contentPid => ["GeckoMain", "pid:" + parentPid]);

  Assert.greater(
    pidsWithSamplerThread.length,
    2,
    "There should be lots of SamplerThreads after starting the profiler"
  );

  let contentProcessIndex = profile.processes.findIndex(
    p => p.threads[0].pid == contentPid
  );
  Assert.notEqual(
    contentProcessIndex,
    -1,
    "The content process should be present"
  );

  Assert.equal(
    profile.processes[contentProcessIndex].threads.length,
    1,
    "The content process should have only one thread"
  );

  // Note: Some threads may not be registered, so we can't expect that many. But
  // 10 is much more than the default 4.
  Assert.greater(
    profile.threads.length,
    10,
    "The parent process should have many threads"
  );
});

add_task(async function browser_test_profile_capture_by_only_content_pid() {
  const parentPid = Services.appinfo.processID;
  const {
    contentPid,
    pidsWithSamplerThread,
    profile,
  } = await test_with_filter(contentPid => ["pid:" + contentPid]);

  // The sampler thread always runs in the parent process, see bug 1754100.
  Assert.deepEqual(
    pidsWithSamplerThread,
    [parentPid, contentPid],
    "There should only be SamplerThreads in the parent and the target child"
  );

  Assert.equal(
    profile.processes.length,
    1,
    "There should only be one child process"
  );
  // Note: Some threads may not be registered, so we can't expect that many. But
  // 10 is much more than the default 4.
  Assert.greater(
    profile.processes[0].threads.length,
    10,
    "The child process should have many threads"
  );
  Assert.equal(
    profile.processes[0].threads[0].pid,
    contentPid,
    "The only child process should be our content"
  );
});

add_task(async function browser_test_profile_capture_by_only_parent_pid() {
  const parentPid = Services.appinfo.processID;
  const {
    pidsWithSamplerThread,
    profile,
  } = await test_with_filter(contentPid => ["pid:" + parentPid]);

  Assert.deepEqual(
    pidsWithSamplerThread,
    [parentPid],
    "There should only be a SamplerThread in the parent"
  );

  // Note: Some threads may not be registered, so we can't expect that many. But
  // 10 is much more than the default 4.
  Assert.greater(
    profile.threads.length,
    10,
    "The parent process should have many threads"
  );
  Assert.equal(
    profile.processes.length,
    0,
    "There should be no child processes"
  );
});
