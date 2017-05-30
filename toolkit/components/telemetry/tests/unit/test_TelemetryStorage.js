/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);

const PING_TYPE_MAIN = "main";
const REASON_GATHER_PAYLOAD = "gather-payload";

function getPing() {
  TelemetrySession.earlyInit(true);

  const payload = TelemetrySession.getPayload(REASON_GATHER_PAYLOAD);
  const options = {addClientId: true, addEnvironment: true};
  return TelemetryController.testAssemblePing(PING_TYPE_MAIN, payload, options);
}

// Setting up test environment.

add_task(function* test_setup() {
  do_get_profile();
});

// Testing whether correct values are being recorded in
// "TELEMETRY_PENDING_LOAD_MS" histogram.

add_task(function* test_pendingLoadTime() {
  TelemetryStorage.reset();
  var ping = getPing();

  var h = Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_MS");
  var initialSum = h.snapshot().sum;

  TelemetryStorage.addPendingPing(ping).then(() => {
    TelemetryStorage.loadPendingPing(ping.id).then(() => {
      TelemetryStorage.removePendingPing(ping.id);
      Assert.ok(h.snapshot().sum - initialSum > 0,
                "Value must be inserted into the histogram.");
    });
  });
});

// Testing whether correct values are being recorded in
// "TELEMETRY_ARCHIVE_LOAD_MS" histogram.

add_task(function* test_archiveLoadTime() {
  TelemetryStorage.reset();

  var ping = getPing();
  var creationDate = new Date(ping.creationDate);

  var h = Telemetry.getHistogramById("TELEMETRY_ARCHIVE_LOAD_MS");
  var initialSum = h.snapshot().sum;

  TelemetryStorage.saveArchivedPing(ping).then(() => {
    TelemetryStorage.loadArchivedPing(ping.id).then(() => {
      TelemetryStorage.removeArchivedPing(ping.id, creationDate, ping.type);
      Assert.ok(h.snapshot().sum - initialSum > 0,
                "Value must be inserted into the histogram.");
    });
  });
});
