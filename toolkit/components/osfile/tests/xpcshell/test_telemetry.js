"use strict";

var {OS: {File, Path, Constants}} = Components.utils.import("resource://gre/modules/osfile.jsm", {});
var {Services} = Components.utils.import("resource://gre/modules/Services.jsm", {});

// Ensure that we have a profile but that the OS.File worker is not launched
add_task(async function init() {
  do_get_profile();
  await File.resetWorker();
});

function getCount(histogram) {
  if (histogram == null) {
    return 0;
  }

  let total = 0;
  for (let i of histogram.counts) {
    total += i;
  }
  return total;
}

// Ensure that launching the OS.File worker adds data to the relevant
// histograms
add_task(async function test_startup() {
  let LAUNCH = "OSFILE_WORKER_LAUNCH_MS";
  let READY = "OSFILE_WORKER_READY_MS";

  let before = Services.telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                     false,
                                                     false).parent;

  // Launch the OS.File worker
  await File.getCurrentDirectory();

  let after = Services.telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                    false,
                                                    false).parent;


  do_print("Ensuring that we have recorded measures for histograms");
  Assert.equal(getCount(after[LAUNCH]), getCount(before[LAUNCH]) + 1);
  Assert.equal(getCount(after[READY]), getCount(before[READY]) + 1);

  do_print("Ensuring that launh <= ready");
  Assert.ok(after[LAUNCH].sum <= after[READY].sum);
});

// Ensure that calling writeAtomic adds data to the relevant histograms
add_task(async function test_writeAtomic() {
  let LABEL = "OSFILE_WRITEATOMIC_JANK_MS";

  let before = Services.telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                     false,
                                                     false).parent;

  // Perform a write.
  let path = Path.join(Constants.Path.profileDir, "test_osfile_telemetry.tmp");
  await File.writeAtomic(path, LABEL, { tmpPath: path + ".tmp" } );

  let after = Services.telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                    false,
                                                    false).parent;

  Assert.equal(getCount(after[LABEL]), getCount(before[LABEL]) + 1);
});
