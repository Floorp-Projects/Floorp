/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Log} = ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  TelemetryController: "resource://gre/modules/TelemetryController.jsm",
  TelemetryStorage: "resource://gre/modules/TelemetryStorage.jsm",
  UITelemetry: "resource://gre/modules/UITelemetry.jsm",
  GCTelemetry: "resource://gre/modules/GCTelemetry.jsm",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.jsm",
  TelemetryReportingPolicy: "resource://gre/modules/TelemetryReportingPolicy.jsm",
  TelemetryScheduler: "resource://gre/modules/TelemetryScheduler.jsm",
});

const Utils = TelemetryUtils;

const myScope = this;

// When modifying the payload in incompatible ways, please bump this version number
const PAYLOAD_VERSION = 4;
const PING_TYPE_MAIN = "main";
const PING_TYPE_SAVED_SESSION = "saved-session";

const REASON_ABORTED_SESSION = "aborted-session";
const REASON_DAILY = "daily";
const REASON_SAVED_SESSION = "saved-session";
const REASON_GATHER_PAYLOAD = "gather-payload";
const REASON_GATHER_SUBSESSION_PAYLOAD = "gather-subsession-payload";
const REASON_TEST_PING = "test-ping";
const REASON_ENVIRONMENT_CHANGE = "environment-change";
const REASON_SHUTDOWN = "shutdown";

const ENVIRONMENT_CHANGE_LISTENER = "TelemetrySession::onEnvironmentChange";

const MIN_SUBSESSION_LENGTH_MS = Services.prefs.getIntPref("toolkit.telemetry.minSubsessionLength", 5 * 60) * 1000;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetrySession" + (Utils.isContentProcess ? "#content::" : "::");

// Whether the FHR/Telemetry unification features are enabled.
// Changing this pref requires a restart.
const IS_UNIFIED_TELEMETRY = Services.prefs.getBoolPref(TelemetryUtils.Preferences.Unified, false);

var gWasDebuggerAttached = false;

XPCOMUtils.defineLazyServiceGetters(this, {
  Telemetry: ["@mozilla.org/base/telemetry;1", "nsITelemetry"],
});

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

/**
 * This is a policy object used to override behavior for testing.
 */
var Policy = {
  now: () => new Date(),
  monotonicNow: Utils.monotonicNow,
  generateSessionUUID: () => generateUUID(),
  generateSubsessionUUID: () => generateUUID(),
};

/**
 * Get the ping type based on the payload.
 * @param {Object} aPayload The ping payload.
 * @return {String} A string representing the ping type.
 */
function getPingType(aPayload) {
  // To remain consistent with server-side ping handling, set "saved-session" as the ping
  // type for "saved-session" payload reasons.
  if (aPayload.info.reason == REASON_SAVED_SESSION) {
    return PING_TYPE_SAVED_SESSION;
  }

  return PING_TYPE_MAIN;
}

/**
 * Annotate the current session ID with the crash reporter to map potential
 * crash pings with the related main ping.
 */
function annotateCrashReport(sessionId) {
  try {
    const cr = Cc["@mozilla.org/toolkit/crash-reporter;1"];
    if (cr) {
      cr.getService(Ci.nsICrashReporter).setTelemetrySessionId(sessionId);
    }
  } catch (e) {
    // Ignore errors when crash reporting is disabled
  }
}

/**
 * Read current process I/O counters.
 */
var processInfo = {
  _initialized: false,
  _IO_COUNTERS: null,
  _kernel32: null,
  _GetProcessIoCounters: null,
  _GetCurrentProcess: null,
  getCounters() {
    let isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);
    if (isWindows)
      return this.getCounters_Windows();
    return null;
  },
  getCounters_Windows() {
    if (!this._initialized) {
      var {ctypes} = ChromeUtils.import("resource://gre/modules/ctypes.jsm");
      this._IO_COUNTERS = new ctypes.StructType("IO_COUNTERS", [
        {"readOps": ctypes.unsigned_long_long},
        {"writeOps": ctypes.unsigned_long_long},
        {"otherOps": ctypes.unsigned_long_long},
        {"readBytes": ctypes.unsigned_long_long},
        {"writeBytes": ctypes.unsigned_long_long},
        {"otherBytes": ctypes.unsigned_long_long} ]);
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
    if (!this._GetProcessIoCounters(this._GetCurrentProcess(), io.address()))
      return null;
    return [parseInt(io.readBytes), parseInt(io.writeBytes)];
  },
};

var EXPORTED_SYMBOLS = ["TelemetrySession"];

