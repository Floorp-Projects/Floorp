"use strict";

(function() {
  async function startProfiler(settings) {
    let startPromise = Services.profiler.StartProfiler(
      settings.entries,
      settings.interval,
      settings.features,
      settings.threads,
      0,
      settings.duration
    );

    info("Parent Profiler has started");

    await startPromise;

    info("Child profilers have started");
  }

  function getProfile() {
    const profile = Services.profiler.getProfileData();
    info(
      "We got a profile, run the mochitest with `--keep-open true` to see the logged profile in the Web Console."
    );

    // Run the mochitest with `--keep-open true` to see the logged profile in the
    // Web console.
    console.log(profile);

    return profile;
  }

  async function stopProfiler() {
    let stopPromise = Services.profiler.StopProfiler();
    info("Parent profiler has stopped");
    await stopPromise;
    info("Child profilers have stopped");
  }

  function end(error) {
    if (error) {
      ok(false, `We got an error: ${error}`);
    } else {
      ok(true, "We ran the whole process");
    }
    SimpleTest.finish();
  }

  async function runTest(settings, workload) {
    SimpleTest.waitForExplicitFinish();
    try {
      await startProfiler(settings);
      await workload();
      await getProfile();
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
