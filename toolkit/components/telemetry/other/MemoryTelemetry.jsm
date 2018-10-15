/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/DeferredTask.jsm", this);
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  GCTelemetry: "resource://gre/modules/GCTelemetry.jsm",
});

const Utils = TelemetryUtils;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "MemoryTelemetry" + (Utils.isContentProcess ? "#content::" : "::");

const MESSAGE_TELEMETRY_USS = "Telemetry:USS";
const MESSAGE_TELEMETRY_GET_CHILD_USS = "Telemetry:GetChildUSS";

// Do not gather data more than once a minute (ms)
const TELEMETRY_INTERVAL = 60 * 1000;
// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY = Services.prefs.getIntPref("toolkit.telemetry.initDelay", 60) * 1000;
// Delay before initializing telemetry if we're testing (ms)
const TELEMETRY_TEST_DELAY = 1;

const TOPIC_CYCLE_COLLECTOR_BEGIN = "cycle-collector-begin";

// How long to wait in millis for all the child memory reports to come in
const TOTAL_MEMORY_COLLECTOR_TIMEOUT = 200;

var gLastMemoryPoll = null;

XPCOMUtils.defineLazyServiceGetters(this, {
  Telemetry: ["@mozilla.org/base/telemetry;1", "nsITelemetry"],
});

var EXPORTED_SYMBOLS = ["MemoryTelemetry"];

var MemoryTelemetry = Object.freeze({
  /**
   * Pull values from about:memory into corresponding histograms
   */
  gatherMemory() {
    return Impl.gatherMemory();
  },

  /**
   * Triggers shutdown of the module.
   */
  shutdown() {
    return Impl.shutdown();
  },

  /**
   * Sets up components used in the content process.
   */
  setupContent(testing = false) {
    return Impl.setupContent(testing);
  },

  /**
   * Lightweight init function, called as soon as Firefox starts.
   */
  earlyInit(aTesting = false) {
    return Impl.earlyInit(aTesting);
  },

  /**
   * Does the "heavy" Telemetry initialization later on, so we
   * don't impact startup performance.
   * @return {Promise} Resolved when the initialization completes.
   */
  delayedInit() {
    return Impl.delayedInit();
  },
});