var TelemetrySession = Object.freeze({
  /**
   * Send a ping to a test server. Used only for testing.
   */
  testPing() {
    return Impl.testPing();
  },
  /**
   * Returns the current telemetry payload.
   * @param reason Optional, the reason to trigger the payload.
   * @param clearSubsession Optional, whether to clear subsession specific data.
   * @returns Object
   */
  getPayload(reason, clearSubsession = false) {
    return Impl.getPayload(reason, clearSubsession);
  },
  /**
   * Save the session state to a pending file.
   * Used only for testing purposes.
   */
  testSavePendingPing() {
    return Impl.testSavePendingPing();
  },
  /**
   * Collect and store information about startup.
   */
  gatherStartup() {
    return Impl.gatherStartup();
  },
  /**
   * Inform the ping which AddOns are installed.
   *
   * @param aAddOns - The AddOns.
   */
  setAddOns(aAddOns) {
    return Impl.setAddOns(aAddOns);
  },
  /**
   * Descriptive metadata
   *
   * @param  reason
   *         The reason for the telemetry ping, this will be included in the
   *         returned metadata,
   * @return The metadata as a JS object
   */
  getMetadata(reason) {
    return Impl.getMetadata(reason);
  },

  /**
   * Reset the subsession and profile subsession counter.
   * This should only be called when the profile should be considered completely new,
   * e.g. after opting out of sending Telemetry
   */
  resetSubsessionCounter() {
    Impl._subsessionCounter = 0;
    Impl._profileSubsessionCounter = 0;
  },

  /**
   * Used only for testing purposes.
   */
  testReset() {
    Impl._newProfilePingSent = false;
    Impl._sessionId = null;
    Impl._subsessionId = null;
    Impl._previousSessionId = null;
    Impl._previousSubsessionId = null;
    Impl._subsessionCounter = 0;
    Impl._profileSubsessionCounter = 0;
    Impl._subsessionStartActiveTicks = 0;
    Impl._sessionActiveTicks = 0;
    Impl._isUserActive = true;
    Impl._subsessionStartTimeMonotonic = 0;
    Impl._lastEnvironmentChangeDate = Policy.monotonicNow();
    this.testUninstall();
  },
  /**
   * Triggers shutdown of the module.
   */
  shutdown() {
    return Impl.shutdownChromeProcess();
  },
  /**
   * Used only for testing purposes.
   */
  testUninstall() {
    try {
      Impl.uninstall();
    } catch (ex) {
      // Ignore errors
    }
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
  /**
   * Send a notification.
   */
  observe(aSubject, aTopic, aData) {
    return Impl.observe(aSubject, aTopic, aData);
  },
  /**
   * Marks the "new-profile" ping as sent in the telemetry state file.
   * @return {Promise} A promise resolved when the new telemetry state is saved to disk.
   */
  markNewProfilePingSent() {
    return Impl.markNewProfilePingSent();
  },
  /**
   * Returns if the "new-profile" ping has ever been sent for this profile.
   * Please note that the returned value is trustworthy only after the delayed setup.
   *
   * @return {Boolean} True if the new profile ping was sent on this profile,
   *         false otherwise.
   */
  get newProfilePingSent() {
    return Impl._newProfilePingSent;
  },


  saveAbortedSessionPing(aProvidedPayload) {
    return Impl._saveAbortedSessionPing(aProvidedPayload);
  },

  sendDailyPing() {
    return Impl._sendDailyPing();
  },
});

var Impl = {
  _initialized: false,
  _logger: null,
  _slowSQLStartup: {},
  // The activity state for the user. If false, don't count the next
  // active tick. Otherwise, increment the active ticks as usual.
  _isUserActive: true,
  _startupIO: {},
  // The previous build ID, if this is the first run with a new build.
  // Null if this is the first run, or the previous build ID is unknown.
  _previousBuildId: null,
  // Unique id that identifies this session so the server can cope with duplicate
  // submissions, orphaning and other oddities. The id is shared across subsessions.
  _sessionId: null,
  // Random subsession id.
  _subsessionId: null,
  // Session id of the previous session, null on first run.
  _previousSessionId: null,
  // Subsession id of the previous subsession (even if it was in a different session),
  // null on first run.
  _previousSubsessionId: null,
  // The running no. of subsessions since the start of the browser session
  _subsessionCounter: 0,
  // The running no. of all subsessions for the whole profile life time
  _profileSubsessionCounter: 0,
  // Date of the last session split
  _subsessionStartDate: null,
  // Start time of the current subsession using a monotonic clock for the subsession
  // length measurements.
  _subsessionStartTimeMonotonic: 0,
  // The active ticks counted when the subsession starts
  _subsessionStartActiveTicks: 0,
  // Active ticks in the whole session.
  _sessionActiveTicks: 0,
  // A task performing delayed initialization of the chrome process
  _delayedInitTask: null,
  _testing: false,
  // An accumulator of total memory across all processes. Only valid once the final child reports.
  _lastEnvironmentChangeDate: 0,
  // We save whether the "new-profile" ping was sent yet, to
  // survive profile refresh and migrations.
  _newProfilePingSent: false,
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
   * Gets a series of simple measurements (counters). At the moment, this
   * only returns startup data from nsIAppStartup.getStartupInfo().
   * @param {Boolean} isSubsession True if this is a subsession, false otherwise.
   * @param {Boolean} clearSubsession True if a new subsession is being started, false otherwise.
   *
   * @return simple measurements as a dictionary.
   */
  getSimpleMeasurements: function getSimpleMeasurements(forSavedSession, isSubsession, clearSubsession) {
    let si = Services.startup.getStartupInfo();

    // Measurements common to chrome and content processes.
    let elapsedTime = Date.now() - si.process;
    var ret = {
      totalTime: Math.round(elapsedTime / 1000), // totalTime, in seconds
    };

    // Look for app-specific timestamps
    var appTimestamps = {};
    try {
      let o = {};
      ChromeUtils.import("resource://gre/modules/TelemetryTimestamps.jsm", o);
      appTimestamps = o.TelemetryTimestamps.get();
    } catch (ex) {}

    // Only submit this if the extended set is enabled.
    if (!Utils.isContentProcess && Telemetry.canRecordExtended) {
      try {
        ret.addonManager = AddonManagerPrivate.getSimpleMeasures();
      } catch (ex) {}
    }

    if (si.process) {
      for (let field of Object.keys(si)) {
        if (field == "process")
          continue;
        ret[field] = si[field] - si.process;
      }

      for (let p in appTimestamps) {
        if (!(p in ret) && appTimestamps[p])
          ret[p] = appTimestamps[p] - si.process;
      }
    }

    ret.startupInterrupted = Number(Services.startup.interrupted);

    let maximalNumberOfConcurrentThreads = Telemetry.maximalNumberOfConcurrentThreads;
    if (maximalNumberOfConcurrentThreads) {
      ret.maximalNumberOfConcurrentThreads = maximalNumberOfConcurrentThreads;
    }

    if (Utils.isContentProcess) {
      return ret;
    }

    // Measurements specific to chrome process

    // Update debuggerAttached flag
    let debugService = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
    let isDebuggerAttached = debugService.isDebuggerAttached;
    gWasDebuggerAttached = gWasDebuggerAttached || isDebuggerAttached;
    ret.debuggerAttached = Number(gWasDebuggerAttached);

    let shutdownDuration = Telemetry.lastShutdownDuration;
    if (shutdownDuration)
      ret.shutdownDuration = shutdownDuration;

    let failedProfileLockCount = Telemetry.failedProfileLockCount;
    if (failedProfileLockCount)
      ret.failedProfileLockCount = failedProfileLockCount;

    for (let ioCounter in this._startupIO)
      ret[ioCounter] = this._startupIO[ioCounter];

    let activeTicks = this._sessionActiveTicks;
    if (isSubsession) {
      activeTicks = this._sessionActiveTicks - this._subsessionStartActiveTicks;
    }

    if (clearSubsession) {
      this._subsessionStartActiveTicks = this._sessionActiveTicks;
    }

    ret.activeTicks = activeTicks;

    return ret;
  },

  getHistograms: function getHistograms(clearSubsession) {
    return Telemetry.getSnapshotForHistograms("main", clearSubsession, !this._testing);
  },

  getKeyedHistograms(clearSubsession) {
    return Telemetry.getSnapshotForKeyedHistograms("main", clearSubsession, !this._testing);
  },

  /**
   * Get a snapshot of the scalars and clear them.
   * @param {subsession} If true, then we collect the data for a subsession.
   * @param {clearSubsession} If true, we  need to clear the subsession.
   * @param {keyed} Take a snapshot of keyed or non keyed scalars.
   * @return {Object} The scalar data as a Javascript object, including the
   *         data from child processes, in the following format:
   *            {'content': { 'scalarName': ... }, 'gpu': { ... } }
   */
  getScalars(subsession, clearSubsession, keyed) {
    if (!subsession) {
      // We only support scalars for subsessions.
      this._log.trace("getScalars - We only support scalars in subsessions.");
      return {};
    }

    let scalarsSnapshot = keyed ?
      Telemetry.getSnapshotForKeyedScalars("main", clearSubsession, !this._testing) :
      Telemetry.getSnapshotForScalars("main", clearSubsession, !this._testing);

    return scalarsSnapshot;
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
    const sessionStartDate = Utils.toLocalTimeISOString(Utils.truncateToHours(this._sessionStartDate));
    const subsessionStartDate = Utils.toLocalTimeISOString(Utils.truncateToHours(this._subsessionStartDate));
    const monotonicNow = Policy.monotonicNow();

    let ret = {
      reason,
      revision: AppConstants.SOURCE_REVISION_URL,

      // Date.getTimezoneOffset() unintuitively returns negative values if we are ahead of
      // UTC and vice versa (e.g. -60 for UTC+1). We invert the sign here.
      timezoneOffset: -this._subsessionStartDate.getTimezoneOffset(),
      previousBuildId: this._previousBuildId,

      sessionId: this._sessionId,
      subsessionId: this._subsessionId,
      previousSessionId: this._previousSessionId,
      previousSubsessionId: this._previousSubsessionId,

      subsessionCounter: this._subsessionCounter,
      profileSubsessionCounter: this._profileSubsessionCounter,

      sessionStartDate,
      subsessionStartDate,

      // Compute the session and subsession length in seconds.
      // We use monotonic clocks as Date() is affected by jumping clocks (leading
      // to negative lengths and other issues).
      sessionLength: Math.floor(monotonicNow / 1000),
      subsessionLength:
        Math.floor((monotonicNow - this._subsessionStartTimeMonotonic) / 1000),
    };

    // TODO: Remove this when bug 1201837 lands.
    if (this._addons)
      ret.addons = this._addons;

    // TODO: Remove this when bug 1201837 lands.
    let flashVersion = this.getFlashVersion();
    if (flashVersion)
      ret.flashVersion = flashVersion;

    return ret;
  },

  /**
   * Get the current session's payload using the provided
   * simpleMeasurements and info, which are typically obtained by a call
   * to |this.getSimpleMeasurements| and |this.getMetadata|,
   * respectively.
   */
  assemblePayloadWithMeasurements(simpleMeasurements, info, reason, clearSubsession) {
    const isSubsession = IS_UNIFIED_TELEMETRY && !this._isClassicReason(reason);
    clearSubsession = IS_UNIFIED_TELEMETRY && clearSubsession;
    this._log.trace("assemblePayloadWithMeasurements - reason: " + reason +
                    ", submitting subsession data: " + isSubsession);

    // This allows wrapping data retrieval calls in a try-catch block so that
    // failures don't break the rest of the ping assembly.
    const protect = (fn, defaultReturn = null) => {
      try {
        return fn();
      } catch (ex) {
        this._log.error("assemblePayloadWithMeasurements - caught exception", ex);
        return defaultReturn;
      }
    };

    // Payload common to chrome and content processes.
    let payloadObj = {
      ver: PAYLOAD_VERSION,
      simpleMeasurements,
    };

    // Add extended set measurements common to chrome & content processes
    if (Telemetry.canRecordExtended) {
      payloadObj.log = [];
      payloadObj.webrtc = protect(() => Telemetry.webrtcStats);
    }

    if (Utils.isContentProcess) {
      return payloadObj;
    }

    // Additional payload for chrome process.
    let measurements = {
      histograms: protect(() => this.getHistograms(clearSubsession), {}),
      keyedHistograms: protect(() => this.getKeyedHistograms(clearSubsession), {}),
      scalars: protect(() => this.getScalars(isSubsession, clearSubsession), {}),
      keyedScalars: protect(() => this.getScalars(isSubsession, clearSubsession, true), {}),
    };

    let measurementsContainGPU = Object
      .keys(measurements)
      .some(key => "gpu" in measurements[key]);

    let measurementsContainSocket = Object
      .keys(measurements)
      .some(key => "socket" in measurements[key]);

    payloadObj.processes = {};
    let processTypes = ["parent", "content", "extension", "dynamic"];
    // Only include the GPU process if we've accumulated data for it.
    if (measurementsContainGPU) {
      processTypes.push("gpu");
    }
    if (measurementsContainSocket) {
      processTypes.push("socket");
    }

    // Collect per-process measurements.
    for (const processType of processTypes) {
      let processPayload = {};

      for (const key in measurements) {
        let payloadLoc = processPayload;
        // Parent histograms are added to the top-level payload object instead of the process payload.
        if (processType == "parent" && (key == "histograms" || key == "keyedHistograms")) {
          payloadLoc = payloadObj;
        }
        // The Dynamic process only collects scalars.
        if (processType == "dynamic" && key !== "scalars") {
          continue;
        }

        // Process measurements can be empty, set a default value.
        payloadLoc[key] = measurements[key][processType] || {};
      }

      // Add process measurements to payload.
      payloadObj.processes[processType] = processPayload;
    }

    payloadObj.info = info;

    // Add extended set measurements for chrome process.
    if (Telemetry.canRecordExtended) {
      payloadObj.slowSQL = protect(() => Telemetry.slowSQL);
      payloadObj.fileIOReports = protect(() => Telemetry.fileIOReports);
      payloadObj.lateWrites = protect(() => Telemetry.lateWrites);

      payloadObj.addonDetails = protect(() => AddonManagerPrivate.getTelemetryDetails());

      let clearUIsession = !(reason == REASON_GATHER_PAYLOAD || reason == REASON_GATHER_SUBSESSION_PAYLOAD);

      if (AppConstants.platform == "android") {
        payloadObj.UIMeasurements = protect(() => UITelemetry.getUIMeasurements(clearUIsession));
      }

      if (this._slowSQLStartup &&
          Object.keys(this._slowSQLStartup).length != 0 &&
          (Object.keys(this._slowSQLStartup.mainThread).length ||
           Object.keys(this._slowSQLStartup.otherThreads).length)) {
        payloadObj.slowSQLStartup = this._slowSQLStartup;
      }

      if (!this._isClassicReason(reason)) {
        payloadObj.processes.parent.gc = protect(() => GCTelemetry.entries("main", clearSubsession));
        payloadObj.processes.content.gc = protect(() => GCTelemetry.entries("content", clearSubsession));
      }

      // Adding captured stacks to the payload only if any exist and clearing
      // captures for this sub-session.
      let stacks = protect(() => Telemetry.snapshotCapturedStacks(true));
      if (stacks && ("captures" in stacks) && (stacks.captures.length > 0)) {
        payloadObj.processes.parent.capturedStacks = stacks;
      }
    }

    return payloadObj;
  },

  /**
   * Start a new subsession.
   */
  startNewSubsession() {
    this._subsessionStartDate = Policy.now();
    this._subsessionStartTimeMonotonic = Policy.monotonicNow();
    this._previousSubsessionId = this._subsessionId;
    this._subsessionId = Policy.generateSubsessionUUID();
    this._subsessionCounter++;
    this._profileSubsessionCounter++;
  },

  getSessionPayload: function getSessionPayload(reason, clearSubsession) {
    this._log.trace("getSessionPayload - reason: " + reason + ", clearSubsession: " + clearSubsession);

    let payload;
    try {
      const isMobile = (AppConstants.platform == "android");
      const isSubsession = isMobile ? false : !this._isClassicReason(reason);

      if (isMobile) {
        clearSubsession = false;
      }

      let measurements =
        this.getSimpleMeasurements(reason == REASON_SAVED_SESSION, isSubsession, clearSubsession);
      let info = !Utils.isContentProcess ? this.getMetadata(reason) : null;
      payload = this.assemblePayloadWithMeasurements(measurements, info, reason, clearSubsession);
    } catch (ex) {
      Telemetry.getHistogramById("TELEMETRY_ASSEMBLE_PAYLOAD_EXCEPTION").add(1);
      throw ex;
    } finally {
      if (!Utils.isContentProcess && clearSubsession) {
        this.startNewSubsession();
        // Persist session data to disk (don't wait until it completes).
        let sessionData = this._getSessionDataObject();
        TelemetryStorage.saveSessionData(sessionData);

        // Notify that there was a subsession split in the parent process. This is an
        // internal topic and is only meant for internal Telemetry usage.
        Services.obs.notifyObservers(null, "internal-telemetry-after-subsession-split");
      }
    }

    return payload;
  },

  /**
   * Send data to the server. Record success/send-time in histograms
   */
  send: async function send(reason) {
    this._log.trace("send - Reason " + reason);
    // populate histograms one last time
    await Services.telemetry.gatherMemory();

    const isSubsession = !this._isClassicReason(reason);
    let payload = this.getSessionPayload(reason, isSubsession);
    let options = {
      addClientId: true,
      addEnvironment: true,
    };
    return TelemetryController.submitExternalPing(getPingType(payload), payload, options);
  },

  /**
   * Attaches the needed observers during Telemetry early init, in the
   * chrome process.
   */
  attachEarlyObservers() {
    this.addObserver("sessionstore-windows-restored");
    if (AppConstants.platform === "android") {
      this.addObserver("application-background");
    }
    this.addObserver("xul-window-visible");

    // Attach the active-ticks related observers.
    this.addObserver("user-interaction-active");
    this.addObserver("user-interaction-inactive");
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

    // Generate a unique id once per session so the server can cope with duplicate
    // submissions, orphaning and other oddities. The id is shared across subsessions.
    this._sessionId = Policy.generateSessionUUID();
    this.startNewSubsession();
    // startNewSubsession sets |_subsessionStartDate| to the current date/time. Use
    // the very same value for |_sessionStartDate|.
    this._sessionStartDate = this._subsessionStartDate;

    annotateCrashReport(this._sessionId);

    // Record old value and update build ID preference if this is the first
    // run with a new build ID.
    let previousBuildId = Services.prefs.getStringPref(TelemetryUtils.Preferences.PreviousBuildID, null);
    let thisBuildID = Services.appinfo.appBuildID;
    // If there is no previousBuildId preference, we send null to the server.
    if (previousBuildId != thisBuildID) {
      this._previousBuildId = previousBuildId;
      Services.prefs.setStringPref(TelemetryUtils.Preferences.PreviousBuildID, thisBuildID);
    }

    this.attachEarlyObservers();
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

        await this._loadSessionData();
        // Update the session data to keep track of new subsessions created before
        // the initialization.
        await TelemetryStorage.saveSessionData(this._getSessionDataObject());

        this.addObserver("idle-daily");
        await Services.telemetry.gatherMemory();

        Telemetry.asyncFetchTelemetryData(function() {});

        if (IS_UNIFIED_TELEMETRY) {
          // Check for a previously written aborted session ping.
          await TelemetryController.checkAbortedSessionPing();

          // Write the first aborted-session ping as early as possible. Just do that
          // if we are not testing, since calling Telemetry.reset() will make a previous
          // aborted ping a pending ping.
          if (!this._testing) {
            await this._saveAbortedSessionPing();
          }

          // The last change date for the environment, used to throttle environment changes.
          this._lastEnvironmentChangeDate = Policy.monotonicNow();
          TelemetryEnvironment.registerChangeListener(ENVIRONMENT_CHANGE_LISTENER,
                                 (reason, data) => this._onEnvironmentChange(reason, data));

          // Start the scheduler.
          // We skip this if unified telemetry is off, so we don't
          // trigger the new unified ping types.
          TelemetryScheduler.init();
        }

        this._delayedInitTask = null;
      } catch (e) {
        this._delayedInitTask = null;
        throw e;
      }
    })();

    return this._delayedInitTask;
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

  /**
   * On Desktop: Save the "shutdown" ping to disk.
   * On Android: Save the "saved-session" ping to disk.
   * This needs to be called after TelemetrySend shuts down otherwise pings
   * would be sent instead of getting persisted to disk.
   */
  saveShutdownPings() {
    this._log.trace("saveShutdownPings");

    // We append the promises to this list and wait
    // on all pings to be saved after kicking off their collection.
    let p = [];

    if (IS_UNIFIED_TELEMETRY) {
      let shutdownPayload = this.getSessionPayload(REASON_SHUTDOWN, false);

      // Only send the shutdown ping using the pingsender from the second
      // browsing session on, to mitigate issues with "bot" profiles (see bug 1354482).
      const sendOnThisSession =
        Services.prefs.getBoolPref(Utils.Preferences.ShutdownPingSenderFirstSession, false) ||
        !TelemetryReportingPolicy.isFirstRun();
      let sendWithPingsender = Services.prefs.getBoolPref(TelemetryUtils.Preferences.ShutdownPingSender, false) &&
                               sendOnThisSession;

      let options = {
        addClientId: true,
        addEnvironment: true,
        usePingSender: sendWithPingsender,
      };
      p.push(TelemetryController.submitExternalPing(getPingType(shutdownPayload), shutdownPayload, options)
                                .catch(e => this._log.error("saveShutdownPings - failed to submit shutdown ping", e)));

      // Send a duplicate of first-shutdown pings as a new ping type, in order to properly
      // evaluate first session profiles (see bug 1390095).
      const sendFirstShutdownPing =
        Services.prefs.getBoolPref(Utils.Preferences.ShutdownPingSender, false) &&
        Services.prefs.getBoolPref(Utils.Preferences.FirstShutdownPingEnabled, false) &&
        TelemetryReportingPolicy.isFirstRun();

      if (sendFirstShutdownPing) {
        let options = {
            addClientId: true,
            addEnvironment: true,
            usePingSender: true,
          };
        p.push(TelemetryController.submitExternalPing("first-shutdown", shutdownPayload, options)
                                  .catch(e => this._log.error("saveShutdownPings - failed to submit first shutdown ping", e)));
      }
    }

    if (AppConstants.platform == "android" && Telemetry.canRecordExtended) {
      let payload = this.getSessionPayload(REASON_SAVED_SESSION, false);

      let options = {
        addClientId: true,
        addEnvironment: true,
      };
      p.push(TelemetryController.submitExternalPing(getPingType(payload), payload, options)
                                .catch(e => this._log.error("saveShutdownPings - failed to submit saved-session ping", e)));
    }

    // Wait on pings to be saved.
    return Promise.all(p);
  },

  testSavePendingPing() {
    let payload = this.getSessionPayload(REASON_SAVED_SESSION, false);
    let options = {
      addClientId: true,
      addEnvironment: true,
      overwrite: true,
    };
    return TelemetryController.addPendingPing(getPingType(payload), payload, options);
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
  },

  getPayload: function getPayload(reason, clearSubsession) {
    this._log.trace("getPayload - clearSubsession: " + clearSubsession);
    reason = reason || REASON_GATHER_PAYLOAD;
    // This function returns the current Telemetry payload to the caller.
    // We only gather startup info once.
    if (Object.keys(this._slowSQLStartup).length == 0) {
      this._slowSQLStartup = Telemetry.slowSQL;
    }
    Services.telemetry.gatherMemory();
    return this.getSessionPayload(reason, clearSubsession);
  },

  gatherStartup: function gatherStartup() {
    this._log.trace("gatherStartup");
    let counters = processInfo.getCounters();
    if (counters) {
      [this._startupIO.startupSessionRestoreReadBytes,
        this._startupIO.startupSessionRestoreWriteBytes] = counters;
    }
    this._slowSQLStartup = Telemetry.slowSQL;
  },

  setAddOns: function setAddOns(aAddOns) {
    this._addons = aAddOns;
  },

  testPing: function testPing() {
    return this.send(REASON_TEST_PING);
  },

  /**
   * Tracks the number of "ticks" the user was active in.
   */
  _onActiveTick(aUserActive) {
    const needsUpdate = aUserActive && this._isUserActive;
    this._isUserActive = aUserActive;

    // Don't count the first active tick after we get out of
    // inactivity, because it is just the start of this active tick.
    if (needsUpdate) {
      this._sessionActiveTicks++;
      Telemetry.scalarAdd("browser.engagement.active_ticks", 1);
    }
  },

  /**
   * This observer drives telemetry.
   */
  observe(aSubject, aTopic, aData) {
    this._log.trace("observe - " + aTopic + " notified.");

    switch (aTopic) {
    case "xul-window-visible":
      this.removeObserver("xul-window-visible");
      var counters = processInfo.getCounters();
      if (counters) {
        [this._startupIO.startupWindowVisibleReadBytes,
          this._startupIO.startupWindowVisibleWriteBytes] = counters;
      }
      break;
    case "sessionstore-windows-restored":
      this.removeObserver("sessionstore-windows-restored");
      // Check whether debugger was attached during startup
      let debugService = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
      gWasDebuggerAttached = debugService.isDebuggerAttached;
      this.gatherStartup();
      break;
    case "idle-daily":
      // Enqueue to main-thread, otherwise components may be inited by the
      // idle-daily category and miss the gather-telemetry notification.
      Services.tm.dispatchToMainThread((function() {
        // Notify that data should be gathered now.
        // TODO: We are keeping this behaviour for now but it will be removed as soon as
        // bug 1127907 lands.
        Services.obs.notifyObservers(null, "gather-telemetry");
      }));
      break;

    case "application-background":
      if (AppConstants.platform !== "android") {
        break;
      }
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
      let payload = this.getSessionPayload(REASON_SAVED_SESSION, false);
      let options = {
        addClientId: true,
        addEnvironment: true,
        overwrite: true,
      };
      TelemetryController.addPendingPing(getPingType(payload), payload, options);
      break;
    case "user-interaction-active":
      this._onActiveTick(true);
      break;
    case "user-interaction-inactive":
      this._onActiveTick(false);
      break;
    }
    return undefined;
  },

  /**
   * This tells TelemetrySession to uninitialize and save any pending pings.
   */
  shutdownChromeProcess() {
    this._log.trace("shutdownChromeProcess");

    let cleanup = () => {
      if (IS_UNIFIED_TELEMETRY) {
        TelemetryEnvironment.unregisterChangeListener(ENVIRONMENT_CHANGE_LISTENER);
        TelemetryScheduler.shutdown();
      }
      this.uninstall();

      let reset = () => {
        this._initStarted = false;
        this._initialized = false;
      };

      return (async () => {
        await this.saveShutdownPings();

        if (IS_UNIFIED_TELEMETRY) {
          await TelemetryController.removeAbortedSessionPing();
        }

        reset();
      })();
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

  /**
   * Gather and send a daily ping.
   * @return {Promise} Resolved when the ping is sent.
   */
  _sendDailyPing() {
    this._log.trace("_sendDailyPing");
    let payload = this.getSessionPayload(REASON_DAILY, true);

    let options = {
      addClientId: true,
      addEnvironment: true,
    };

    let promise = TelemetryController.submitExternalPing(getPingType(payload), payload, options);

    // Also save the payload as an aborted session. If we delay this, aborted-session can
    // lag behind for the profileSubsessionCounter and other state, complicating analysis.
    if (IS_UNIFIED_TELEMETRY) {
      this._saveAbortedSessionPing(payload)
          .catch(e => this._log.error("_sendDailyPing - Failed to save the aborted session ping", e));
    }

    return promise;
  },

  /** Loads session data from the session data file.
   * @return {Promise<object>} A promise which is resolved with an object when
   *                            loading has completed, with null otherwise.
   */
  async _loadSessionData() {
    let data = await TelemetryStorage.loadSessionData();

    if (!data) {
      return null;
    }

    if (!("profileSubsessionCounter" in data) ||
        !(typeof(data.profileSubsessionCounter) == "number") ||
        !("subsessionId" in data) || !("sessionId" in data)) {
      this._log.error("_loadSessionData - session data is invalid");
      Telemetry.getHistogramById("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").add(1);
      return null;
    }

    this._previousSessionId = data.sessionId;
    this._previousSubsessionId = data.subsessionId;
    // Add |_subsessionCounter| to the |_profileSubsessionCounter| to account for
    // new subsession while loading still takes place. This will always be exactly
    // 1 - the current subsessions.
    this._profileSubsessionCounter = data.profileSubsessionCounter +
                                     this._subsessionCounter;
    // If we don't have this flag in the state file, it means that this is an old profile.
    // We don't want to send the "new-profile" ping on new profile, so se this to true.
    this._newProfilePingSent = ("newProfilePingSent" in data) ? data.newProfilePingSent : true;
    return data;
  },

  /**
   * Get the session data object to serialise to disk.
   */
  _getSessionDataObject() {
    return {
      sessionId: this._sessionId,
      subsessionId: this._subsessionId,
      profileSubsessionCounter: this._profileSubsessionCounter,
      newProfilePingSent: this._newProfilePingSent,
    };
  },

  _onEnvironmentChange(reason, oldEnvironment) {
    this._log.trace("_onEnvironmentChange", reason);

    let now = Policy.monotonicNow();
    let timeDelta = now - this._lastEnvironmentChangeDate;
    if (timeDelta <= MIN_SUBSESSION_LENGTH_MS) {
      this._log.trace(`_onEnvironmentChange - throttling; last change was ${Math.round(timeDelta / 1000)}s ago.`);
      return;
    }

    this._lastEnvironmentChangeDate = now;
    let payload = this.getSessionPayload(REASON_ENVIRONMENT_CHANGE, true);
    TelemetryScheduler.rescheduleDailyPing(payload);

    let options = {
      addClientId: true,
      addEnvironment: true,
      overrideEnvironment: oldEnvironment,
    };
    TelemetryController.submitExternalPing(getPingType(payload), payload, options);
  },

  _isClassicReason(reason) {
    const classicReasons = [
      REASON_SAVED_SESSION,
      REASON_GATHER_PAYLOAD,
      REASON_TEST_PING,
    ];
    return classicReasons.includes(reason);
  },

  /**
   * Get an object describing the current state of this module for AsyncShutdown diagnostics.
   */
  _getState() {
    return {
      initialized: this._initialized,
      initStarted: this._initStarted,
      haveDelayedInitTask: !!this._delayedInitTask,
    };
  },

  /**
   * Saves the aborted session ping to disk.
   * @param {Object} [aProvidedPayload=null] A payload object to be used as an aborted
   *                 session ping. The reason of this payload is changed to aborted-session.
   *                 If not provided, a new payload is gathered.
   */
  _saveAbortedSessionPing(aProvidedPayload = null) {
    this._log.trace("_saveAbortedSessionPing");

    let payload = null;
    if (aProvidedPayload) {
      payload = Cu.cloneInto(aProvidedPayload, myScope);
      // Overwrite the original reason.
      payload.info.reason = REASON_ABORTED_SESSION;
    } else {
      payload = this.getSessionPayload(REASON_ABORTED_SESSION, false);
    }

    return TelemetryController.saveAbortedSessionPing(payload);
  },

  async markNewProfilePingSent() {
    this._log.trace("markNewProfilePingSent");
    this._newProfilePingSent = true;
    return TelemetryStorage.saveSessionData(this._getSessionDataObject());
  },
};
