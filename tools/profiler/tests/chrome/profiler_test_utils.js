"use strict";

(function() {

ChromeUtils.import("resource://gre/modules/Services.jsm");

function startProfiler(settings) {
  Services.profiler.StartProfiler(
    settings.entries,
    settings.interval,
    settings.features,
    settings.features.length,
    settings.threads,
    settings.threads.length
  );

  info("Profiler has started");
}

function getProfile() {
  const profile = Services.profiler.getProfileData();
  info("We got a profile, run the mochitest with `--keep-open true` to see the logged profile in the Web Console.");

  // Run the mochitest with `--keep-open true` to see the logged profile in the
  // Web console.
  console.log(profile);

  return profile;
}

function stopProfiler() {
  Services.profiler.StopProfiler();
  info("Profiler has stopped");
}

function end(error) {
  if (error) {
    ok(false, `We got an error: ${error}`);
  } else {
    ok(true, "We ran the whole process");
  }
  SimpleTest.finish();
}

function getBufferInfo() {
  let position = {}, totalSize = {}, generation = {};
  Services.profiler.GetBufferInfo(position, totalSize, generation);
  return {
    position: position.value,
    totalSize: totalSize.value,
    generation: generation.value
  };
}

async function runTest(settings, workload,
                       checkProfileCallback = function(profile) {}) {
  SimpleTest.waitForExplicitFinish();
  try {
    await startProfiler(settings);

    // Run workload() one or more times until at least one sample has been taken.
    const bufferInfoAtStart = getBufferInfo();
    while (true) {
      await workload();
      const bufferInfoAfterWorkload = getBufferInfo();
      if (bufferInfoAfterWorkload.generation > bufferInfoAtStart.generation ||
          bufferInfoAfterWorkload.position > bufferInfoAtStart.position) {
        // The buffer position advanced, so we've either added a marker or a
        // sample. It would be better to have conclusive evidence that we
        // actually have a sample...
        break;
      }
    }

    const profile = await getProfile();
    await checkProfileCallback(profile);
    await stopProfiler();
    await end();
  } catch (e) {
    // By catching and handling the error, we're being nice to mochitest
    // runners: instead of waiting for the timeout, we fail right away.
    await end(e);
  }
}

window.runTest = runTest;

})();