var Impl = {
  _initialized: false,
  _logger: null,
  _prevValues: {},
  // A task performing delayed initialization of the chrome process
  _delayedInitTask: null,
  // Need a timeout in case children are tardy in giving back their memory reports.
  _totalMemoryTimeout: undefined,
  _testing: false,
  // An accumulator of total memory across all processes. Only valid once the final child reports.
  _totalMemory: null,
  // A Set of outstanding USS report ids
  _childrenToHearFrom: null,
  // monotonically-increasing id for USS reports
  _nextTotalMemoryId: 1,
  _USSFromChildProcesses: null,
  // Keep track of the active observers
  _observedTopics: new Set(),

  addObserver(aTopic) {
    Services.obs.addObserver(this, aTopic);
    this._observedTopics.add(aTopic);
  },

  removeObserver(aTopic) {
    Services.obs.removeObserver(this, aTopic);
    this._observedTopics.delete(aTopic);
  },

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }
    return this._logger;
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
    // test_TelemetrySession.js relies on some of these histograms being
    // here.  If you remove any of the following histograms from here, you'll
    // have to modify test_TelemetrySession.js:
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
    let h = (id, units, amountName) => {
      try {
        // If mgr[amountName] throws an exception, just move on -- some amounts
        // aren't available on all platforms.  But if the attribute simply
        // isn't present, that indicates the distinguished amounts have changed
        // and this file hasn't been updated appropriately.
        let amount = mgr[amountName];
        if (amount === undefined) {
          this._log.error("gatherMemory - telemetry accessed an unknown distinguished amount");
        }
        boundHandleMemoryReport(id, units, amount);
      } catch (e) {
      }
    };
    let b = (id, n) => h(id, Ci.nsIMemoryReporter.UNITS_BYTES, n);
    let c = (id, n) => h(id, Ci.nsIMemoryReporter.UNITS_COUNT, n);
    let cc = (id, n) => h(id, Ci.nsIMemoryReporter.UNITS_COUNT_CUMULATIVE, n);
    let p = (id, n) => h(id, Ci.nsIMemoryReporter.UNITS_PERCENTAGE, n);

    // GHOST_WINDOWS is opt-out as of Firefox 55
    c("GHOST_WINDOWS", "ghostWindows");

    if (!Telemetry.canRecordExtended) {
      return;
    }

    b("MEMORY_VSIZE", "vsize");
    if (!Services.appinfo.is64Bit || AppConstants.platform !== "win") {
      b("MEMORY_VSIZE_MAX_CONTIGUOUS", "vsizeMaxContiguous");
    }
    b("MEMORY_RESIDENT_FAST", "residentFast");
    b("MEMORY_UNIQUE", "residentUnique");
    p("MEMORY_HEAP_OVERHEAD_FRACTION", "heapOverheadFraction");
    b("MEMORY_JS_GC_HEAP", "JSMainRuntimeGCHeap");
    c("MEMORY_JS_COMPARTMENTS_SYSTEM", "JSMainRuntimeRealmsSystem");
    c("MEMORY_JS_COMPARTMENTS_USER", "JSMainRuntimeRealmsUser");
    b("MEMORY_IMAGES_CONTENT_USED_UNCOMPRESSED", "imagesContentUsedUncompressed");
    b("MEMORY_STORAGE_SQLITE", "storageSQLite");
    cc("LOW_MEMORY_EVENTS_VIRTUAL", "lowMemoryEventsVirtual");
    cc("LOW_MEMORY_EVENTS_COMMIT_SPACE", "lowMemoryEventsCommitSpace");
    cc("LOW_MEMORY_EVENTS_PHYSICAL", "lowMemoryEventsPhysical");
    cc("PAGE_FAULTS_HARD", "pageFaultsHard");

    try {
      mgr.getHeapAllocatedAsync(heapAllocated => {
        boundHandleMemoryReport("MEMORY_HEAP_ALLOCATED",
                                Ci.nsIMemoryReporter.UNITS_BYTES,
                                heapAllocated);
      });
    } catch (e) {
    }

    if (!Utils.isContentProcess && !this._totalMemoryTimeout) {
      // Only the chrome process should gather total memory
      // total = parent RSS + sum(child USS)
      this._totalMemory = mgr.residentFast;
      if (Services.ppmm.childCount > 1) {
        // Do not report If we time out waiting for the children to call
        this._totalMemoryTimeout = setTimeout(
          () => {
            this._totalMemoryTimeout = undefined;
            this._childrenToHearFrom.clear();
          },
          TOTAL_MEMORY_COLLECTOR_TIMEOUT);
        this._USSFromChildProcesses = [];
        this._childrenToHearFrom = new Set();
        for (let i = 1; i < Services.ppmm.childCount; i++) {
          let child = Services.ppmm.getChildAt(i);
          try {
            child.sendAsyncMessage(MESSAGE_TELEMETRY_GET_CHILD_USS, {id: this._nextTotalMemoryId});
            this._childrenToHearFrom.add(this._nextTotalMemoryId);
            this._nextTotalMemoryId++;
          } catch (ex) {
            // If a content process has just crashed, then attempting to send it
            // an async message will throw an exception.
            Cu.reportError(ex);
          }
        }
      } else {
        boundHandleMemoryReport(
          "MEMORY_TOTAL",
          Ci.nsIMemoryReporter.UNITS_BYTES,
          this._totalMemory);
      }
    }

    histogram.add(new Date() - startTime);
  },

  handleMemoryReport(id, units, amount, key) {
    let val;
    if (units == Ci.nsIMemoryReporter.UNITS_BYTES) {
      val = Math.floor(amount / 1024);
    } else if (units == Ci.nsIMemoryReporter.UNITS_PERCENTAGE) {
      // UNITS_PERCENTAGE amounts are 100x greater than their raw value.
      val = Math.floor(amount / 100);
    } else if (units == Ci.nsIMemoryReporter.UNITS_COUNT) {
      val = amount;
    } else if (units == Ci.nsIMemoryReporter.UNITS_COUNT_CUMULATIVE) {
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
    } else {
      this._log.error("handleMemoryReport - Can't handle memory reporter with units " + units);
      return;
    }

    if (key) {
      Telemetry.getKeyedHistogramById(id).add(key, val);
      return;
    }

    Telemetry.getHistogramById(id).add(val);
  },

  attachObservers: function attachObservers() {
    if (!this._initialized)
      return;
    if (Telemetry.canRecordExtended) {
      this.addObserver(TOPIC_CYCLE_COLLECTOR_BEGIN);
    }
  },


  /**
   * Lightweight init function, called as soon as Firefox starts.
   */
  earlyInit(testing) {
    this._log.trace("earlyInit");

    this._initStarted = true;
    this._testing = testing;

    if (this._initialized && !testing) {
      this._log.error("earlyInit - already initialized");
      return;
    }

    if (!Telemetry.canRecordBase && !testing) {
      this._log.config("earlyInit - Telemetry recording is disabled, skipping Chrome process setup.");
      return;
    }

    Services.ppmm.addMessageListener(MESSAGE_TELEMETRY_USS, this);
  },

  /**
   * Does the "heavy" Telemetry initialization later on, so we
   * don't impact startup performance.
   * @return {Promise} Resolved when the initialization completes.
   */
  delayedInit() {
    this._log.trace("delayedInit");

    this._delayedInitTask = (async () => {
      try {
        this._initialized = true;

        this.attachObservers();
        this.gatherMemory();

        if (Telemetry.canRecordExtended) {
          GCTelemetry.init();
        }

        this._delayedInitTask = null;
      } catch (e) {
        this._delayedInitTask = null;
        throw e;
      }
    })();

    return this._delayedInitTask;
  },

  /**
   * Initializes telemetry for a content process.
   */
  setupContent: function setupContent(testing) {
    this._log.trace("setupContent");
    this._testing = testing;

    if (!Telemetry.canRecordBase) {
      this._log.trace("setupContent - base recording is disabled, not initializing");
      return;
    }

    this.addObserver("content-child-shutdown");
    Services.cpmm.addMessageListener(MESSAGE_TELEMETRY_GET_CHILD_USS, this);

    let delayedTask = new DeferredTask(() => {
      this._initialized = true;

      this.attachObservers();
      this.gatherMemory();

      if (Telemetry.canRecordExtended) {
        GCTelemetry.init();
      }
    }, testing ? TELEMETRY_TEST_DELAY : TELEMETRY_DELAY,
    testing ? 0 : undefined);

    delayedTask.arm();
  },

  getOpenTabsCount: function getOpenTabsCount() {
    let tabCount = 0;

    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      tabCount += win.gBrowser.tabs.length;
    }

    return tabCount;
  },

  receiveMessage: function receiveMessage(message) {
    this._log.trace("receiveMessage - Message name " + message.name);
    switch (message.name) {
    case MESSAGE_TELEMETRY_USS:
    {
      // In parent process, receive the USS report from the child
      if (this._totalMemoryTimeout && this._childrenToHearFrom.delete(message.data.id)) {
        let uss = message.data.bytes;
        this._totalMemory += uss;
        this._USSFromChildProcesses.push(uss);
        if (this._childrenToHearFrom.size == 0) {
          clearTimeout(this._totalMemoryTimeout);
          this._totalMemoryTimeout = undefined;
          this.handleMemoryReport(
            "MEMORY_TOTAL",
            Ci.nsIMemoryReporter.UNITS_BYTES,
            this._totalMemory);

          let length = this._USSFromChildProcesses.length;
          if (length > 1) {
            // Mean of the USS of all the content processes.
            let mean = this._USSFromChildProcesses.reduce((a, b) => a + b, 0) / length;
            // Absolute error of USS for each content process, normalized by the mean (*100 to get it in percentage).
            // 20% means for a content process that it is using 20% more or 20% less than the mean.
            let diffs = this._USSFromChildProcesses.map(value => Math.floor(Math.abs(value - mean) * 100 / mean));
            let tabsCount = this.getOpenTabsCount();
            let key;
            if (tabsCount < 11) {
              key = "0 - 10 tabs";
            } else if (tabsCount < 501) {
              key = "11 - 500 tabs";
            } else {
              key = "more tabs";
            }

            diffs.forEach(value => {
              this.handleMemoryReport(
              "MEMORY_DISTRIBUTION_AMONG_CONTENT",
              Ci.nsIMemoryReporter.UNITS_COUNT,
              value,
              key);
            });

            // This notification is for testing only.
            Services.obs.notifyObservers(null, "gather-memory-telemetry-finished");
          }
          this._USSFromChildProcesses = undefined;
        }
      } else {
        this._log.trace("Child USS report was missed");
      }
      break;
    }
    case MESSAGE_TELEMETRY_GET_CHILD_USS:
    {
      // In child process, send the requested USS report
      this.sendContentProcessUSS(message.data.id);
      break;
    }
    default:
      throw new Error("Telemetry.receiveMessage: bad message name");
    }
  },

  sendContentProcessUSS: function sendContentProcessUSS(aMessageId) {
    this._log.trace("sendContentProcessUSS");

    let mgr;
    try {
      mgr = Cc["@mozilla.org/memory-reporter-manager;1"].
            getService(Ci.nsIMemoryReporterManager);
    } catch (e) {
      // OK to skip memory reporters in xpcshell
      return;
    }

    Services.cpmm.sendAsyncMessage(
      MESSAGE_TELEMETRY_USS,
      {bytes: mgr.residentUnique, id: aMessageId}
    );
  },

  /**
   * Do some shutdown work that is common to all process types.
   */
  uninstall() {
    for (let topic of this._observedTopics) {
      try {
        // Tests may flip Telemetry.canRecordExtended on and off. It can be the case
        // that the observer TOPIC_CYCLE_COLLECTOR_BEGIN was not added.
        this.removeObserver(topic);
      } catch (e) {
        this._log.warn("uninstall - Failed to remove " + topic, e);
      }
    }

    GCTelemetry.shutdown();
  },

  observe(aSubject, aTopic, aData) {
    // Prevent the cycle collector begin topic from cluttering the log.
    if (aTopic != TOPIC_CYCLE_COLLECTOR_BEGIN) {
      this._log.trace("observe - " + aTopic + " notified.");
    }

    switch (aTopic) {
    case "content-child-shutdown":
      // content-child-shutdown is only registered for content processes.
      this.uninstall();
      Telemetry.flushBatchedChildTelemetry();
      break;
    case TOPIC_CYCLE_COLLECTOR_BEGIN:
      let now = new Date();
      if (!gLastMemoryPoll
          || (TELEMETRY_INTERVAL <= now - gLastMemoryPoll)) {
        gLastMemoryPoll = now;

        this._log.trace("Dispatching idle gatherMemory task");
        Services.tm.idleDispatchToMainThread(() => {
          this._log.trace("Running idle gatherMemory task");
          this.gatherMemory();
          return true;
        });
      }
      break;
    }
    return undefined;
  },

  shutdown() {
    this._log.trace("shutdown");

    let cleanup = () => {
      this.uninstall();

      this._initStarted = false;
      this._initialized = false;
    };

    // We can be in one the following states here:
    // 1) delayedInit was never called
    // or it was called and
    //   2) _delayedInitTask is running now.
    //   3) _delayedInitTask finished running already.

    // This handles 1).
    if (!this._initStarted) {
      return Promise.resolve();
    }

    // This handles 3).
    if (!this._delayedInitTask) {
      // We already ran the delayed initialization.
      return cleanup();
    }

    // This handles 2).
    return this._delayedInitTask.then(cleanup);
  },
};
