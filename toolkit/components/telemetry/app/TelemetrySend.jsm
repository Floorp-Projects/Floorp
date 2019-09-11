/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module is responsible for uploading pings to the server and persisting
 * pings that can't be send now.
 * Those pending pings are persisted on disk and sent at the next opportunity,
 * newest first.
 */

"use strict";

var EXPORTED_SYMBOLS = ["TelemetrySend"];

ChromeUtils.import("resource://gre/modules/AppConstants.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
const { ClientID } = ChromeUtils.import("resource://gre/modules/ClientID.jsm");
ChromeUtils.import("resource://gre/modules/Log.jsm", this);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
ChromeUtils.import("resource://gre/modules/ServiceRequest.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Timer.jsm", this);

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryStorage",
  "resource://gre/modules/TelemetryStorage.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryReportingPolicy",
  "resource://gre/modules/TelemetryReportingPolicy.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyServiceGetter(
  this,
  "Telemetry",
  "@mozilla.org/base/telemetry;1",
  "nsITelemetry"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryHealthPing",
  "resource://gre/modules/HealthPing.jsm"
);

const Utils = TelemetryUtils;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetrySend::";

const TOPIC_IDLE_DAILY = "idle-daily";
// The following topics are notified when Firefox is closing
// because the OS is shutting down.
const TOPIC_QUIT_APPLICATION_GRANTED = "quit-application-granted";
const TOPIC_QUIT_APPLICATION_FORCED = "quit-application-forced";
const PREF_CHANGED_TOPIC = "nsPref:changed";
const TOPIC_PROFILE_CHANGE_NET_TEARDOWN = "profile-change-net-teardown";

// Whether the FHR/Telemetry unification features are enabled.
// Changing this pref requires a restart.
const IS_UNIFIED_TELEMETRY = Services.prefs.getBoolPref(
  TelemetryUtils.Preferences.Unified,
  false
);

const MS_IN_A_MINUTE = 60 * 1000;

const PING_TYPE_OPTOUT = "optout";

// We try to spread "midnight" pings out over this interval.
const MIDNIGHT_FUZZING_INTERVAL_MS = 60 * MS_IN_A_MINUTE;
// We delay sending "midnight" pings on this client by this interval.
const MIDNIGHT_FUZZING_DELAY_MS = Math.random() * MIDNIGHT_FUZZING_INTERVAL_MS;

// Timeout after which we consider a ping submission failed.
const PING_SUBMIT_TIMEOUT_MS = 1.5 * MS_IN_A_MINUTE;

// To keep resource usage in check, we limit ping sending to a maximum number
// of pings per minute.
const MAX_PING_SENDS_PER_MINUTE = 10;

// If we have more pending pings then we can send right now, we schedule the next
// send for after SEND_TICK_DELAY.
const SEND_TICK_DELAY = 1 * MS_IN_A_MINUTE;
// If we had any ping send failures since the last ping, we use a backoff timeout
// for the next ping sends. We increase the delay exponentially up to a limit of
// SEND_MAXIMUM_BACKOFF_DELAY_MS.
// This exponential backoff will be reset by external ping submissions & idle-daily.
const SEND_MAXIMUM_BACKOFF_DELAY_MS = 120 * MS_IN_A_MINUTE;

// Strings to map from XHR.errorCode to TELEMETRY_SEND_FAILURE_TYPE.
// Echoes XMLHttpRequestMainThread's ErrorType enum.
// Make sure that any additions done to XHR_ERROR_TYPE enum are also mirrored in
// TELEMETRY_SEND_FAILURE_TYPE and TELEMETRY_SEND_FAILURE_TYPE_PER_PING's labels.
const XHR_ERROR_TYPE = [
  "eOK",
  "eRequest",
  "eUnreachable",
  "eChannelOpen",
  "eRedirect",
  "eTerminated",
];

/**
 * This is a policy object used to override behavior within this module.
 * Tests override properties on this object to allow for control of behavior
 * that would otherwise be very hard to cover.
 */
var Policy = {
  now: () => new Date(),
  midnightPingFuzzingDelay: () => MIDNIGHT_FUZZING_DELAY_MS,
  pingSubmissionTimeout: () => PING_SUBMIT_TIMEOUT_MS,
  setSchedulerTickTimeout: (callback, delayMs) => setTimeout(callback, delayMs),
  clearSchedulerTickTimeout: id => clearTimeout(id),
  gzipCompressString: data => gzipCompressString(data),
};

/**
 * Determine if the ping has the new v4 ping format or the legacy v2 one or earlier.
 */
function isV4PingFormat(aPing) {
  return (
    "id" in aPing &&
    "application" in aPing &&
    "version" in aPing &&
    aPing.version >= 2
  );
}

/**
 * Check if the provided ping is an optout ping.
 * @param {Object} aPing The ping to check.
 * @return {Boolean} True if the ping is an optout ping, false otherwise.
 */
function isOptoutPing(aPing) {
  return isV4PingFormat(aPing) && aPing.type == PING_TYPE_OPTOUT;
}

/**
 * Save the provided ping as a pending ping.
 * @param {Object} aPing The ping to save.
 * @return {Promise} A promise resolved when the ping is saved.
 */
function savePing(aPing) {
  return TelemetryStorage.savePendingPing(aPing);
}

/**
 * @return {String} This returns a string with the gzip compressed data.
 */
function gzipCompressString(string) {
  let observer = {
    buffer: "",
    onStreamComplete(loader, context, status, length, result) {
      // String.fromCharCode can only deal with 500,000 characters at
      // a time, so chunk the result into parts of that size.
      const chunkSize = 500000;
      for (let offset = 0; offset < result.length; offset += chunkSize) {
        this.buffer += String.fromCharCode.apply(
          String,
          result.slice(offset, offset + chunkSize)
        );
      }
    },
  };

  let scs = Cc["@mozilla.org/streamConverters;1"].getService(
    Ci.nsIStreamConverterService
  );
  let listener = Cc["@mozilla.org/network/stream-loader;1"].createInstance(
    Ci.nsIStreamLoader
  );
  listener.init(observer);
  let converter = scs.asyncConvertData("uncompressed", "gzip", listener, null);
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stringStream.data = string;
  converter.onStartRequest(null, null);
  converter.onDataAvailable(null, stringStream, 0, string.length);
  converter.onStopRequest(null, null, null);
  return observer.buffer;
}

