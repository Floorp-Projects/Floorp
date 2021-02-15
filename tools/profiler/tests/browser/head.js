/* import-globals-from ../shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/tools/profiler/tests/browser/shared-head.js",
  this
);

const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

const BASE_URL = "http://example.com/browser/tools/profiler/tests/browser/";

registerCleanupFunction(() => {
  if (Services.profiler.IsActive()) {
    info(
      "The profiler was found to still be running at the end of the test, which means that some error likely occured. Let's stop it to prevent issues with following tests!"
    );
    Services.profiler.StopProfiler();
  }
});

/**
 * This is a helper function that will stop the profiler of the browser running
 * with PID contentPid.
 * This happens immediately, without waiting for any sampling to happen or
 * finish. Use stopProfilerAndGetThreads (without "Now") below instead to wait
 * for samples before stopping.
 *
 * @param {number} contentPid
 * @returns {Promise}
 */
async function stopProfilerNowAndGetThreads(contentPid) {
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

/**
 * This is a helper function that will stop the profiler of the browser running
 * with PID contentPid.
 * As opposed to stopProfilerNowAndGetThreads (with "Now") above, the profiler
 * in that PID will not stop until there is at least one periodic sample taken.
 *
 * @param {number} contentPid
 * @returns {Promise}
 */
async function stopProfilerAndGetThreads(contentPid) {
  await Services.profiler.waitOnePeriodicSampling();

  return stopProfilerNowAndGetThreads(contentPid);
}
