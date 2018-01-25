/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let { classes: Cc, utils: Cu, interfaces: Ci, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
const { TelemetryUtils } = Cu.import("resource://gre/modules/TelemetryUtils.jsm", {});

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

add_task(async function test_BHRObserver() {
  if (!Services.telemetry.canRecordExtended) {
    ok("Hang reporting not enabled.");
    return;
  }

  if (!ensureProfilerInitialized()) {
    return;
  }

  let telSvc = Cc["@mozilla.org/bhr-telemetry-service;1"].getService().wrappedJSObject;
  ok(telSvc, "Should have BHRTelemetryService");
  let beforeLen = telSvc.payload.hangs.length;

  if (Services.appinfo.OS === "Linux" || Services.appinfo.OS === "Android") {
    // We use the rt_tgsigqueueinfo syscall on Linux which requires a
    // certain kernel version. It's not an error if the system running
    // the test is older than that.
    let kernel = Services.sysinfo.get("kernel_version") ||
          Services.sysinfo.get("version");
    if (Services.vc.compare(kernel, "2.6.31") < 0) {
      ok("Hang reporting not supported for old kernel.");
      return;
    }
  }

  let hangsPromise = new Promise(resolve => {
    let hangs = [];
    const onThreadHang = subject => {
      let hang = subject.QueryInterface(Ci.nsIHangDetails);
      if (hang.thread.startsWith("Gecko")) {
        hangs.push(hang);
        if (hangs.length >= 3) {
          Services.obs.removeObserver(onThreadHang, "bhr-thread-hang");
          resolve(hangs);
        }
      }
    };
    Services.obs.addObserver(onThreadHang, "bhr-thread-hang");
  });

  // We're going to trigger two hangs, of various lengths. One should be a
  // transient hang, and the other a permanent hang. We'll wait for the hangs to
  // be recorded.

  executeSoon(() => {
    let startTime = Date.now();
    while ((Date.now() - startTime) < 10000);
  });

  executeSoon(() => {
    let startTime = Date.now();
    while ((Date.now() - startTime) < 1000);
  });

  Services.prefs.setBoolPref(TelemetryUtils.Preferences.OverridePreRelease, true);
  let childDone = run_test_in_child("child_cause_hang.js");

  // Now we wait for the hangs to have their bhr-thread-hang message fired for
  // them, collect them, and analyize the response.
  let hangs = await hangsPromise;
  equal(hangs.length, 3);
  hangs.forEach(hang => {
    ok(hang.duration > 0);
    ok(hang.thread == "Gecko" || hang.thread == "Gecko_Child");
    equal(typeof hang.runnableName, "string");

    // hang.stack
    ok(Array.isArray(hang.stack));
    ok(hang.stack.length > 0);
    hang.stack.forEach(entry => {
      // Each stack frame entry is either a native or pseudostack entry. A
      // native stack entry is an array with module index (number), and offset
      // (hex string), while the pseudostack entry is a bare string.
      if (Array.isArray(entry)) {
        equal(entry.length, 2);
        equal(typeof entry[0], "number");
        equal(typeof entry[1], "string");
      } else {
        equal(typeof entry, "string");
      }
    });

    // hang.modules
    ok(Array.isArray(hang.modules));
    hang.modules.forEach(module => {
      ok(Array.isArray(module));
      equal(module.length, 2);
      equal(typeof module[0], "string");
      equal(typeof module[1], "string");
    });

    // hang.annotations
    equal(typeof hang.annotations, "object");
    Object.keys(hang.annotations).forEach(key => {
      equal(typeof hang.annotations[key], "string");
    });

  });

  // Check that the telemetry service collected pings which make sense
  ok(telSvc.payload.hangs.length - beforeLen >= 3);
  ok(Array.isArray(telSvc.payload.modules));
  telSvc.payload.modules.forEach(module => {
    ok(Array.isArray(module));
    equal(module.length, 2);
    equal(typeof module[0], "string");
    equal(typeof module[1], "string");
  });

  telSvc.payload.hangs.forEach(hang => {
    ok(hang.duration > 0);
    ok(hang.thread == "Gecko" || hang.thread == "Gecko_Child");
    equal(typeof hang.runnableName, "string");

    // hang.stack
    ok(Array.isArray(hang.stack));
    ok(hang.stack.length > 0);
    hang.stack.forEach(entry => {
      // Each stack frame entry is either a native or pseudostack entry. A
      // native stack entry is an array with module index (number), and offset
      // (hex string), while the pseudostack entry is a bare string.
      if (Array.isArray(entry)) {
        equal(entry.length, 2);
        equal(typeof entry[0], "number");
        ok(entry[0] < telSvc.payload.hangs.length);
        equal(typeof entry[1], "string");
      } else {
        equal(typeof entry, "string");
      }
    });

    // hang.annotations
    equal(typeof hang.annotations, "object");
    Object.keys(hang.annotations).forEach(key => {
      equal(typeof hang.annotations[key], "string");
    });
  });

  do_send_remote_message("bhr_hangs_detected");
  await childDone;
});
