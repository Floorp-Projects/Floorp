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
