/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that TelemetryPing notifies correctly on idle.

Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  do_test_pending();

  Services.obs.addObserver(function observeTelemetry() {
    Services.obs.removeObserver(observeTelemetry, "gather-telemetry");
    do_test_finished();
  }, "gather-telemetry", false);

  Components.classes["@mozilla.org/base/telemetry-ping;1"]
    .getService(Components.interfaces.nsITelemetryPing)
    .observe(null, "idle-daily", null);
}
