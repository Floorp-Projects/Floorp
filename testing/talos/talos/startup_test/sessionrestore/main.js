"use strict";

var Services = Components.utils.import("resource://gre/modules/Services.jsm", {}).Services;

/**
 * Display the result, send it to the harness and quit.
 */
function finish() {
  Profiler.pause("This test measures the time between sessionRestoreInit and sessionRestored, ignore everything around that");
  Profiler.initFromURLQueryParams(location.search);
  Profiler.finishStartupProfiling();

  setTimeout(function () {
    var startup_info = Services.startup.getStartupInfo();

    var duration = startup_info.sessionRestored - startup_info.sessionRestoreInit;

    // Show result on screen. Nice but not really necessary.
    document.getElementById("sessionRestoreInit-to-sessionRestored").textContent = duration + "ms";

    // Report data to Talos, if possible
    dumpLog("__start_report" +
            duration         +
            "__end_report\n\n");

    // Next one is required by the test harness but not used
    dumpLog("__startTimestamp" +
            Date.now()         +
            "__endTimestamp\n\n");

    goQuitApplication();
  }, 0);
}

function main() {
  // Collect (and display) data
  var startup_info = Services.startup.getStartupInfo();

  // The script may be triggered before or after sessionRestored
  // and sessionRestoreInit are available. If both are available,
  // we are done.
  if (typeof startup_info.sessionRestored != "undefined"
   && typeof startup_info.sessionRestoreInit != "undefined") {
    finish();
    return;
  }

  // Otherwise, we need to wait until *after* sesionstore-windows-restored,
  // which is the event that sets sessionRestored - since sessionRestoreInit
  // is set before sessionRestored, we are certain that both are now set.
  Services.obs.addObserver(finish, "sessionstore-windows-restored", false);
}

main();
