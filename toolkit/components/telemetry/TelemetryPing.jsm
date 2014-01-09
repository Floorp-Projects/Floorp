/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/debug.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
#ifndef MOZ_WIDGET_GONK
Cu.import("resource://gre/modules/LightweightThemeManager.jsm");
#endif
Cu.import("resource://gre/modules/ctypes.jsm");
Cu.import("resource://gre/modules/ThirdPartyCookieProbe.jsm");
Cu.import("resource://gre/modules/TelemetryFile.jsm");
Cu.import("resource://gre/modules/UITelemetry.jsm");

// When modifying the payload in incompatible ways, please bump this version number
const PAYLOAD_VERSION = 1;

// This is the HG changeset of the Histogram.json file, used to associate
// submitted ping data with its histogram definition (bug 832007)
#expand const HISTOGRAMS_FILE_VERSION = "__HISTOGRAMS_FILE_VERSION__";

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_SERVER = PREF_BRANCH + "server";
#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
const PREF_ENABLED = PREF_BRANCH + "enabledPreRelease";
#else
const PREF_ENABLED = PREF_BRANCH + "enabled";
#endif
const PREF_PREVIOUS_BUILDID = PREF_BRANCH + "previousBuildID";

// Do not gather data more than once a minute
const TELEMETRY_INTERVAL = 60000;
// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY = 60000;
// Delay before initializing telemetry if we're testing (ms)
const TELEMETRY_TEST_DELAY = 100;

// Seconds of idle time before pinging.
// On idle-daily a gather-telemetry notification is fired, during it probes can
// start asynchronous tasks to gather data.  On the next idle the data is sent.
const IDLE_TIMEOUT_SECONDS = 5 * 60;

var gLastMemoryPoll = null;

let gWasDebuggerAttached = false;

function getLocale() {
  return Cc["@mozilla.org/chrome/chrome-registry;1"].
         getService(Ci.nsIXULChromeRegistry).
         getSelectedLocale('global');
}

XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                   "@mozilla.org/base/telemetry;1",
                                   "nsITelemetry");
XPCOMUtils.defineLazyServiceGetter(this, "idleService",
                                   "@mozilla.org/widget/idleservice;1",
                                   "nsIIdleService");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonManagerPrivate",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry",
                                  "resource://gre/modules/UITelemetry.jsm");

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

/**
 * Read current process I/O counters.
 */
let processInfo = {
  _initialized: false,
  _IO_COUNTERS: null,
  _kernel32: null,
  _GetProcessIoCounters: null,
  _GetCurrentProcess: null,
  getCounters: function() {
    let isWindows = ("@mozilla.org/windows-registry-key;1" in Components.classes);
    if (isWindows)
      return this.getCounters_Windows();
    return null;
  },
  getCounters_Windows: function() {
    if (!this._initialized){
      this._IO_COUNTERS = new ctypes.StructType("IO_COUNTERS", [
        {'readOps': ctypes.unsigned_long_long},
        {'writeOps': ctypes.unsigned_long_long},
        {'otherOps': ctypes.unsigned_long_long},
        {'readBytes': ctypes.unsigned_long_long},
        {'writeBytes': ctypes.unsigned_long_long},
        {'otherBytes': ctypes.unsigned_long_long} ]);
      try {
        this._kernel32 = ctypes.open("Kernel32.dll");
        this._GetProcessIoCounters = this._kernel32.declare("GetProcessIoCounters",
          ctypes.winapi_abi,
          ctypes.bool, // return
          ctypes.voidptr_t, // hProcess
          this._IO_COUNTERS.ptr); // lpIoCounters
        this._GetCurrentProcess = this._kernel32.declare("GetCurrentProcess",
          ctypes.winapi_abi,
          ctypes.voidptr_t); // return
        this._initialized = true;
      } catch (err) {
        return null;
      }
    }
    let io = new this._IO_COUNTERS();
    if(!this._GetProcessIoCounters(this._GetCurrentProcess(), io.address()))
      return null;
    return [parseInt(io.readBytes), parseInt(io.writeBytes)];
  }
};

this.EXPORTED_SYMBOLS = ["TelemetryPing"];

