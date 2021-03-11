/* import-globals-from ../shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/tools/profiler/tests/browser/shared-head.js",
  this
);

const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

const BASE_URL = "http://example.com/browser/tools/profiler/tests/browser/";
const BASE_URL_HTTPS =
  "https://example.com/browser/tools/profiler/tests/browser/";

registerCleanupFunction(() => {
  if (Services.profiler.IsActive()) {
    info(
      "The profiler was found to still be running at the end of the test, which means that some error likely occured. Let's stop it to prevent issues with following tests!"
    );
    Services.profiler.StopProfiler();
  }
});

/**
 * This is a helper function that will stop the profiler and returns the main
 * threads for the parent process and the content process with PID contentPid.
 * This happens immediately, without waiting for any sampling to happen or
 * finish. Use stopProfilerAndGetThreads (without "Now") below instead to wait
 * for samples before stopping.
 * This returns also the full profile in case the caller wants more information.
 *
 * @param {number} contentPid
 * @returns {Promise}
 */
async function stopProfilerNowAndGetThreads(contentPid) {
  Services.profiler.Pause();
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

  return { parentThread, contentThread, profile };
}

/**
 * This is a helper function that will stop the profiler and returns the main
 * threads for the parent process and the content process with PID contentPid.
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

/**
 * This finds a thread with a specific network marker. This is useful to find
 * the thread for a service worker.
 *
 * @param {Object} profile
 * @param {string} filename
 * @returns {Object | null} the found thread
 */
function findContentThreadWithNetworkMarkerForFilename(profile, filename) {
  const allThreads = [
    profile.threads,
    ...profile.processes.map(process => process.threads),
  ].flat();

  const thread = allThreads.find(
    ({ processType, markers }) =>
      processType === "tab" &&
      markers.data.some(markerTuple => {
        const data = markerTuple[markers.schema.data];
        return (
          data &&
          data.type === "Network" &&
          data.URI &&
          data.URI.endsWith(filename)
        );
      })
  );
  return thread || null;
}

/**
 * This logs some basic information about the passed thread.
 *
 * @param {string} prefix
 * @param {Object} thread
 */
function logInformationForThread(prefix, thread) {
  const { name, pid, tid, processName, processType } = thread;
  info(
    `${prefix}: ` +
      `name(${name}) pid(${pid}) tid(${tid}) processName(${processName}) processType(${processType})`
  );
}
