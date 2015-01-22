/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that TelemetrySession notifies correctly on idle-daily.

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);

function run_test() {
  do_test_pending();

  Services.obs.addObserver(function observeTelemetry() {
    Services.obs.removeObserver(observeTelemetry, "gather-telemetry");
    do_test_finished();
  }, "gather-telemetry", false);

  TelemetrySession.observe(null, "idle-daily", null);
}