var TelemetrySend = {
  get pendingPingCount() {
    return TelemetrySendImpl.pendingPingCount;
  },

  /**
   * Partial setup that runs immediately at startup. This currently triggers
   * the crash report annotations.
   */
  earlyInit() {
    TelemetrySendImpl.earlyInit();
  },

  /**
   * Initializes this module.
   *
   * @param {Boolean} testing Whether this is run in a test. This changes some behavior
   * to enable proper testing.
   * @return {Promise} Resolved when setup is finished.
   */
  setup(testing = false) {
    return TelemetrySendImpl.setup(testing);
  },

  /**
   * Shutdown this module - this will cancel any pending ping tasks and wait for
   * outstanding async activity like network and disk I/O.
   *
   * @return {Promise} Promise that is resolved when shutdown is finished.
   */
  shutdown() {
    return TelemetrySendImpl.shutdown();
  },

  /**
   * Submit a ping for sending. This will:
   * - send the ping right away if possible or
   * - save the ping to disk and send it at the next opportunity
   *
   * @param {Object} ping The ping data to send, must be serializable to JSON.
   * @param {Object} [aOptions] Options object.
   * @param {Boolean} [options.usePingSender=false] if true, send the ping using the PingSender.
   * @return {Promise} Test-only - a promise that is resolved when the ping is sent or saved.
   */
  submitPing(ping, options = {}) {
    options.usePingSender = options.usePingSender || false;
    return TelemetrySendImpl.submitPing(ping, options);
  },

  /**
   * Check if sending is disabled. If Telemetry is not allowed to upload,
   * pings are not sent to the server.
   * If trying to send an optout ping, don't block it.
   *
   * @param {Object} [ping=null] A ping to be checked.
   * @return {Boolean} True if pings can be send to the servers, false otherwise.
   */
  sendingEnabled(ping = null) {
    return TelemetrySendImpl.sendingEnabled(ping);
  },

  /**
   * Notify that we can start submitting data to the servers.
   */
  notifyCanUpload() {
    return TelemetrySendImpl.notifyCanUpload();
  },

  /**
   * Only used in tests. Used to reset the module data to emulate a restart.
   */
  reset() {
    return TelemetrySendImpl.reset();
  },

  /**
   * Only used in tests.
   */
  setServer(server) {
    return TelemetrySendImpl.setServer(server);
  },

  /**
   * Clear out unpersisted, yet to be sent, pings and cancel outgoing ping requests.
   */
  clearCurrentPings() {
    return TelemetrySendImpl.clearCurrentPings();
  },

  /**
   * Only used in tests to wait on outgoing pending pings.
   */
  testWaitOnOutgoingPings() {
    return TelemetrySendImpl.promisePendingPingActivity();
  },

  /**
   * Only used in tests to set whether it is too late in shutdown to send pings.
   */
  testTooLateToSend(tooLate) {
    TelemetrySendImpl._tooLateToSend = tooLate;
  },

  /**
   * Test-only - this allows overriding behavior to enable ping sending in debug builds.
   */
  setTestModeEnabled(testing) {
    TelemetrySendImpl.setTestModeEnabled(testing);
  },

  /**
   * This returns state info for this module for AsyncShutdown timeout diagnostics.
   */
  getShutdownState() {
    return TelemetrySendImpl.getShutdownState();
  },

  /**
   * Send a ping using the ping sender.
   * This method will not wait for the ping to be sent, instead it will return
   * as soon as the pingsender program has been launched.
   *
   * This method is currently exposed here only for testing purposes as it's
   * only used internally.
   *
   * @param {String} aUrl The telemetry server URL
   * @param {String} aPingFilePath The path to the file holding the ping
   *        contents, if if sent successfully the pingsender will delete it.
   * @param {callback} observer A function called with parameters
            (subject, topic, data) and a topic of "process-finished" or
            "process-failed" after pingsender completion.
   *
   * @throws NS_ERROR_FAILURE if we couldn't find or run the pingsender
   *         executable.
   * @throws NS_ERROR_NOT_IMPLEMENTED on Android as the pingsender is not
   *         available.
   */
  testRunPingSender(url, pingPath, observer) {
    return TelemetrySendImpl.runPingSender(url, pingPath, observer);
  },
};

var CancellableTimeout = {
  _deferred: null,
  _timer: null,

  /**
   * This waits until either the given timeout passed or the timeout was cancelled.
   *
   * @param {Number} timeoutMs The timeout in ms.
   * @return {Promise<bool>} Promise that is resolved with false if the timeout was cancelled,
   *                         false otherwise.
   */
  promiseWaitOnTimeout(timeoutMs) {
    if (!this._deferred) {
      this._deferred = PromiseUtils.defer();
      this._timer = Policy.setSchedulerTickTimeout(
        () => this._onTimeout(),
        timeoutMs
      );
    }

    return this._deferred.promise;
  },

  _onTimeout() {
    if (this._deferred) {
      this._deferred.resolve(false);
      this._timer = null;
      this._deferred = null;
    }
  },

  cancelTimeout() {
    if (this._deferred) {
      Policy.clearSchedulerTickTimeout(this._timer);
      this._deferred.resolve(true);
      this._timer = null;
      this._deferred = null;
    }
  },
};

/**
 * SendScheduler implements the timer & scheduling behavior for ping sends.
 */