this.TelemetryPing = Object.freeze({
  getPayload: function() {
    return Impl.getPayload();
  },
  saveHistograms: function(aFile, aSync) {
    return Impl.saveHistograms(aFile, aSync);
  },
  gatherStartup: function() {
    return Impl.gatherStartup();
  },
  enableLoadSaveNotifications: function() {
    return Impl.enableLoadSaveNotifications();
  },
  cacheProfileDirectory: function() {
    return Impl.cacheProfileDirectory();
  },
  setAddOns: function(aAddOns) {
    return Impl.setAddOns(aAddOns);
  },
  testPing: function(aServer) {
    return Impl.testPing(aServer);
  },
  testLoadHistograms: function(aFile, aSync) {
    return Impl.testLoadHistograms(aFile, aSync);
  },
  submissionPath: function() {
    return Impl.submissionPath();
  },
  Constants: Object.freeze({
    PREF_ENABLED: PREF_ENABLED,
    PREF_PREVIOUS_BUILDID: PREF_PREVIOUS_BUILDID,
  }),
  /**
   * Reset TelemetryPing.
   *
   * Used only for testing purposes.
   */
  reset: function() {
    try {
      Impl.uninstall();
    } catch (ex) {
      // Ignore errors
    }
    Impl.setup();
  },
  /**
   * Descriptive metadata
   *
   * @param  reason
   *         The reason for the telemetry ping, this will be included in the
   *         returned metadata,
   * @return The metadata as a JS object
   */
  getMetadata: function(reason) {
    return Impl.getMetadata(reason);
  },
  /**
   * Send a notification.
   */
  observe: function (aSubject, aTopic, aData) {
    return Impl.observe(aSubject, aTopic, aData);
  }
});

