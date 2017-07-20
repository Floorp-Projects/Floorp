/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

let { classes: Cc, utils: Cu, interfaces: Ci, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");

function ensureProfilerInitialized() {
  // Starting and stopping the profiler with the "stackwalk" flag will cause the
  // profiler's stackwalking features to be synchronously initialized. This
  // should prevent us from not initializing BHR quickly enough.
  if (!Services.profiler.CanProfile()) {
    return false;
  }
  let features = ["stackwalk"];
  Services.profiler.StartProfiler(1000, 10, features, features.length);
  Services.profiler.StopProfiler();
  return true;
}

add_task(async function childCauseHang() {
  if (!ensureProfilerInitialized()) {
    return;
  }

  do_execute_soon(() => {
    let startTime = Date.now();
    while ((Date.now() - startTime) < 2000);
  });


  let hang = await new Promise(resolve => {
    const onhang = subject => {
      Services.obs.removeObserver(onhang, "bhr-thread-hang");
      resolve(subject.QueryInterface(Ci.nsIHangDetails));
    };
    Services.obs.addObserver(onhang, "bhr-thread-hang");
  });

  equal(hang.process, "tab");
  equal(hang.thread, "Gecko_Child");
  // NOTE: This hang should also be sent to the parent process, where it'll be
  // checked more.
});
