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

registerCleanupFunction(async () => {
  if (Services.profiler.IsActive()) {
    info(
      "The profiler was found to still be running at the end of the test, which means that some error likely occured. Let's stop it to prevent issues with following tests!"
    );
    await Services.profiler.StopProfiler();
  }
});

/**
 * This is a helper function that will stop the profiler and returns the main
 * threads for the parent process and the content process with PID contentPid.
 * This happens immediately, without waiting for any sampling to happen or
 * finish. Use waitSamplingAndStopProfilerAndGetThreads below instead to wait
 * for samples before stopping.
 * This returns also the full profile in case the caller wants more information.
 *
 * @param {number} contentPid
 * @returns {Promise<{profile, parentThread, contentProcess, contentThread}>}
 */
async function stopProfilerNowAndGetThreads(contentPid) {
  const profile = await stopNowAndGetProfile();

  const parentThread = profile.threads[0];
  const contentProcess = profile.processes.find(
    p => p.threads[0].pid == contentPid
  );
  if (!contentProcess) {
    throw new Error(
      `Could not find the content process with given pid: ${contentPid}`
    );
  }

  if (!parentThread) {
    throw new Error("The parent thread was not found in the profile.");
  }

  const contentThread = contentProcess.threads[0];
  if (!contentThread) {
    throw new Error("The content thread was not found in the profile.");
  }

  return { profile, parentThread, contentProcess, contentThread };
}

/**
 * This is a helper function that will stop the profiler and returns the main
 * threads for the parent process and the content process with PID contentPid.
 * As opposed to stopProfilerNowAndGetThreads (with "Now") above, the profiler
 * in that PID will not stop until there is at least one periodic sample taken.
 *
 * @param {number} contentPid
 * @returns {Promise<{profile, parentThread, contentProcess, contentThread}>}
 */
async function waitSamplingAndStopProfilerAndGetThreads(contentPid) {
  await Services.profiler.waitOnePeriodicSampling();

  return stopProfilerNowAndGetThreads(contentPid);
}

/** This tries to find the service worker thread by targeting a very specific
 * UserTiming marker. Indeed we use performance.mark to add this marker from the
 * service worker's events.
 * Then from this thread we get its parent thread. Indeed the parent thread is
 * where all network stuff happens, so this is useful for network marker tests.
 *
 * @param {Object} profile
 * @returns {{ serviceWorkerThread: Object, serviceWorkerParentThread: Object }} the found threads
 */
function findServiceWorkerThreads(profile) {
  const allThreads = [
    profile.threads,
    ...profile.processes.map(process => process.threads),
  ].flat();

  const serviceWorkerThread = allThreads.find(
    ({ processType, markers }) =>
      processType === "tab" &&
      markers.data.some(markerTuple => {
        const data = markerTuple[markers.schema.data];
        return (
          data &&
          data.type === "UserTiming" &&
          data.name === "__serviceworker_event"
        );
      })
  );

  if (!serviceWorkerThread) {
    info(
      "We couldn't find a service worker thread. Here are all the threads in this profile:"
    );
    allThreads.forEach(logInformationForThread.bind(null, ""));
    return null;
  }

  const serviceWorkerParentThread = allThreads.find(
    ({ name, pid }) => pid === serviceWorkerThread.pid && name === "GeckoMain"
  );

  if (!serviceWorkerParentThread) {
    info(
      `We couldn't find a parent thread for the service worker thread (pid: ${serviceWorkerThread.pid}, tid: ${serviceWorkerThread.tid}).`
    );
    info("Here are all the threads in this profile:");
    allThreads.forEach(logInformationForThread.bind(null, ""));

    // Let's write the profile on disk if MOZ_UPLOAD_DIR is present
    const env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    const path = env.get("MOZ_UPLOAD_DIR");
    if (path) {
      const profileName = `profile_${Date.now()}.json`;
      const profilePath = PathUtils.join(path, profileName);
      info(
        `We wrote down the profile on disk as an artifact, with name ${profileName}.`
      );
      // This function returns a Promise, but we're not waiting on it because
      // we're in a synchronous function. Hopefully writing will be finished
      // when the process ends.
      IOUtils.writeJSON(profilePath, profile).catch(err =>
        console.error("An error happened when writing the profile on disk", err)
      );
    }
    throw new Error(
      "We couldn't find a parent thread for the service worker thread. Please read logs to find more information."
    );
  }

  return { serviceWorkerThread, serviceWorkerParentThread };
}

/**
 * This logs some basic information about the passed thread.
 *
 * @param {string} prefix
 * @param {Object} thread
 */
function logInformationForThread(prefix, thread) {
  if (!thread) {
    info(prefix + ": thread is null or undefined.");
    return;
  }

  const { name, pid, tid, processName, processType } = thread;
  info(
    `${prefix}: ` +
      `name(${name}) pid(${pid}) tid(${tid}) processName(${processName}) processType(${processType})`
  );
}
