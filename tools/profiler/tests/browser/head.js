const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

const BASE_URL = "http://example.com/browser/tools/profiler/tests/browser/";

const defaultSettings = {
  entries: 1000000, // 9MB
  interval: 1, // ms
  features: ["threads"],
  threads: ["GeckoMain"],
};

function startProfiler(callersSettings) {
  const settings = Object.assign({}, defaultSettings, callersSettings);
  Services.profiler.StartProfiler(
    settings.entries,
    settings.interval,
    settings.features,
    settings.threads,
    settings.duration
  );
}

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

/**
 * This is a helper function be able to run `await wait(500)`. Unfortunately this
 * is needed as the act of collecting functions relies on the periodic sampling of
 * the threads. See: https://bugzilla.mozilla.org/show_bug.cgi?id=1529053
 *
 * @param {number} time
 * @returns {Promise}
 */
function wait(time) {
  return new Promise(resolve => {
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    setTimeout(resolve, time);
  });
}

/**
 * Get the payloads of a type from a single thread.
 *
 * @param {Object} thread The thread from a profile.
 * @param {string} type The marker payload type, e.g. "DiskIO".
 * @return {Array} The payloads.
 */
function getPayloadsOfType(thread, type) {
  const { markers } = thread;
  const results = [];
  for (const markerTuple of markers.data) {
    const payload = markerTuple[markers.schema.data];
    if (payload && payload.type === type) {
      results.push(payload);
    }
  }
  return results;
}
