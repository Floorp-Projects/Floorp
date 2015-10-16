"use strict";

var Services = Components.utils.import("resource://gre/modules/Services.jsm", {}).Services;

// Process Message Manager topics.
const MSG_REQUEST = "session-restore-test?duration";
const MSG_PROVIDE = "session-restore-test:duration";

Services.cpmm.addMessageListener(MSG_PROVIDE,
  /**
   * Display the result, send it to the harness and quit.
   */
  function finish(msg) {
    console.log(`main.js: received data on ${MSG_PROVIDE}`, msg);
    Services.cpmm.removeMessageListener(MSG_PROVIDE, finish);
    var duration = msg.data.duration;

    Profiler.pause("This test measures the time between sessionRestoreInit and sessionRestored, ignore everything around that");
    Profiler.initFromURLQueryParams(location.search);
    Profiler.finishStartupProfiling();

    setTimeout(function () {
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
});

// In case the add-on has broadcasted the message before we were loaded,
// request a second broadcast.
Services.cpmm.sendAsyncMessage(MSG_REQUEST, {});
