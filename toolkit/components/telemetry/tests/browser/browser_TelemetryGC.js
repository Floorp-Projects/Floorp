"use strict";

/*
 *********************************************************************************
 *                                                                               *
 *                                  WARNING                                      *
 *                                                                               *
 * If you adjust any of the constants here (slice limit, number of keys, etc.)   *
 * make sure to update the JSON schema at:                                       *
 * https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/master/     *
 * telemetry/main.schema.json                                                    *
 *                                                                               *
 * Otherwise, pings may be dropped by the telemetry backend!                     *
 *                                                                               *
 ********************************************************************************/

const {GCTelemetry} = Cu.import("resource://gre/modules/GCTelemetry.jsm", {});

function check(entries) {
  const FIELDS = ["random", "worst"];

  // Check that all FIELDS are in |entries|.
  for (let f of FIELDS) {
    ok(f in entries, `${f} found in entries`);
  }

  // Check that only FIELDS are in |entries|.
  for (let k of Object.keys(entries)) {
    ok(FIELDS.includes(k), `${k} found in FIELDS`);
  }

  let foundGCs = 0;

  for (let f of FIELDS) {
    ok(Array.isArray(entries[f]), "have an array of GCs");

    ok(entries[f].length <= 2, "not too many GCs");

    for (let gc of entries[f]) {
      ok(gc !== null, "GC is non-null");

      foundGCs++;

      ok(Object.keys(gc).length <= 25, "number of keys in GC is not too large");

      // Sanity check the GC data.
      ok("total_time" in gc, "total_time field present");
      ok("max_pause" in gc, "max_pause field present");

      ok("slices" in gc, "slices field present");
      ok(Array.isArray(gc.slices), "slices is an array");
      ok(gc.slices.length > 0, "slices array non-empty");
      ok(gc.slices.length <= 4, "slices array is not too long");

      ok("totals" in gc, "totals field present");
      ok(typeof(gc.totals) == "object", "totals is an object");
      ok(Object.keys(gc.totals).length <= 65, "totals array is not too long");

      // Make sure we don't skip any big objects.
      for (let key in gc) {
        if (key != "slices" && key != "totals") {
          ok(typeof(gc[key]) != "object", `${key} property should be primitive`);
        }
      }

      let phases = new Set();

      for (let slice of gc.slices) {
        ok(Object.keys(slice).length <= 15, "slice is not too large");

        ok("pause" in slice, "pause field present in slice");
        ok("reason" in slice, "reason field present in slice");
        ok("times" in slice, "times field present in slice");

        // Make sure we don't skip any big objects.
        for (let key in slice) {
          if (key != "times") {
            ok(typeof(slice[key]) != "object", `${key} property should be primitive`);
          }
        }

        ok(Object.keys(slice.times).length <= 65, "no more than 65 phases");

        for (let phase in slice.times) {
          phases.add(phase);
          ok(typeof(slice.times[phase]) == "number", `${phase} property should be a number`);
        }
      }

      let totals = gc.totals;
      // Make sure we don't skip any big objects.
      for (let phase in totals) {
        ok(typeof(totals[phase]) == "number", `${phase} property should be a number`);
      }

      for (let phase of phases) {
        ok(phase in totals, `${phase} is in totals`);
      }
    }
  }

  ok(foundGCs > 0, "saw at least one GC");
}

add_task(function* test() {
  let multiprocess = Services.appinfo.browserTabsRemoteAutostart;

  // Set these prefs to ensure that we get measurements.
  const prefs = {"set": [["javascript.options.mem.notify", true]]};
  yield new Promise(resolve => SpecialPowers.pushPrefEnv(prefs, resolve));

  function runRemote(f) {
    gBrowser.selectedBrowser.messageManager.loadFrameScript(`data:,(${f})()`, false);
  }

  function initScript() {
    const {GCTelemetry} = Components.utils.import("resource://gre/modules/GCTelemetry.jsm", {});

    /*
     * Don't shut down GC telemetry if it was already running before the test!
     * Note: We need to use a multiline comment here since this code is turned into a data: URI.
     */
    let shutdown = GCTelemetry.init();

    function listener() {
      removeMessageListener("GCTelemTest:Shutdown", listener);
      if (shutdown) {
        GCTelemetry.shutdown();
      }
    }
    addMessageListener("GCTelemTest:Shutdown", listener);
  }

  if (multiprocess) {
    runRemote(initScript);
  }

  // Don't shut down GC telemetry if it was already running before the test!
  let shutdown = GCTelemetry.init();
  registerCleanupFunction(() => {
    if (shutdown) {
      GCTelemetry.shutdown();
    }

    gBrowser.selectedBrowser.messageManager.sendAsyncMessage("GCTelemTest:Shutdown");
  });

  let localPromise = new Promise(resolve => {
    function obs() {
      Services.obs.removeObserver(obs, "garbage-collection-statistics");
      resolve();
    }
    Services.obs.addObserver(obs, "garbage-collection-statistics", false);
  });

  let remotePromise;
  if (multiprocess) {
    remotePromise = new Promise(resolve => {
      function obs() {
        Services.ppmm.removeMessageListener("Telemetry:GCStatistics", obs);
        resolve();
      }
      Services.ppmm.addMessageListener("Telemetry:GCStatistics", obs);
    });
  } else {
    remotePromise = Promise.resolve();
  }

  // Make sure we have a GC to work with in both processes.
  Cu.forceGC();
  if (multiprocess) {
    runRemote(() => Components.utils.forceGC());
  }

  info("Waiting for GCs");

  yield Promise.all([localPromise, remotePromise]);

  let localEntries = GCTelemetry.entries("main", true);
  let remoteEntries = multiprocess ? GCTelemetry.entries("content", true) : localEntries;

  check(localEntries);
  check(remoteEntries);

  localEntries = GCTelemetry.entries("main", false);
  remoteEntries = multiprocess ? GCTelemetry.entries("content", false) : localEntries;

  is(localEntries.random.length, 0, "no random GCs after reset");
  is(localEntries.worst.length, 0, "no worst GCs after reset");

  is(remoteEntries.random.length, 0, "no random GCs after reset");
  is(remoteEntries.worst.length, 0, "no worst GCs after reset");
});