var SendScheduler = {
  // Whether any ping sends failed since the last tick. If yes, we start with our exponential
  // backoff timeout.
  _sendsFailed: false,
  // The current retry delay after ping send failures. We use this for the exponential backoff,
  // increasing this value everytime we had send failures since the last tick.
  _backoffDelay: SEND_TICK_DELAY,
  _shutdown: false,
  _sendTask: null,
  // A string that tracks the last seen send task state, null if it never ran.
  _sendTaskState: null,

  _logger: null,

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX + "Scheduler::"
      );
    }

    return this._logger;
  },

  shutdown() {
    this._log.trace("shutdown");
    this._shutdown = true;
    CancellableTimeout.cancelTimeout();
    return Promise.resolve(this._sendTask);
  },

  start() {
    this._log.trace("start");
    this._sendsFailed = false;
    this._backoffDelay = SEND_TICK_DELAY;
    this._shutdown = false;
  },

  /**
   * Only used for testing, resets the state to emulate a restart.
   */
  reset() {
    this._log.trace("reset");
    return this.shutdown().then(() => this.start());
  },

  /**
   * Notify the scheduler of a failure in sending out pings that warrants retrying.
   * This will trigger the exponential backoff timer behavior on the next tick.
   */
  notifySendsFailed() {
    this._log.trace("notifySendsFailed");
    if (this._sendsFailed) {
      return;
    }

    this._sendsFailed = true;
    this._log.trace("notifySendsFailed - had send failures");
  },

  /**
   * Returns whether ping submissions are currently throttled.
   */
  isThrottled() {
    const now = Policy.now();
    const nextPingSendTime = this._getNextPingSendTime(now);
    return nextPingSendTime > now.getTime();
  },

  waitOnSendTask() {
    return Promise.resolve(this._sendTask);
  },

  triggerSendingPings(immediately) {
    this._log.trace(
      "triggerSendingPings - active send task: " +
        !!this._sendTask +
        ", immediately: " +
        immediately
    );

    if (!this._sendTask) {
      this._sendTask = this._doSendTask();
      let clear = () => (this._sendTask = null);
      this._sendTask.then(clear, clear);
    } else if (immediately) {
      CancellableTimeout.cancelTimeout();
    }

    return this._sendTask;
  },

  async _doSendTask() {
    this._sendTaskState = "send task started";
    this._backoffDelay = SEND_TICK_DELAY;
    this._sendsFailed = false;

    const resetBackoffTimer = () => {
      this._backoffDelay = SEND_TICK_DELAY;
    };

    for (;;) {
      this._log.trace("_doSendTask iteration");
      this._sendTaskState = "start iteration";

      if (this._shutdown) {
        this._log.trace("_doSendTask - shutting down, bailing out");
        this._sendTaskState = "bail out - shutdown check";
        return;
      }

      // Get a list of pending pings, sorted by last modified, descending.
      // Filter out all the pings we can't send now. This addresses scenarios like "optout" pings
      // which can be sent even when upload is disabled.
      let pending = TelemetryStorage.getPendingPingList();
      let current = TelemetrySendImpl.getUnpersistedPings();
      this._log.trace(
        "_doSendTask - pending: " +
          pending.length +
          ", current: " +
          current.length
      );
      // Note that the two lists contain different kind of data. |pending| only holds ping
      // info, while |current| holds actual ping data.
      if (!TelemetrySendImpl.sendingEnabled()) {
        // If sending is disabled, only handle an unpersisted optout ping
        pending = [];
        current = current.filter(p => isOptoutPing(p));
      }
      this._log.trace(
        "_doSendTask - can send - pending: " +
          pending.length +
          ", current: " +
          current.length
      );

      // Bail out if there is nothing to send.
      if (pending.length == 0 && current.length == 0) {
        this._log.trace("_doSendTask - no pending pings, bailing out");
        this._sendTaskState = "bail out - no pings to send";
        return;
      }

      // If we are currently throttled (e.g. fuzzing to avoid midnight spikes), wait for the next send window.
      const now = Policy.now();
      if (this.isThrottled()) {
        const nextPingSendTime = this._getNextPingSendTime(now);
        this._log.trace(
          "_doSendTask - throttled, delaying ping send to " +
            new Date(nextPingSendTime)
        );
        this._sendTaskState = "wait for throttling to pass";

        const delay = nextPingSendTime - now.getTime();
        const cancelled = await CancellableTimeout.promiseWaitOnTimeout(delay);
        if (cancelled) {
          this._log.trace(
            "_doSendTask - throttling wait was cancelled, resetting backoff timer"
          );
          resetBackoffTimer();
        }

        continue;
      }

      let sending = pending.slice(0, MAX_PING_SENDS_PER_MINUTE);
      pending = pending.slice(MAX_PING_SENDS_PER_MINUTE);
      this._log.trace(
        "_doSendTask - triggering sending of " +
          sending.length +
          " pings now" +
          ", " +
          pending.length +
          " pings waiting"
      );

      this._sendsFailed = false;
      const sendStartTime = Policy.now();
      this._sendTaskState = "wait on ping sends";
      await TelemetrySendImpl.sendPings(current, sending.map(p => p.id));
      if (this._shutdown || TelemetrySend.pendingPingCount == 0) {
        this._log.trace(
          "_doSendTask - bailing out after sending, shutdown: " +
            this._shutdown +
            ", pendingPingCount: " +
            TelemetrySend.pendingPingCount
        );
        this._sendTaskState = "bail out - shutdown & pending check after send";
        return;
      }

      // Calculate the delay before sending the next batch of pings.
      // We start with a delay that makes us send max. 1 batch per minute.
      // If we had send failures in the last batch, we will override this with
      // a backoff delay.
      const timeSinceLastSend = Policy.now() - sendStartTime;
      let nextSendDelay = Math.max(0, SEND_TICK_DELAY - timeSinceLastSend);

      if (!this._sendsFailed) {
        this._log.trace(
          "_doSendTask - had no send failures, resetting backoff timer"
        );
        resetBackoffTimer();
      } else {
        const newDelay = Math.min(
          SEND_MAXIMUM_BACKOFF_DELAY_MS,
          this._backoffDelay * 2
        );
        this._log.trace(
          "_doSendTask - had send failures, backing off -" +
            " old timeout: " +
            this._backoffDelay +
            ", new timeout: " +
            newDelay
        );
        this._backoffDelay = newDelay;
        nextSendDelay = this._backoffDelay;
      }

      this._log.trace(
        "_doSendTask - waiting for next send opportunity, timeout is " +
          nextSendDelay
      );
      this._sendTaskState = "wait on next send opportunity";
      const cancelled = await CancellableTimeout.promiseWaitOnTimeout(
        nextSendDelay
      );
      if (cancelled) {
        this._log.trace(
          "_doSendTask - batch send wait was cancelled, resetting backoff timer"
        );
        resetBackoffTimer();
      }
    }
  },

  /**
   * This helper calculates the next time that we can send pings at.
   * Currently this mostly redistributes ping sends from midnight until one hour after
   * to avoid submission spikes around local midnight for daily pings.
   *
   * @param now Date The current time.
   * @return Number The next time (ms from UNIX epoch) when we can send pings.
   */
  _getNextPingSendTime(now) {
    // 1. First we check if the pref is set to skip any delay and send immediately.
    // 2. Next we check if the time is between 0am and 1am. If it's not, we send
    // immediately.
    // 3. If we confirmed the time is indeed between 0am and 1am in step 1, we disallow
    // sending before (midnight + fuzzing delay), which is a random time between 0am-1am
    // (decided at startup).

    let disableFuzzingDelay = Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.DisableFuzzingDelay,
      false
    );
    if (disableFuzzingDelay) {
      return now.getTime();
    }

    const midnight = Utils.truncateToDays(now);
    // Don't delay pings if we are not within the fuzzing interval.
    if (now.getTime() - midnight.getTime() > MIDNIGHT_FUZZING_INTERVAL_MS) {
      return now.getTime();
    }

    // Delay ping send if we are within the midnight fuzzing range.
    // We spread those ping sends out between |midnight| and |midnight + midnightPingFuzzingDelay|.
    return midnight.getTime() + Policy.midnightPingFuzzingDelay();
  },

  getShutdownState() {
    return {
      shutdown: this._shutdown,
      hasSendTask: !!this._sendTask,
      sendsFailed: this._sendsFailed,
      sendTaskState: this._sendTaskState,
      backoffDelay: this._backoffDelay,
    };
  },
};

