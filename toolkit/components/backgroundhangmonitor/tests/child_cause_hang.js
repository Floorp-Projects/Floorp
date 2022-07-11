/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

function ensureProfilerInitialized() {
  // Starting and stopping the profiler with the "stackwalk" flag will cause the
  // profiler's stackwalking features to be synchronously initialized. This
  // should prevent us from not initializing BHR quickly enough.
  let features = ["stackwalk"];
  Services.profiler.StartProfiler(1000, 10, features);
  Services.profiler.StopProfiler();
}

add_task(async function childCauseHang() {
  ensureProfilerInitialized();

  executeSoon(() => {
    let startTime = Date.now();
    // eslint-disable-next-line no-empty
    while (Date.now() - startTime < 2000) {}
  });

  await do_await_remote_message("bhr_hangs_detected");
});