let Impl = {
  _histograms: {},
  _initialized: false,
  _prevValues: {},
  // Generate a unique id once per session so the server can cope with
  // duplicate submissions.
  _uuid: generateUUID(),
  // Regex that matches histograms we care about during startup.
  // Keep this in sync with gen-histogram-bucket-ranges.py.
  _startupHistogramRegex: /SQLITE|HTTP|SPDY|CACHE|DNS/,
  _slowSQLStartup: {},
  _prevSession: null,
  _hasWindowRestoredObserver: false,
  _hasXulWindowVisibleObserver: false,
  _startupIO : {},
  // The previous build ID, if this is the first run with a new build.
  // Undefined if this is not the first run, or the previous build ID is unknown.
  _previousBuildID: undefined,

  /**
   * Gets a series of simple measurements (counters). At the moment, this
   * only returns startup data from nsIAppStartup.getStartupInfo().
   *
   * @return simple measurements as a dictionary.
   */
  getSimpleMeasurements: function getSimpleMeasurements(forSavedSession) {
    let si = Services.startup.getStartupInfo();

    var ret = {
      // uptime in minutes
      uptime: Math.round((new Date() - si.process) / 60000)
    }

    // Look for app-specific timestamps
    var appTimestamps = {};
    try {
      let o = {};
      Cu.import("resource://gre/modules/TelemetryTimestamps.jsm", o);
      appTimestamps = o.TelemetryTimestamps.get();
    } catch (ex) {}
    try {
      ret.addonManager = AddonManagerPrivate.getSimpleMeasures();
    } catch (ex) {}
    try {
      ret.UITelemetry = UITelemetry.getSimpleMeasures();
    } catch (ex) {}

    if (si.process) {
      for each (let field in Object.keys(si)) {
        if (field == "process")
          continue;
        ret[field] = si[field] - si.process
      }

      for (let p in appTimestamps) {
        if (!(p in ret) && appTimestamps[p])
          ret[p] = appTimestamps[p] - si.process;
      }
    }

    ret.startupInterrupted = new Number(Services.startup.interrupted);

    // Update debuggerAttached flag
    let debugService = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
    let isDebuggerAttached = debugService.isDebuggerAttached;
    gWasDebuggerAttached = gWasDebuggerAttached || isDebuggerAttached;
    ret.debuggerAttached = new Number(gWasDebuggerAttached);

    ret.js = Cc["@mozilla.org/js/xpc/XPConnect;1"]
      .getService(Ci.nsIJSEngineTelemetryStats)
      .telemetryValue;

    let shutdownDuration = Telemetry.lastShutdownDuration;
    if (shutdownDuration)
      ret.shutdownDuration = shutdownDuration;

    let failedProfileLockCount = Telemetry.failedProfileLockCount;
    if (failedProfileLockCount)
      ret.failedProfileLockCount = failedProfileLockCount;

    let maximalNumberOfConcurrentThreads = Telemetry.maximalNumberOfConcurrentThreads;
    if (maximalNumberOfConcurrentThreads)
      ret.maximalNumberOfConcurrentThreads = maximalNumberOfConcurrentThreads;

    for (let ioCounter in this._startupIO)
      ret[ioCounter] = this._startupIO[ioCounter];

    let hasPingBeenSent = false;
    try {
      hasPingBeenSent = Telemetry.getHistogramById("TELEMETRY_SUCCESS").snapshot().sum > 0;
    } catch(e) {
    }
    if (!forSavedSession || hasPingBeenSent) {
      ret.savedPings = TelemetryFile.pingsLoaded;
    }

    ret.pingsOverdue = TelemetryFile.pingsOverdue;
    ret.pingsDiscarded = TelemetryFile.pingsDiscarded;

    return ret;
  },

  /**
   * When reflecting a histogram into JS, Telemetry hands us an object
   * with the following properties:
   *
   * - min, max, histogram_type, sum, sum_squares_{lo,hi}: simple integers;
   * - log_sum, log_sum_squares: doubles;
   * - counts: array of counts for histogram buckets;
   * - ranges: array of calculated bucket sizes.
   *
   * This format is not straightforward to read and potentially bulky
   * with lots of zeros in the counts array.  Packing histograms makes
   * raw histograms easier to read and compresses the data a little bit.
   *
   * Returns an object:
   * { range: [min, max], bucket_count: <number of buckets>,
   *   histogram_type: <histogram_type>, sum: <sum>,
   *   sum_squares_lo: <sum_squares_lo>,
   *   sum_squares_hi: <sum_squares_hi>,
   *   log_sum: <log_sum>, log_sum_squares: <log_sum_squares>,
   *   values: { bucket1: count1, bucket2: count2, ... } }
   */
  packHistogram: function packHistogram(hgram) {
    let r = hgram.ranges;;
    let c = hgram.counts;
    let retgram = {
      range: [r[1], r[r.length - 1]],
      bucket_count: r.length,
      histogram_type: hgram.histogram_type,
      values: {},
      sum: hgram.sum
    };

    if (hgram.histogram_type == Telemetry.HISTOGRAM_EXPONENTIAL) {
      retgram.log_sum = hgram.log_sum;
      retgram.log_sum_squares = hgram.log_sum_squares;
    } else {
      retgram.sum_squares_lo = hgram.sum_squares_lo;
      retgram.sum_squares_hi = hgram.sum_squares_hi;
    }

    let first = true;
    let last = 0;

    for (let i = 0; i < c.length; i++) {
      let value = c[i];
      if (!value)
        continue;

      // add a lower bound
      if (i && first) {
        retgram.values[r[i - 1]] = 0;
      }
      first = false;
      last = i + 1;
      retgram.values[r[i]] = value;
    }

    // add an upper bound
    if (last && last < c.length)
      retgram.values[r[last]] = 0;
    return retgram;
  },

  getHistograms: function getHistograms(hls) {
    let registered = Telemetry.registeredHistograms([]);
    let ret = {};

    for (let name of registered) {
      for (let n of [name, "STARTUP_" + name]) {
        if (n in hls) {
          ret[n] = this.packHistogram(hls[n]);
        }
      }
    }

    return ret;
  },

  getAddonHistograms: function getAddonHistograms() {
    let ahs = Telemetry.addonHistogramSnapshots;
    let ret = {};

    for (let addonName in ahs) {
      let addonHistograms = ahs[addonName];
      let packedHistograms = {};
      for (let name in addonHistograms) {
        packedHistograms[name] = this.packHistogram(addonHistograms[name]);
      }
      if (Object.keys(packedHistograms).length != 0)
        ret[addonName] = packedHistograms;
    }

    return ret;
  },

  getThreadHangStats: function getThreadHangStats(stats) {
    stats.forEach((thread) => {
      thread.activity = this.packHistogram(thread.activity);
      thread.hangs.forEach((hang) => {
        hang.histogram = this.packHistogram(hang.histogram);
      });
    });
    return stats;
  },

  /**
   * Descriptive metadata
   *
   * @param  reason
   *         The reason for the telemetry ping, this will be included in the
   *         returned metadata,
   * @return The metadata as a JS object
   */
  getMetadata: function getMetadata(reason) {
    let ai = Services.appinfo;
    let ret = {
      reason: reason,
      OS: ai.OS,
      appID: ai.ID,
      appVersion: ai.version,
      appName: ai.name,
      appBuildID: ai.appBuildID,
      appUpdateChannel: UpdateChannel.get(),
      platformBuildID: ai.platformBuildID,
      revision: HISTOGRAMS_FILE_VERSION,
      locale: getLocale()
    };
    if (this._previousBuildID) {
      ret.previousBuildID = this._previousBuildID;
    }

    // sysinfo fields are not always available, get what we can.
    let sysInfo = Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2);
    let fields = ["cpucount", "memsize", "arch", "version", "kernel_version",
                  "device", "manufacturer", "hardware",
                  "hasMMX", "hasSSE", "hasSSE2", "hasSSE3",
                  "hasSSSE3", "hasSSE4A", "hasSSE4_1", "hasSSE4_2",
                  "hasEDSP", "hasARMv6", "hasARMv7", "hasNEON", "isWow64",
                  "profileHDDModel", "profileHDDRevision", "binHDDModel",
                  "binHDDRevision", "winHDDModel", "winHDDRevision"];
    for each (let field in fields) {
      let value;
      try {
        value = sysInfo.getProperty(field);
      } catch (e) {
        continue;
      }
      if (field == "memsize") {
        // Send RAM size in megabytes. Rounding because sysinfo doesn't
        // always provide RAM in multiples of 1024.
        value = Math.round(value / 1024 / 1024);
      }
      ret[field] = value;
    }

    // gfxInfo fields are not always available, get what we can.
    let gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
    let gfxfields = ["adapterDescription", "adapterVendorID", "adapterDeviceID",
                     "adapterRAM", "adapterDriver", "adapterDriverVersion",
                     "adapterDriverDate", "adapterDescription2",
                     "adapterVendorID2", "adapterDeviceID2", "adapterRAM2",
                     "adapterDriver2", "adapterDriverVersion2",
                     "adapterDriverDate2", "isGPU2Active", "D2DEnabled",
                     "DWriteEnabled", "DWriteVersion"
                    ];

    if (gfxInfo) {
      for each (let field in gfxfields) {
        try {
          let value = gfxInfo[field];
          // bug 940806: We need to do a strict equality comparison here,
          // otherwise a type conversion will occur and boolean false values
          // will get filtered out
          if (value !== "") {
            ret[field] = value;
          }
        } catch (e) {
          continue
        }
      }
    }

#ifndef MOZ_WIDGET_GONK
    let theme = LightweightThemeManager.currentTheme;
    if (theme)
      ret.persona = theme.id;
#endif

    if (this._addons)
      ret.addons = this._addons;

    let flashVersion = this.getFlashVersion();
    if (flashVersion)
      ret.flashVersion = flashVersion;

    return ret;
  },

  /**
   * Pull values from about:memory into corresponding histograms
   */
  gatherMemory: function gatherMemory() {
    let mgr;
    try {
      mgr = Cc["@mozilla.org/memory-reporter-manager;1"].
            getService(Ci.nsIMemoryReporterManager);
    } catch (e) {
      // OK to skip memory reporters in xpcshell
      return;
    }

    let histogram = Telemetry.getHistogramById("TELEMETRY_MEMORY_REPORTER_MS");
    let startTime = new Date();

    // Get memory measurements from distinguished amount attributes.  We used
    // to measure "explicit" too, but it could cause hangs, and the data was
    // always really noisy anyway.  See bug 859657.
    //
    // test_TelemetryPing.js relies on some of these histograms being
    // here.  If you remove any of the following histograms from here, you'll
    // have to modify test_TelemetryPing.js:
    //
    //   * MEMORY_JS_GC_HEAP, and
    //   * MEMORY_JS_COMPARTMENTS_SYSTEM.
    //
    // The distinguished amount attribute names don't match the telemetry id
    // names in some cases due to a combination of (a) historical reasons, and
    // (b) the fact that we can't change telemetry id names without breaking
    // data continuity.
    //
    let boundHandleMemoryReport = this.handleMemoryReport.bind(this);
    function h(id, units, amountName) {
      try {
        // If mgr[amountName] throws an exception, just move on -- some amounts
        // aren't available on all platforms.  But if the attribute simply
        // isn't present, that indicates the distinguished amounts have changed
        // and this file hasn't been updated appropriately.
        let amount = mgr[amountName];
        NS_ASSERT(amount !== undefined,
                  "telemetry accessed an unknown distinguished amount");
        boundHandleMemoryReport(id, units, amount);
      } catch (e) {
      };
    }
    let b = (id, n) => h(id, Ci.nsIMemoryReporter.UNITS_BYTES, n);
    let c = (id, n) => h(id, Ci.nsIMemoryReporter.UNITS_COUNT, n);
    let cc= (id, n) => h(id, Ci.nsIMemoryReporter.UNITS_COUNT_CUMULATIVE, n);
    let p = (id, n) => h(id, Ci.nsIMemoryReporter.UNITS_PERCENTAGE, n);

    b("MEMORY_VSIZE", "vsize");
    b("MEMORY_VSIZE_MAX_CONTIGUOUS", "vsizeMaxContiguous");
    b("MEMORY_RESIDENT", "residentFast");
    b("MEMORY_HEAP_ALLOCATED", "heapAllocated");
    p("MEMORY_HEAP_COMMITTED_UNUSED_RATIO", "heapOverheadRatio");
    b("MEMORY_JS_GC_HEAP", "JSMainRuntimeGCHeap");
    b("MEMORY_JS_MAIN_RUNTIME_TEMPORARY_PEAK", "JSMainRuntimeTemporaryPeak");
    c("MEMORY_JS_COMPARTMENTS_SYSTEM", "JSMainRuntimeCompartmentsSystem");
    c("MEMORY_JS_COMPARTMENTS_USER", "JSMainRuntimeCompartmentsUser");
    b("MEMORY_IMAGES_CONTENT_USED_UNCOMPRESSED", "imagesContentUsedUncompressed");
    b("MEMORY_STORAGE_SQLITE", "storageSQLite");
    cc("MEMORY_EVENTS_VIRTUAL", "lowMemoryEventsVirtual");
    cc("MEMORY_EVENTS_PHYSICAL", "lowMemoryEventsPhysical");
    c("GHOST_WINDOWS", "ghostWindows");
    cc("PAGE_FAULTS_HARD", "pageFaultsHard");

    histogram.add(new Date() - startTime);
  },

  handleMemoryReport: function(id, units, amount) {
    let val;
    if (units == Ci.nsIMemoryReporter.UNITS_BYTES) {
      val = Math.floor(amount / 1024);
    }
    else if (units == Ci.nsIMemoryReporter.UNITS_PERCENTAGE) {
      // UNITS_PERCENTAGE amounts are 100x greater than their raw value.
      val = Math.floor(amount / 100);
    }
    else if (units == Ci.nsIMemoryReporter.UNITS_COUNT) {
      val = amount;
    }
    else if (units == Ci.nsIMemoryReporter.UNITS_COUNT_CUMULATIVE) {
      // If the reporter gives us a cumulative count, we'll report the
      // difference in its value between now and our previous ping.

      if (!(id in this._prevValues)) {
        // If this is the first time we're reading this reporter, store its
        // current value but don't report it in the telemetry ping, so we
        // ignore the effect startup had on the reporter.
        this._prevValues[id] = amount;
        return;
      }

      val = amount - this._prevValues[id];
      this._prevValues[id] = amount;
    }
    else {
      NS_ASSERT(false, "Can't handle memory reporter with units " + units);
      return;
    }

    let h = this._histograms[id];
    if (!h) {
      h = Telemetry.getHistogramById(id);
      this._histograms[id] = h;
    }
    h.add(val);
  },

  /**
   * Return true if we're interested in having a STARTUP_* histogram for
   * the given histogram name.
   */
  isInterestingStartupHistogram: function isInterestingStartupHistogram(name) {
    return this._startupHistogramRegex.test(name);
  },

  /**
   * Make a copy of interesting histograms at startup.
   */
  gatherStartupHistograms: function gatherStartupHistograms() {
    let info = Telemetry.registeredHistograms([]);
    let snapshots = Telemetry.histogramSnapshots;
    for (let name of info) {
      // Only duplicate histograms with actual data.
      if (this.isInterestingStartupHistogram(name) && name in snapshots) {
        Telemetry.histogramFrom("STARTUP_" + name, name);
      }
    }
  },

  /**
   * Get the current session's payload using the provided
   * simpleMeasurements and info, which are typically obtained by a call
   * to |this.getSimpleMeasurements| and |this.getMetadata|,
   * respectively.
   */
  assemblePayloadWithMeasurements: function assemblePayloadWithMeasurements(simpleMeasurements, info) {
    let payloadObj = {
      ver: PAYLOAD_VERSION,
      simpleMeasurements: simpleMeasurements,
      histograms: this.getHistograms(Telemetry.histogramSnapshots),
      slowSQL: Telemetry.slowSQL,
      chromeHangs: Telemetry.chromeHangs,
      threadHangStats: this.getThreadHangStats(Telemetry.threadHangStats),
      lateWrites: Telemetry.lateWrites,
      addonHistograms: this.getAddonHistograms(),
      addonDetails: AddonManagerPrivate.getTelemetryDetails(),
      UIMeasurements: UITelemetry.getUIMeasurements(),
      info: info
    };

    if (Object.keys(this._slowSQLStartup).length != 0 &&
        (Object.keys(this._slowSQLStartup.mainThread).length ||
         Object.keys(this._slowSQLStartup.otherThreads).length)) {
      payloadObj.slowSQLStartup = this._slowSQLStartup;
    }

    return payloadObj;
  },

  getSessionPayload: function getSessionPayload(reason) {
    let measurements = this.getSimpleMeasurements(reason == "saved-session");
    let info = this.getMetadata(reason);
    return this.assemblePayloadWithMeasurements(measurements, info);
  },

  assemblePing: function assemblePing(payloadObj, reason) {
    let slug = this._uuid;
    return { slug: slug, reason: reason, payload: payloadObj };
  },

  getSessionPayloadAndSlug: function getSessionPayloadAndSlug(reason) {
    return this.assemblePing(this.getSessionPayload(reason), reason);
  },

  popPayloads: function popPayloads(reason) {
    function payloadIter() {
      if (reason != "overdue-flush") {
        yield this.getSessionPayloadAndSlug(reason);
      }
      let iterator = TelemetryFile.popPendingPings(reason);
      for (let data of iterator) {
        yield data;
      }
    }

    let payloadIterWithThis = payloadIter.bind(this);
    return { __iterator__: payloadIterWithThis };
  },

  /**
   * Send data to the server. Record success/send-time in histograms
   */
  send: function send(reason, server) {
    // populate histograms one last time
    this.gatherMemory();
    this.sendPingsFromIterator(server, reason,
                               Iterator(this.popPayloads(reason)));
  },

  /**
   * What we want to do is the following:
   *
   * for data in getPayloads(reason):
   *   if sending ping data to server failed:
   *     break;
   *
   * but we can't do that, since XMLHttpRequest is async.  What we do
   * instead is let this function control the essential looping logic
   * and provide callbacks for XMLHttpRequest when a request has
   * finished.
   */
  sendPingsFromIterator: function sendPingsFromIterator(server, reason, i) {
    function finishPings(reason) {
      if (reason == "test-ping") {
        Services.obs.notifyObservers(null, "telemetry-test-xhr-complete", null);
      }
    }

    let data = null;
    try {
      data = i.next();
    } catch (e if e instanceof StopIteration) {
      finishPings(reason);
      return;
    }
    function onSuccess() {
      this.sendPingsFromIterator(server, reason, i);
    }
    function onError() {
      TelemetryFile.savePing(data, true);
      // Notify that testing is complete, even if we didn't send everything.
      finishPings(reason);
    }
    this.doPing(server, data,
                onSuccess.bind(this), onError.bind(this));
  },

  finishPingRequest: function finishPingRequest(success, startTime, ping) {
    let hping = Telemetry.getHistogramById("TELEMETRY_PING");
    let hsuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");

    hsuccess.add(success);
    hping.add(new Date() - startTime);

    if (success) {
      TelemetryFile.cleanupPingFile(ping);
    }
  },

  submissionPath: function submissionPath(ping) {
    let slug;
    if (!ping) {
      slug = this._uuid;
    } else {
      let info = ping.payload.info;
      let pathComponents = [ping.slug, info.reason, info.appName,
                            info.appVersion, info.appUpdateChannel,
                            info.appBuildID];
      slug = pathComponents.join("/");
    }
    return "/submit/telemetry/" + slug;
  },

  doPing: function doPing(server, ping, onSuccess, onError) {
    let url = server + this.submissionPath(ping);
    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance(Ci.nsIXMLHttpRequest);
    request.mozBackgroundRequest = true;
    request.open("POST", url, true);
    request.overrideMimeType("text/plain");
    request.setRequestHeader("Content-Type", "application/json; charset=UTF-8");

    let startTime = new Date();

    function handler(success, callback) {
      return function(event) {
        this.finishPingRequest(success, startTime, ping);
        callback();
      };
    }
    request.addEventListener("error", handler(false, onError).bind(this), false);
    request.addEventListener("load", handler(true, onSuccess).bind(this), false);

    request.setRequestHeader("Content-Encoding", "gzip");
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    let utf8Payload = converter.ConvertFromUnicode(JSON.stringify(ping.payload));
    utf8Payload += converter.Finish();
    let payloadStream = Cc["@mozilla.org/io/string-input-stream;1"]
                        .createInstance(Ci.nsIStringInputStream);
    payloadStream.data = this.gzipCompressString(utf8Payload);
    request.send(payloadStream);
  },

  gzipCompressString: function gzipCompressString(string) {
    let observer = {
      buffer: "",
      onStreamComplete: function(loader, context, status, length, result) {
        this.buffer = String.fromCharCode.apply(this, result);
      }
    };

    let scs = Cc["@mozilla.org/streamConverters;1"]
              .getService(Ci.nsIStreamConverterService);
    let listener = Cc["@mozilla.org/network/stream-loader;1"]
                  .createInstance(Ci.nsIStreamLoader);
    listener.init(observer);
    let converter = scs.asyncConvertData("uncompressed", "gzip",
                                         listener, null);
    let stringStream = Cc["@mozilla.org/io/string-input-stream;1"]
                       .createInstance(Ci.nsIStringInputStream);
    stringStream.data = string;
    converter.onStartRequest(null, null);
    converter.onDataAvailable(null, null, stringStream, 0, string.length);
    converter.onStopRequest(null, null, null);
    return observer.buffer;
  },

  attachObservers: function attachObservers() {
    if (!this._initialized)
      return;
    Services.obs.addObserver(this, "cycle-collector-begin", false);
    Services.obs.addObserver(this, "idle-daily", false);
  },

  detachObservers: function detachObservers() {
    if (!this._initialized)
      return;
    Services.obs.removeObserver(this, "idle-daily");
    Services.obs.removeObserver(this, "cycle-collector-begin");
    if (this._isIdleObserver) {
      idleService.removeIdleObserver(this, IDLE_TIMEOUT_SECONDS);
      this._isIdleObserver = false;
    }
  },

  /**
   * Initializes telemetry within a timer. If there is no PREF_SERVER set, don't turn on telemetry.
   */
  setup: function setup(aTesting) {
    // Initialize some probes that are kept in their own modules
    this._thirdPartyCookies = new ThirdPartyCookieProbe();
    this._thirdPartyCookies.init();

    // Record old value and update build ID preference if this is the first
    // run with a new build ID.
    let previousBuildID = undefined;
    try {
      previousBuildID = Services.prefs.getCharPref(PREF_PREVIOUS_BUILDID);
    } catch (e) {
      // Preference was not set.
    }
    let thisBuildID = Services.appinfo.appBuildID;
    // If there is no previousBuildID preference, this._previousBuildID remains
    // undefined so no value is sent in the telemetry metadata.
    if (previousBuildID != thisBuildID) {
      this._previousBuildID = previousBuildID;
      Services.prefs.setCharPref(PREF_PREVIOUS_BUILDID, thisBuildID);
    }

#ifdef MOZILLA_OFFICIAL
    if (!Telemetry.canSend) {
      // We can't send data; no point in initializing observers etc.
      // Only do this for official builds so that e.g. developer builds
      // still enable Telemetry based on prefs.
      Telemetry.canRecord = false;
      return;
    }
#endif
    let enabled = false;
    try {
      enabled = Services.prefs.getBoolPref(PREF_ENABLED);
      this._server = Services.prefs.getCharPref(PREF_SERVER);
    } catch (e) {
      // Prerequesite prefs aren't set
    }
    if (!enabled) {
      // Turn off local telemetry if telemetry is disabled.
      // This may change once about:telemetry is added.
      Telemetry.canRecord = false;
      return;
    }
    Services.obs.addObserver(this, "profile-before-change2", false);
    Services.obs.addObserver(this, "sessionstore-windows-restored", false);
    Services.obs.addObserver(this, "quit-application-granted", false);
#ifdef MOZ_WIDGET_ANDROID
    Services.obs.addObserver(this, "application-background", false);
#endif
    Services.obs.addObserver(this, "xul-window-visible", false);
    this._hasWindowRestoredObserver = true;
    this._hasXulWindowVisibleObserver = true;

    // Delay full telemetry initialization to give the browser time to
    // run various late initializers. Otherwise our gathered memory
    // footprint and other numbers would be too optimistic.
    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    function timerCallback() {
      this._initialized = true;
      TelemetryFile.loadSavedPings(false, (success =>
        {
          let success_histogram = Telemetry.getHistogramById("READ_SAVED_PING_SUCCESS");
          success_histogram.add(success);
        }), () =>
        {
          // If we have any TelemetryPings lying around, we'll be aggressive
          // and try to send them all off ASAP.
          if (TelemetryFile.pingsOverdue > 0) {
            // It doesn't really matter what we pass to this.send as a reason,
            // since it's never sent to the server. All that this.send does with
            // the reason is check to make sure it's not a test-ping.
            this.send("overdue-flush", this._server);
          }
        });
      this.attachObservers();
      this.gatherMemory();

      Telemetry.asyncFetchTelemetryData(function () {
      });
      delete this._timer;
    }
    this._timer.initWithCallback(timerCallback.bind(this),
                                 aTesting ? TELEMETRY_TEST_DELAY : TELEMETRY_DELAY,
                                 Ci.nsITimer.TYPE_ONE_SHOT);
  },

  testLoadHistograms: function testLoadHistograms(file, sync) {
    TelemetryFile.testLoadHistograms(file, sync, (success =>
        {
          let success_histogram = Telemetry.getHistogramById("READ_SAVED_PING_SUCCESS");
          success_histogram.add(success);
        }));
  },

  getFlashVersion: function getFlashVersion() {
    let host = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let tags = host.getPluginTags();

    for (let i = 0; i < tags.length; i++) {
      if (tags[i].name == "Shockwave Flash")
        return tags[i].version;
    }

    return null;
  },

  savePendingPings: function savePendingPings() {
    let sessionPing = this.getSessionPayloadAndSlug("saved-session");
    TelemetryFile.savePendingPings(sessionPing);
  },

  saveHistograms: function saveHistograms(file, sync) {
    TelemetryFile.savePingToFile(
      this.getSessionPayloadAndSlug("saved-session"),
      file, sync, true);
  },

  /**
   * Remove observers to avoid leaks
   */
  uninstall: function uninstall() {
    this.detachObservers();
    if (this._hasWindowRestoredObserver) {
      Services.obs.removeObserver(this, "sessionstore-windows-restored");
      this._hasWindowRestoredObserver = false;
    }
    if (this._hasXulWindowVisibleObserver) {
      Services.obs.removeObserver(this, "xul-window-visible");
      this._hasXulWindowVisibleObserver = false;
    }
    Services.obs.removeObserver(this, "profile-before-change2");
    Services.obs.removeObserver(this, "quit-application-granted");
#ifdef MOZ_WIDGET_ANDROID
    Services.obs.removeObserver(this, "application-background", false);
#endif
  },

  getPayload: function getPayload() {
    // This function returns the current Telemetry payload to the caller.
    // We only gather startup info once.
    if (Object.keys(this._slowSQLStartup).length == 0) {
      this.gatherStartupHistograms();
      this._slowSQLStartup = Telemetry.slowSQL;
    }
    this.gatherMemory();
    return this.getSessionPayload("gather-payload");
  },

  gatherStartup: function gatherStartup() {
    let counters = processInfo.getCounters();
    if (counters) {
      [this._startupIO.startupSessionRestoreReadBytes,
        this._startupIO.startupSessionRestoreWriteBytes] = counters;
    }
    this.gatherStartupHistograms();
    this._slowSQLStartup = Telemetry.slowSQL;
  },

  enableLoadSaveNotifications: function enableLoadSaveNotifications() {
    TelemetryFile.shouldNotifyUponSave = true;
  },

  setAddOns: function setAddOns(aAddOns) {
    this._addons = aAddOns;
  },

  sendIdlePing: function sendIdlePing(aTest, aServer) {
    if (this._isIdleObserver) {
      idleService.removeIdleObserver(this, IDLE_TIMEOUT_SECONDS);
      this._isIdleObserver = false;
    }
    if (aTest) {
      this.send("test-ping", aServer);
    } else if (Telemetry.canSend) {
      this.send("idle-daily", aServer);
    }
  },

  testPing: function testPing(server) {
    this.sendIdlePing(true, server);
  },

  cacheProfileDirectory: function cacheProfileDirectory() {
    // This method doesn't do anything anymore
    return;
  },

  /**
   * This observer drives telemetry.
   */
  observe: function (aSubject, aTopic, aData) {
    switch (aTopic) {
    case "profile-after-change":
      this.setup();
      break;
    case "cycle-collector-begin":
      let now = new Date();
      if (!gLastMemoryPoll
          || (TELEMETRY_INTERVAL <= now - gLastMemoryPoll)) {
        gLastMemoryPoll = now;
        this.gatherMemory();
      }
      break;
    case "xul-window-visible":
      Services.obs.removeObserver(this, "xul-window-visible");
      this._hasXulWindowVisibleObserver = false;
      var counters = processInfo.getCounters();
      if (counters) {
        [this._startupIO.startupWindowVisibleReadBytes,
          this._startupIO.startupWindowVisibleWriteBytes] = counters;
      }
      break;
    case "sessionstore-windows-restored":
      Services.obs.removeObserver(this, "sessionstore-windows-restored");
      this._hasWindowRestoredObserver = false;
      // Check whether debugger was attached during startup
      let debugService = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
      gWasDebuggerAttached = debugService.isDebuggerAttached;
      this.gatherStartup();
      break;
    case "idle-daily":
      // Enqueue to main-thread, otherwise components may be inited by the
      // idle-daily category and miss the gather-telemetry notification.
      Services.tm.mainThread.dispatch((function() {
        // Notify that data should be gathered now, since ping will happen soon.
        Services.obs.notifyObservers(null, "gather-telemetry", null);
        // The ping happens at the first idle of length IDLE_TIMEOUT_SECONDS.
        idleService.addIdleObserver(this, IDLE_TIMEOUT_SECONDS);
        this._isIdleObserver = true;
      }).bind(this), Ci.nsIThread.DISPATCH_NORMAL);
      break;
    case "idle":
      this.sendIdlePing(false, this._server);
      break;
    case "profile-before-change2":
      this.uninstall();
      if (Telemetry.canSend) {
        this.savePendingPings();
      }
      break;

#ifdef MOZ_WIDGET_ANDROID
    // On Android, we can get killed without warning once we are in the background,
    // but we may also submit data and/or come back into the foreground without getting
    // killed. To deal with this, we save the current session data to file when we are
    // put into the background. This handles the following post-backgrounding scenarios:
    // 1) We are killed immediately. In this case the current session data (which we
    //    save to a file) will be loaded and submitted on a future run.
    // 2) We submit the data while in the background, and then are killed. In this case
    //    the file that we saved will be deleted by the usual process in
    //    finishPingRequest after it is submitted.
    // 3) We submit the data, and then come back into the foreground. Same as case (2).
    // 4) We do not submit the data, but come back into the foreground. In this case
    //    we have the option of either deleting the file that we saved (since we will either
    //    send the live data while in the foreground, or create the file again on the next
    //    backgrounding), or not (in which case we will delete it on submit, or overwrite
    //    it on the next backgrounding). Not deleting it is faster, so that's what we do.
    case "application-background":
      if (Telemetry.canSend) {
        let ping = this.getSessionPayloadAndSlug("saved-session");
        TelemetryFile.savePing(ping, true);
      }
      break;
#endif
    }
  }
};