var TelemetrySendImpl = {
  _sendingEnabled: false,
  // Tracks the shutdown state.
  _shutdown: false,
  _logger: null,
  // This tracks all pending ping requests to the server.
  _pendingPingRequests: new Map(),
  // This tracks all the pending async ping activity.
  _pendingPingActivity: new Set(),
  // This is true when running in the test infrastructure.
  _testMode: false,
  // This holds pings that we currently try and haven't persisted yet.
  _currentPings: new Map(),
  // Used to skip spawning the pingsender if OS is shutting down.
  _isOSShutdown: false,
  // Has the network shut down, making it too late to send pings?
  _tooLateToSend: false,

  OBSERVER_TOPICS: [
    TOPIC_IDLE_DAILY,
    TOPIC_QUIT_APPLICATION_GRANTED,
    TOPIC_QUIT_APPLICATION_FORCED,
    TOPIC_PROFILE_CHANGE_NET_TEARDOWN,
  ],

  OBSERVED_PREFERENCES: [
    TelemetryUtils.Preferences.TelemetryEnabled,
    TelemetryUtils.Preferences.FhrUploadEnabled,
  ],

  // Whether sending pings has been overridden.
  get _overrideOfficialCheck() {
    return Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.OverrideOfficialCheck,
      false
    );
  },

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX
      );
    }

    return this._logger;
  },

  get pendingPingRequests() {
    return this._pendingPingRequests;
  },

  get pendingPingCount() {
    return (
      TelemetryStorage.getPendingPingList().length + this._currentPings.size
    );
  },

  setTestModeEnabled(testing) {
    this._testMode = testing;
  },

  earlyInit() {
    this._annotateCrashReport();

    // Install the observer to detect OS shutdown early enough, so
    // that we catch this before the delayed setup happens.
    Services.obs.addObserver(this, TOPIC_QUIT_APPLICATION_FORCED);
    Services.obs.addObserver(this, TOPIC_QUIT_APPLICATION_GRANTED);
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsISupportsWeakReference]),

  async setup(testing) {
    this._log.trace("setup");

    this._testMode = testing;

    Services.obs.addObserver(this, TOPIC_IDLE_DAILY);
    Services.obs.addObserver(this, TOPIC_PROFILE_CHANGE_NET_TEARDOWN);

    this._server = Services.prefs.getStringPref(
      TelemetryUtils.Preferences.Server,
      undefined
    );
    this._sendingEnabled = true;

    // Annotate crash reports so that crash pings are sent correctly and listen
    // to pref changes to adjust the annotations accordingly.
    for (let pref of this.OBSERVED_PREFERENCES) {
      Services.prefs.addObserver(pref, this, true);
    }
    this._annotateCrashReport();

    // Check the pending pings on disk now.
    try {
      await this._checkPendingPings();
    } catch (ex) {
      this._log.error("setup - _checkPendingPings rejected", ex);
    }

    // Enforce the pending pings storage quota. It could take a while so don't
    // block on it.
    TelemetryStorage.runEnforcePendingPingsQuotaTask();

    // Start sending pings, but don't block on this.
    SendScheduler.triggerSendingPings(true);
  },

  /**
   * Triggers the crash report annotations depending on the current
   * configuration. This communicates to the crash reporter if it can send a
   * crash ping or not. This method can be called safely before setup() has
   * been called.
   */
  _annotateCrashReport() {
    try {
      const cr = Cc["@mozilla.org/toolkit/crash-reporter;1"];
      if (cr) {
        const crs = cr.getService(Ci.nsICrashReporter);

        let clientId = ClientID.getCachedClientID();
        let server =
          this._server ||
          Services.prefs.getStringPref(
            TelemetryUtils.Preferences.Server,
            undefined
          );

        if (!this.sendingEnabled() || !TelemetryReportingPolicy.canUpload()) {
          // If we cannot send pings then clear the crash annotations
          crs.removeCrashReportAnnotation("TelemetryClientId");
          crs.removeCrashReportAnnotation("TelemetryServerURL");
        } else {
          crs.annotateCrashReport("TelemetryClientId", clientId);
          crs.annotateCrashReport("TelemetryServerURL", server);
        }
      }
    } catch (e) {
      // Ignore errors when crash reporting is disabled
    }
  },

  /**
   * Discard old pings from the pending pings and detect overdue ones.
   * @return {Boolean} True if we have overdue pings, false otherwise.
   */
  async _checkPendingPings() {
    // Scan the pending pings - that gives us a list sorted by last modified, descending.
    let infos = await TelemetryStorage.loadPendingPingList();
    this._log.info("_checkPendingPings - pending ping count: " + infos.length);
    if (infos.length == 0) {
      this._log.trace("_checkPendingPings - no pending pings");
      return;
    }

    const now = Policy.now();

    // Submit the age of the pending pings.
    for (let pingInfo of infos) {
      const ageInDays = Utils.millisecondsToDays(
        Math.abs(now.getTime() - pingInfo.lastModificationDate)
      );
      Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_AGE").add(ageInDays);
    }
  },

  async shutdown() {
    this._shutdown = true;

    for (let pref of this.OBSERVED_PREFERENCES) {
      // FIXME: When running tests this causes errors to be printed out if
      // TelemetrySend.shutdown() is called twice in a row without calling
      // TelemetrySend.setup() in-between.
      Services.prefs.removeObserver(pref, this);
    }

    for (let topic of this.OBSERVER_TOPICS) {
      try {
        Services.obs.removeObserver(this, topic);
      } catch (ex) {
        this._log.error(
          "shutdown - failed to remove observer for " + topic,
          ex
        );
      }
    }

    // We can't send anymore now.
    this._sendingEnabled = false;

    // Cancel any outgoing requests.
    await this._cancelOutgoingRequests();

    // Stop any active send tasks.
    await SendScheduler.shutdown();

    // Wait for any outstanding async ping activity.
    await this.promisePendingPingActivity();

    // Save any outstanding pending pings to disk.
    await this._persistCurrentPings();
  },

  reset() {
    this._log.trace("reset");

    this._shutdown = false;
    this._currentPings = new Map();
    this._tooLateToSend = false;
    this._isOSShutdown = false;
    this._sendingEnabled = true;

    const histograms = [
      "TELEMETRY_SUCCESS",
      "TELEMETRY_SEND_SUCCESS",
      "TELEMETRY_SEND_FAILURE",
      "TELEMETRY_SEND_FAILURE_TYPE",
    ];

    histograms.forEach(h => Telemetry.getHistogramById(h).clear());

    const keyedHistograms = ["TELEMETRY_SEND_FAILURE_TYPE_PER_PING"];

    keyedHistograms.forEach(h => Telemetry.getKeyedHistogramById(h).clear());

    return SendScheduler.reset();
  },

  /**
   * Notify that we can start submitting data to the servers.
   */
  notifyCanUpload() {
    if (!this._sendingEnabled) {
      this._log.trace(
        "notifyCanUpload - notifying before sending is enabled. Ignoring."
      );
      return Promise.resolve();
    }
    // Let the scheduler trigger sending pings if possible, also inform the
    // crash reporter that it can send crash pings if appropriate.
    SendScheduler.triggerSendingPings(true);
    this._annotateCrashReport();

    return this.promisePendingPingActivity();
  },

  observe(subject, topic, data) {
    let setOSShutdown = () => {
      this._log.trace("setOSShutdown - in OS shutdown");
      this._isOSShutdown = true;
    };

    switch (topic) {
      case TOPIC_IDLE_DAILY:
        SendScheduler.triggerSendingPings(true);
        break;
      case TOPIC_QUIT_APPLICATION_FORCED:
        setOSShutdown();
        break;
      case TOPIC_QUIT_APPLICATION_GRANTED:
        if (data == "syncShutdown") {
          setOSShutdown();
        }
        break;
      case PREF_CHANGED_TOPIC:
        if (this.OBSERVED_PREFERENCES.includes(data)) {
          this._annotateCrashReport();
        }
        break;
      case TOPIC_PROFILE_CHANGE_NET_TEARDOWN:
        this._tooLateToSend = true;
        break;
    }
  },

  /**
   * Spawn the PingSender process that sends a ping. This function does
   * not return an error or throw, it only logs an error.
   *
   * Even if the function doesn't fail, it doesn't mean that the ping was
   * successfully sent, as we have no control over the spawned process. If it,
   * succeeds, the ping is eventually removed from the disk to prevent duplicated
   * submissions.
   *
   * @param {String} pingId The id of the ping to send.
   * @param {String} submissionURL The complete Telemetry-compliant URL for the ping.
   */
  _sendWithPingSender(pingId, submissionURL) {
    this._log.trace(
      "_sendWithPingSender - sending " + pingId + " to " + submissionURL
    );
    try {
      const pingPath = OS.Path.join(TelemetryStorage.pingDirectoryPath, pingId);
      this.runPingSender(submissionURL, pingPath);
    } catch (e) {
      this._log.error("_sendWithPingSender - failed to submit ping", e);
    }
  },

  submitPing(ping, options) {
    this._log.trace(
      "submitPing - ping id: " +
        ping.id +
        ", options: " +
        JSON.stringify(options)
    );

    if (!this.sendingEnabled(ping)) {
      this._log.trace("submitPing - Telemetry is not allowed to send pings.");
      return Promise.resolve();
    }

    // Send the ping using the PingSender, if requested and the user was
    // notified of our policy. We don't support the pingsender on Android,
    // so ignore this option on that platform (see bug 1335917).
    // Moreover, if the OS is shutting down, we don't want to spawn the
    // pingsender as it could unnecessarily slow down OS shutdown.
    // Additionally, it could be be killed before it can complete its tasks,
    // for example after successfully sending the ping but before removing
    // the copy from the disk, resulting in receiving duplicate pings when
    // Firefox restarts.
    if (
      options.usePingSender &&
      !this._isOSShutdown &&
      TelemetryReportingPolicy.canUpload() &&
      AppConstants.platform != "android"
    ) {
      const url = this._buildSubmissionURL(ping);
      // Serialize the ping to the disk and then spawn the PingSender.
      return savePing(ping).then(() => this._sendWithPingSender(ping.id, url));
    }

    if (!this.canSendNow) {
      // Sending is disabled or throttled, add this to the persisted pending pings.
      this._log.trace(
        "submitPing - can't send ping now, persisting to disk - " +
          "canSendNow: " +
          this.canSendNow
      );
      return savePing(ping);
    }

    // Let the scheduler trigger sending pings if possible.
    // As a safety mechanism, this resets any currently active throttling.
    this._log.trace("submitPing - can send pings, trying to send now");
    this._currentPings.set(ping.id, ping);
    SendScheduler.triggerSendingPings(true);
    return Promise.resolve();
  },

  /**
   * Only used in tests.
   */
  setServer(server) {
    this._log.trace("setServer", server);
    this._server = server;
  },

  /**
   * Clear out unpersisted, yet to be sent, pings and cancel outgoing ping requests.
   */
  async clearCurrentPings() {
    if (this._shutdown) {
      this._log.trace("clearCurrentPings - in shutdown, bailing out");
      return;
    }

    // Temporarily disable the scheduler. It must not try to reschedule ping sending
    // while we're deleting them.
    await SendScheduler.shutdown();

    // Now that the ping activity has settled, abort outstanding ping requests.
    this._cancelOutgoingRequests();

    // Also, purge current pings.
    this._currentPings.clear();

    // We might have been interrupted and shutdown could have been started.
    // We need to bail out in that case to avoid triggering send activity etc.
    // at unexpected times.
    if (this._shutdown) {
      this._log.trace(
        "clearCurrentPings - in shutdown, not spinning SendScheduler up again"
      );
      return;
    }

    // Enable the scheduler again and spin the send task.
    SendScheduler.start();
    SendScheduler.triggerSendingPings(true);
  },

  _cancelOutgoingRequests() {
    // Abort any pending ping XHRs.
    for (let [id, request] of this._pendingPingRequests) {
      this._log.trace(
        "_cancelOutgoingRequests - aborting ping request for id " + id
      );
      try {
        request.abort();
      } catch (e) {
        this._log.error(
          "_cancelOutgoingRequests - failed to abort request for id " + id,
          e
        );
      }
    }
    this._pendingPingRequests.clear();
  },

  sendPings(currentPings, persistedPingIds) {
    let pingSends = [];

    // Prioritize health pings to enable low-latency monitoring.
    currentPings = [
      ...currentPings.filter(ping => ping.type === "health"),
      ...currentPings.filter(ping => ping.type !== "health"),
    ];

    for (let current of currentPings) {
      let ping = current;
      let p = (async () => {
        try {
          await this._doPing(ping, ping.id, false);
        } catch (ex) {
          if (isOptoutPing(ping)) {
            // Optout pings should only be tried once and then discarded.
            this._log.info(
              "sendPings - optout ping " + ping.id + " not sent, discarding",
              ex
            );
          } else {
            this._log.info(
              "sendPings - ping " + ping.id + " not sent, saving to disk",
              ex
            );
            await savePing(ping);
          }
        } finally {
          this._currentPings.delete(ping.id);
        }
      })();

      this._trackPendingPingTask(p);
      pingSends.push(p);
    }

    if (persistedPingIds.length > 0) {
      pingSends.push(
        this._sendPersistedPings(persistedPingIds).catch(ex => {
          this._log.info("sendPings - persisted pings not sent", ex);
        })
      );
    }

    return Promise.all(pingSends);
  },

  /**
   * Send the persisted pings to the server.
   *
   * @param {Array<string>} List of ping ids that should be sent.
   *
   * @return Promise A promise that is resolved when all pings finished sending or failed.
   */
  async _sendPersistedPings(pingIds) {
    this._log.trace("sendPersistedPings");

    if (this.pendingPingCount < 1) {
      this._log.trace("_sendPersistedPings - no pings to send");
      return;
    }

    if (pingIds.length < 1) {
      this._log.trace("sendPersistedPings - no pings to send");
      return;
    }

    // We can send now.
    // If there are any send failures, _doPing() sets up handlers that e.g. trigger backoff timer behavior.
    this._log.trace(
      "sendPersistedPings - sending " + pingIds.length + " pings"
    );
    let pingSendPromises = [];
    for (let pingId of pingIds) {
      const id = pingId;
      pingSendPromises.push(
        TelemetryStorage.loadPendingPing(id)
          .then(data => this._doPing(data, id, true))
          .catch(e =>
            this._log.error("sendPersistedPings - failed to send ping " + id, e)
          )
      );
    }

    let promise = Promise.all(pingSendPromises);
    this._trackPendingPingTask(promise);
    await promise;
  },

  _onPingRequestFinished(success, startTime, id, isPersisted) {
    this._log.trace(
      "_onPingRequestFinished - success: " +
        success +
        ", persisted: " +
        isPersisted
    );

    let sendId = success ? "TELEMETRY_SEND_SUCCESS" : "TELEMETRY_SEND_FAILURE";
    let hsend = Telemetry.getHistogramById(sendId);
    let hsuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");

    hsend.add(Utils.monotonicNow() - startTime);
    hsuccess.add(success);

    if (!success) {
      // Let the scheduler know about send failures for triggering backoff timeouts.
      SendScheduler.notifySendsFailed();
    }

    if (success && isPersisted) {
      return TelemetryStorage.removePendingPing(id);
    }
    return Promise.resolve();
  },

  _buildSubmissionURL(ping) {
    const version = isV4PingFormat(ping)
      ? AppConstants.TELEMETRY_PING_FORMAT_VERSION
      : 1;
    return this._server + this._getSubmissionPath(ping) + "?v=" + version;
  },

  _getSubmissionPath(ping) {
    // The new ping format contains an "application" section, the old one doesn't.
    let pathComponents;
    if (isV4PingFormat(ping)) {
      // We insert the Ping id in the URL to simplify server handling of duplicated
      // pings.
      let app = ping.application;
      pathComponents = [
        ping.id,
        ping.type,
        app.name,
        app.version,
        app.channel,
        app.buildId,
      ];
    } else {
      // This is a ping in the old format.
      if (!("slug" in ping)) {
        // That's odd, we don't have a slug. Generate one so that TelemetryStorage.jsm works.
        ping.slug = Utils.generateUUID();
      }

      // Do we have enough info to build a submission URL?
      let payload = "payload" in ping ? ping.payload : null;
      if (payload && "info" in payload) {
        let info = ping.payload.info;
        pathComponents = [
          ping.slug,
          info.reason,
          info.appName,
          info.appVersion,
          info.appUpdateChannel,
          info.appBuildID,
        ];
      } else {
        // Only use the UUID as the slug.
        pathComponents = [ping.slug];
      }
    }

    let slug = pathComponents.join("/");
    return "/submit/telemetry/" + slug;
  },

  _doPing(ping, id, isPersisted) {
    if (!this.sendingEnabled(ping)) {
      // We can't send the pings to the server, so don't try to.
      this._log.trace("_doPing - Can't send ping " + ping.id);
      return Promise.resolve();
    }

    if (this._tooLateToSend) {
      // Too late to send now. Reject so we pend the ping to send it next time.
      this._log.trace("_doPing - Too late to send ping " + ping.id);
      Telemetry.getHistogramById("TELEMETRY_SEND_FAILURE_TYPE").add("eTooLate");
      Telemetry.getKeyedHistogramById(
        "TELEMETRY_SEND_FAILURE_TYPE_PER_PING"
      ).add(ping.type, "eTooLate");
      return Promise.reject();
    }

    this._log.trace(
      "_doPing - server: " +
        this._server +
        ", persisted: " +
        isPersisted +
        ", id: " +
        id
    );

    const url = this._buildSubmissionURL(ping);

    // Don't send cookies with these requests.
    let request = new ServiceRequest({ mozAnon: true });
    request.mozBackgroundRequest = true;
    request.timeout = Policy.pingSubmissionTimeout();

    request.open("POST", url, true);
    request.overrideMimeType("text/plain");
    request.setRequestHeader("Content-Type", "application/json; charset=UTF-8");
    request.setRequestHeader("Date", Policy.now().toUTCString());

    this._pendingPingRequests.set(id, request);

    const monotonicStartTime = Utils.monotonicNow();
    let deferred = PromiseUtils.defer();

    let onRequestFinished = (success, event) => {
      let onCompletion = () => {
        if (success) {
          deferred.resolve();
        } else {
          deferred.reject(event);
        }
      };

      this._pendingPingRequests.delete(id);
      this._onPingRequestFinished(
        success,
        monotonicStartTime,
        id,
        isPersisted
      ).then(
        () => onCompletion(),
        error => {
          this._log.error(
            "_doPing - request success: " + success + ", error: " + error
          );
          onCompletion();
        }
      );
    };

    let errorhandler = event => {
      let failure = event.type;
      if (failure === "error") {
        failure = XHR_ERROR_TYPE[request.errorCode];
      }

      TelemetryHealthPing.recordSendFailure(failure);

      if (this.fallbackHttp) {
        // only one attempt
        this.fallbackHttp = false;

        request.channel.securityInfo
          .QueryInterface(Ci.nsITransportSecurityInfo)
          .QueryInterface(Ci.nsISerializable);
        if (request.channel.securityInfo.errorCodeString.startsWith("SEC_")) {
          // re-open the request with the HTTP version of the URL
          let fallbackUrl = new URL(url);
          fallbackUrl.protocol = "http:";
          // TODO encrypt payload
          request.open("POST", fallbackUrl, true);
          request.sendInputStream(this.payloadStream);
        }
      }

      Telemetry.getHistogramById("TELEMETRY_SEND_FAILURE_TYPE").add(failure);
      Telemetry.getKeyedHistogramById(
        "TELEMETRY_SEND_FAILURE_TYPE_PER_PING"
      ).add(ping.type, failure);

      this._log.error(
        "_doPing - error making request to " + url + ": " + failure
      );
      onRequestFinished(false, event);
    };
    request.onerror = errorhandler;
    request.ontimeout = errorhandler;
    request.onabort = errorhandler;

    request.onload = event => {
      let status = request.status;
      let statusClass = status - (status % 100);
      let success = false;

      if (statusClass === 200) {
        // We can treat all 2XX as success.
        this._log.info("_doPing - successfully loaded, status: " + status);
        success = true;
      } else if (statusClass === 400) {
        // 4XX means that something with the request was broken.
        this._log.error(
          "_doPing - error submitting to " +
            url +
            ", status: " +
            status +
            " - ping request broken?"
        );
        Telemetry.getHistogramById(
          "TELEMETRY_PING_EVICTED_FOR_SERVER_ERRORS"
        ).add();
        // TODO: we should handle this better, but for now we should avoid resubmitting
        // broken requests by pretending success.
        success = true;
      } else if (statusClass === 500) {
        // 5XX means there was a server-side error and we should try again later.
        this._log.error(
          "_doPing - error submitting to " +
            url +
            ", status: " +
            status +
            " - server error, should retry later"
        );
      } else {
        // We received an unexpected status code.
        this._log.error(
          "_doPing - error submitting to " +
            url +
            ", status: " +
            status +
            ", type: " +
            event.type
        );
      }

      onRequestFinished(success, event);
    };

    // If that's a legacy ping format, just send its payload.
    let networkPayload = isV4PingFormat(ping) ? ping : ping.payload;
    request.setRequestHeader("Content-Encoding", "gzip");
    let converter = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    let startTime = Utils.monotonicNow();
    let utf8Payload = converter.ConvertFromUnicode(
      JSON.stringify(networkPayload)
    );
    utf8Payload += converter.Finish();
    Telemetry.getHistogramById("TELEMETRY_STRINGIFY").add(
      Utils.monotonicNow() - startTime
    );

    let payloadStream = Cc[
      "@mozilla.org/io/string-input-stream;1"
    ].createInstance(Ci.nsIStringInputStream);
    startTime = Utils.monotonicNow();
    payloadStream.data = Policy.gzipCompressString(utf8Payload);

    // Check the size and drop pings which are too big.
    const compressedPingSizeBytes = payloadStream.data.length;
    if (compressedPingSizeBytes > TelemetryStorage.MAXIMUM_PING_SIZE) {
      this._log.error(
        "_doPing - submitted ping exceeds the size limit, size: " +
          compressedPingSizeBytes
      );
      Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_SEND").add();
      Telemetry.getHistogramById("TELEMETRY_DISCARDED_SEND_PINGS_SIZE_MB").add(
        Math.floor(compressedPingSizeBytes / 1024 / 1024)
      );
      // We don't need to call |request.abort()| as it was not sent yet.
      this._pendingPingRequests.delete(id);

      TelemetryHealthPing.recordDiscardedPing(ping.type);
      return TelemetryStorage.removePendingPing(id);
    }

    Telemetry.getHistogramById("TELEMETRY_COMPRESS").add(
      Utils.monotonicNow() - startTime
    );
    request.sendInputStream(payloadStream);

    this.payloadStream = payloadStream;

    return deferred.promise;
  },

  /**
   * Check if sending is temporarily disabled.
   * @return {Boolean} True if we can send pings to the server right now, false if
   *         sending is temporarily disabled.
   */
  get canSendNow() {
    // If the reporting policy was not accepted yet, don't send pings.
    if (!TelemetryReportingPolicy.canUpload()) {
      return false;
    }

    return this._sendingEnabled;
  },

  /**
   * Check if sending is disabled. If Telemetry is not allowed to upload,
   * pings are not sent to the server.
   * If trying to send an optout ping, don't block it.
   * If unified telemetry is off, don't send pings if Telemetry is disabled.
   *
   * @param {Object} [ping=null] A ping to be checked.
   * @return {Boolean} True if pings can be send to the servers, false otherwise.
   */
  sendingEnabled(ping = null) {
    // We only send pings from official builds, but allow overriding this for tests.
    if (
      !Telemetry.isOfficialTelemetry &&
      !this._testMode &&
      !this._overrideOfficialCheck
    ) {
      return false;
    }

    // With unified Telemetry, the FHR upload setting controls whether we can send pings.
    // The Telemetry pref enables sending extended data sets instead.
    if (IS_UNIFIED_TELEMETRY) {
      // Optout pings are sent once even if the upload is disabled.
      if (ping && isOptoutPing(ping)) {
        return true;
      }
      return Services.prefs.getBoolPref(
        TelemetryUtils.Preferences.FhrUploadEnabled,
        false
      );
    }

    // Without unified Telemetry, the Telemetry enabled pref controls ping sending.
    return Utils.isTelemetryEnabled;
  },

  /**
   * Track any pending ping send and save tasks through the promise passed here.
   * This is needed to block shutdown on any outstanding ping activity.
   */
  _trackPendingPingTask(promise) {
    let clear = () => this._pendingPingActivity.delete(promise);
    promise.then(clear, clear);
    this._pendingPingActivity.add(promise);
  },

  /**
   * Return a promise that allows to wait on pending pings.
   * @return {Object<Promise>} A promise resolved when all the pending pings promises
   *         are resolved.
   */
  promisePendingPingActivity() {
    this._log.trace("promisePendingPingActivity - Waiting for ping task");
    let p = Array.from(this._pendingPingActivity, p =>
      p.catch(ex => {
        this._log.error(
          "promisePendingPingActivity - ping activity had an error",
          ex
        );
      })
    );
    p.push(SendScheduler.waitOnSendTask());
    return Promise.all(p);
  },

  async _persistCurrentPings() {
    for (let [id, ping] of this._currentPings) {
      try {
        // Never save an optout ping to disk
        if (!isOptoutPing(ping)) {
          await savePing(ping);
          this._log.trace("_persistCurrentPings - saved ping " + id);
        }
      } catch (ex) {
        this._log.error("_persistCurrentPings - failed to save ping " + id, ex);
      } finally {
        this._currentPings.delete(id);
      }
    }
  },

  /**
   * Returns the current pending, not yet persisted, pings, newest first.
   */
  getUnpersistedPings() {
    let current = [...this._currentPings.values()];
    current.reverse();
    return current;
  },

  getShutdownState() {
    return {
      sendingEnabled: this._sendingEnabled,
      pendingPingRequestCount: this._pendingPingRequests.size,
      pendingPingActivityCount: this._pendingPingActivity.size,
      unpersistedPingCount: this._currentPings.size,
      persistedPingCount: TelemetryStorage.getPendingPingList().length,
      schedulerState: SendScheduler.getShutdownState(),
    };
  },

  runPingSender(url, pingPath, observer) {
    if (AppConstants.platform === "android") {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    }

    const exeName =
      AppConstants.platform === "win" ? "pingsender.exe" : "pingsender";

    let exe = Services.dirsvc.get("GreBinD", Ci.nsIFile);
    exe.append(exeName);

    let process = Cc["@mozilla.org/process/util;1"].createInstance(
      Ci.nsIProcess
    );
    process.init(exe);
    process.startHidden = true;
    process.noShell = true;
    process.runAsync([url, pingPath], 2, observer);
  },
};
