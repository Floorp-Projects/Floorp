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

this.EXPORTED_SYMBOLS = [
  "TelemetrySend",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Log.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/PromiseUtils.jsm");
Cu.import("resource://gre/modules/ServiceRequest.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStorage",
                                  "resource://gre/modules/TelemetryStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryReportingPolicy",
                                  "resource://gre/modules/TelemetryReportingPolicy.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "Telemetry",
                                   "@mozilla.org/base/telemetry;1",
                                   "nsITelemetry");

const Utils = TelemetryUtils;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetrySend::";

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_SERVER = PREF_BRANCH + "server";
const PREF_UNIFIED = PREF_BRANCH + "unified";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

const TOPIC_IDLE_DAILY = "idle-daily";
const TOPIC_QUIT_APPLICATION = "quit-application";

// Whether the FHR/Telemetry unification features are enabled.
// Changing this pref requires a restart.
const IS_UNIFIED_TELEMETRY = Preferences.get(PREF_UNIFIED, false);

const PING_FORMAT_VERSION = 4;

const MS_IN_A_MINUTE = 60 * 1000;

const PING_TYPE_DELETION = "deletion";

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

// The age of a pending ping to be considered overdue (in milliseconds).
const OVERDUE_PING_FILE_AGE = 7 * 24 * 60 * MS_IN_A_MINUTE; // 1 week

function monotonicNow() {
  try {
    return Telemetry.msSinceProcessStart();
  } catch (ex) {
    // If this fails fall back to the (non-monotonic) Date value.
    return Date.now();
  }
}

/**
 * This is a policy object used to override behavior within this module.
 * Tests override properties on this object to allow for control of behavior
 * that would otherwise be very hard to cover.
 */
var Policy = {
  now: () => new Date(),
  midnightPingFuzzingDelay: () => MIDNIGHT_FUZZING_DELAY_MS,
  setSchedulerTickTimeout: (callback, delayMs) => setTimeout(callback, delayMs),
  clearSchedulerTickTimeout: (id) => clearTimeout(id),
};

/**
 * Determine if the ping has the new v4 ping format or the legacy v2 one or earlier.
 */
function isV4PingFormat(aPing) {
  return ("id" in aPing) && ("application" in aPing) &&
         ("version" in aPing) && (aPing.version >= 2);
}

/**
 * Check if the provided ping is a deletion ping.
 * @param {Object} aPing The ping to check.
 * @return {Boolean} True if the ping is a deletion ping, false otherwise.
 */
function isDeletionPing(aPing) {
  return isV4PingFormat(aPing) && (aPing.type == PING_TYPE_DELETION);
}

/**
 * Save the provided ping as a pending ping. If it's a deletion ping, save it
 * to a special location.
 * @param {Object} aPing The ping to save.
 * @return {Promise} A promise resolved when the ping is saved.
 */
function savePing(aPing) {
  if (isDeletionPing(aPing)) {
    return TelemetryStorage.saveDeletionPing(aPing);
  }
  return TelemetryStorage.savePendingPing(aPing);
}

/**
 * @return {String} This returns a string with the gzip compressed data.
 */
function gzipCompressString(string) {
  let observer = {
    buffer: "",
    onStreamComplete(loader, context, status, length, result) {
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
}

this.TelemetrySend = {

  /**
   * Age in ms of a pending ping to be considered overdue.
   */
  get OVERDUE_PING_FILE_AGE() {
    return OVERDUE_PING_FILE_AGE;
  },

  get pendingPingCount() {
    return TelemetrySendImpl.pendingPingCount;
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
   * @return {Promise} Test-only - a promise that is resolved when the ping is sent or saved.
   */
  submitPing(ping) {
    return TelemetrySendImpl.submitPing(ping);
  },

  /**
   * Count of pending pings that were found to be overdue at startup.
   */
  get overduePingsCount() {
    return TelemetrySendImpl.overduePingsCount;
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
      this._timer = Policy.setSchedulerTickTimeout(() => this._onTimeout(), timeoutMs);
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
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX + "Scheduler::");
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
    return (nextPingSendTime > now.getTime());
  },

  waitOnSendTask() {
    return Promise.resolve(this._sendTask);
  },

  triggerSendingPings(immediately) {
    this._log.trace("triggerSendingPings - active send task: " + !!this._sendTask + ", immediately: " + immediately);

    if (!this._sendTask) {
      this._sendTask = this._doSendTask();
      let clear = () => this._sendTask = null;
      this._sendTask.then(clear, clear);
    } else if (immediately) {
      CancellableTimeout.cancelTimeout();
    }

    return this._sendTask;
  },

  _doSendTask: Task.async(function*() {
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
      // Filter out all the pings we can't send now. This addresses scenarios like "deletion" pings
      // which can be send even when upload is disabled.
      let pending = TelemetryStorage.getPendingPingList();
      let current = TelemetrySendImpl.getUnpersistedPings();
      this._log.trace("_doSendTask - pending: " + pending.length + ", current: " + current.length);
      // Note that the two lists contain different kind of data. |pending| only holds ping
      // info, while |current| holds actual ping data.
      if (!TelemetrySendImpl.sendingEnabled()) {
        pending = pending.filter(pingInfo => TelemetryStorage.isDeletionPing(pingInfo.id));
        current = current.filter(p => isDeletionPing(p));
      }
      this._log.trace("_doSendTask - can send - pending: " + pending.length + ", current: " + current.length);

      // Bail out if there is nothing to send.
      if ((pending.length == 0) && (current.length == 0)) {
        this._log.trace("_doSendTask - no pending pings, bailing out");
        this._sendTaskState = "bail out - no pings to send";
        return;
      }

      // If we are currently throttled (e.g. fuzzing to avoid midnight spikes), wait for the next send window.
      const now = Policy.now();
      if (this.isThrottled()) {
        const nextPingSendTime = this._getNextPingSendTime(now);
        this._log.trace("_doSendTask - throttled, delaying ping send to " + new Date(nextPingSendTime));
        this._sendTaskState = "wait for throttling to pass";

        const delay = nextPingSendTime - now.getTime();
        const cancelled = yield CancellableTimeout.promiseWaitOnTimeout(delay);
        if (cancelled) {
          this._log.trace("_doSendTask - throttling wait was cancelled, resetting backoff timer");
          resetBackoffTimer();
        }

        continue;
      }

      let sending = pending.slice(0, MAX_PING_SENDS_PER_MINUTE);
      pending = pending.slice(MAX_PING_SENDS_PER_MINUTE);
      this._log.trace("_doSendTask - triggering sending of " + sending.length + " pings now" +
                      ", " + pending.length + " pings waiting");

      this._sendsFailed = false;
      const sendStartTime = Policy.now();
      this._sendTaskState = "wait on ping sends";
      yield TelemetrySendImpl.sendPings(current, sending.map(p => p.id));
      if (this._shutdown || (TelemetrySend.pendingPingCount == 0)) {
        this._log.trace("_doSendTask - bailing out after sending, shutdown: " + this._shutdown +
                        ", pendingPingCount: " + TelemetrySend.pendingPingCount);
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
        this._log.trace("_doSendTask - had no send failures, resetting backoff timer");
        resetBackoffTimer();
      } else {
        const newDelay = Math.min(SEND_MAXIMUM_BACKOFF_DELAY_MS,
                                  this._backoffDelay * 2);
        this._log.trace("_doSendTask - had send failures, backing off -" +
                        " old timeout: " + this._backoffDelay +
                        ", new timeout: " + newDelay);
        this._backoffDelay = newDelay;
        nextSendDelay = this._backoffDelay;
      }

      this._log.trace("_doSendTask - waiting for next send opportunity, timeout is " + nextSendDelay)
      this._sendTaskState = "wait on next send opportunity";
      const cancelled = yield CancellableTimeout.promiseWaitOnTimeout(nextSendDelay);
      if (cancelled) {
        this._log.trace("_doSendTask - batch send wait was cancelled, resetting backoff timer");
        resetBackoffTimer();
      }
    }
  }),

  /**
   * This helper calculates the next time that we can send pings at.
   * Currently this mostly redistributes ping sends from midnight until one hour after
   * to avoid submission spikes around local midnight for daily pings.
   *
   * @param now Date The current time.
   * @return Number The next time (ms from UNIX epoch) when we can send pings.
   */
  _getNextPingSendTime(now) {
    // 1. First we check if the time is between 0am and 1am. If it's not, we send
    // immediately.
    // 2. If we confirmed the time is indeed between 0am and 1am in step 1, we disallow
    // sending before (midnight + fuzzing delay), which is a random time between 0am-1am
    // (decided at startup).

    const midnight = Utils.truncateToDays(now);
    // Don't delay pings if we are not within the fuzzing interval.
    if ((now.getTime() - midnight.getTime()) > MIDNIGHT_FUZZING_INTERVAL_MS) {
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

  // Count of pending pings that were overdue.
  _overduePingCount: 0,

  OBSERVER_TOPICS: [
    TOPIC_IDLE_DAILY,
  ],

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
    }

    return this._logger;
  },

  get overduePingsCount() {
    return this._overduePingCount;
  },

  get pendingPingRequests() {
    return this._pendingPingRequests;
  },

  get pendingPingCount() {
    return TelemetryStorage.getPendingPingList().length + this._currentPings.size;
  },

  setTestModeEnabled(testing) {
    this._testMode = testing;
  },

  setup: Task.async(function*(testing) {
    this._log.trace("setup");

    this._testMode = testing;
    this._sendingEnabled = true;

    Services.obs.addObserver(this, TOPIC_IDLE_DAILY, false);

    this._server = Preferences.get(PREF_SERVER, undefined);

    // Check the pending pings on disk now.
    try {
      yield this._checkPendingPings();
    } catch (ex) {
      this._log.error("setup - _checkPendingPings rejected", ex);
    }

    // Enforce the pending pings storage quota. It could take a while so don't
    // block on it.
    TelemetryStorage.runEnforcePendingPingsQuotaTask();

    // Start sending pings, but don't block on this.
    SendScheduler.triggerSendingPings(true);
  }),

  /**
   * Discard old pings from the pending pings and detect overdue ones.
   * @return {Boolean} True if we have overdue pings, false otherwise.
   */
  _checkPendingPings: Task.async(function*() {
    // Scan the pending pings - that gives us a list sorted by last modified, descending.
    let infos = yield TelemetryStorage.loadPendingPingList();
    this._log.info("_checkPendingPings - pending ping count: " + infos.length);
    if (infos.length == 0) {
      this._log.trace("_checkPendingPings - no pending pings");
      return;
    }

    const now = Policy.now();

    // Check for overdue pings.
    const overduePings = infos.filter((info) =>
      (now.getTime() - info.lastModificationDate) > OVERDUE_PING_FILE_AGE);
    this._overduePingCount = overduePings.length;

    // Submit the age of the pending pings.
    for (let pingInfo of infos) {
      const ageInDays =
        Utils.millisecondsToDays(Math.abs(now.getTime() - pingInfo.lastModificationDate));
      Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_AGE").add(ageInDays);
    }
   }),

  shutdown: Task.async(function*() {
    this._shutdown = true;

    for (let topic of this.OBSERVER_TOPICS) {
      try {
        Services.obs.removeObserver(this, topic);
      } catch (ex) {
        this._log.error("shutdown - failed to remove observer for " + topic, ex);
      }
    }

    // We can't send anymore now.
    this._sendingEnabled = false;

    // Cancel any outgoing requests.
    yield this._cancelOutgoingRequests();

    // Stop any active send tasks.
    yield SendScheduler.shutdown();

    // Wait for any outstanding async ping activity.
    yield this.promisePendingPingActivity();

    // Save any outstanding pending pings to disk.
    yield this._persistCurrentPings();
  }),

  reset() {
    this._log.trace("reset");

    this._shutdown = false;
    this._currentPings = new Map();
    this._overduePingCount = 0;

    const histograms = [
      "TELEMETRY_SUCCESS",
      "TELEMETRY_SEND_SUCCESS",
      "TELEMETRY_SEND_FAILURE",
    ];

    histograms.forEach(h => Telemetry.getHistogramById(h).clear());

    return SendScheduler.reset();
  },

  /**
   * Notify that we can start submitting data to the servers.
   */
  notifyCanUpload() {
    // Let the scheduler trigger sending pings if possible.
    SendScheduler.triggerSendingPings(true);
    return this.promisePendingPingActivity();
  },

  observe(subject, topic, data) {
    switch (topic) {
    case TOPIC_IDLE_DAILY:
      SendScheduler.triggerSendingPings(true);
      break;
    }
  },

  submitPing(ping) {
    this._log.trace("submitPing - ping id: " + ping.id);

    if (!this.sendingEnabled(ping)) {
      this._log.trace("submitPing - Telemetry is not allowed to send pings.");
      return Promise.resolve();
    }

    if (!this.canSendNow) {
      // Sending is disabled or throttled, add this to the persisted pending pings.
      this._log.trace("submitPing - can't send ping now, persisting to disk - " +
                      "canSendNow: " + this.canSendNow);
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
  clearCurrentPings: Task.async(function*() {
    if (this._shutdown) {
      this._log.trace("clearCurrentPings - in shutdown, bailing out");
      return;
    }

    // Temporarily disable the scheduler. It must not try to reschedule ping sending
    // while we're deleting them.
    yield SendScheduler.shutdown();

    // Now that the ping activity has settled, abort outstanding ping requests.
    this._cancelOutgoingRequests();

    // Also, purge current pings.
    this._currentPings.clear();

    // We might have been interrupted and shutdown could have been started.
    // We need to bail out in that case to avoid triggering send activity etc.
    // at unexpected times.
    if (this._shutdown) {
      this._log.trace("clearCurrentPings - in shutdown, not spinning SendScheduler up again");
      return;
    }

    // Enable the scheduler again and spin the send task.
    SendScheduler.start();
    SendScheduler.triggerSendingPings(true);
  }),

  _cancelOutgoingRequests() {
    // Abort any pending ping XHRs.
    for (let [id, request] of this._pendingPingRequests) {
      this._log.trace("_cancelOutgoingRequests - aborting ping request for id " + id);
      try {
        request.abort();
      } catch (e) {
        this._log.error("_cancelOutgoingRequests - failed to abort request for id " + id, e);
      }
    }
    this._pendingPingRequests.clear();
  },

  sendPings(currentPings, persistedPingIds) {
    let pingSends = [];

    for (let current of currentPings) {
      let ping = current;
      let p = Task.spawn(function*() {
        try {
          yield this._doPing(ping, ping.id, false);
        } catch (ex) {
          this._log.info("sendPings - ping " + ping.id + " not sent, saving to disk", ex);
          // Deletion pings must be saved to a special location.
          yield savePing(ping);
        } finally {
          this._currentPings.delete(ping.id);
        }
      }.bind(this));

      this._trackPendingPingTask(p);
      pingSends.push(p);
    }

    if (persistedPingIds.length > 0) {
      pingSends.push(this._sendPersistedPings(persistedPingIds).catch((ex) => {
        this._log.info("sendPings - persisted pings not sent", ex);
      }));
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
  _sendPersistedPings: Task.async(function*(pingIds) {
    this._log.trace("sendPersistedPings");

    if (TelemetryStorage.pendingPingCount < 1) {
      this._log.trace("_sendPersistedPings - no pings to send");
      return;
    }

    if (pingIds.length < 1) {
      this._log.trace("sendPersistedPings - no pings to send");
      return;
    }

    // We can send now.
    // If there are any send failures, _doPing() sets up handlers that e.g. trigger backoff timer behavior.
    this._log.trace("sendPersistedPings - sending " + pingIds.length + " pings");
    let pingSendPromises = [];
    for (let pingId of pingIds) {
      const id = pingId;
      pingSendPromises.push(
        TelemetryStorage.loadPendingPing(id)
          .then((data) => this._doPing(data, id, true))
          .catch(e => this._log.error("sendPersistedPings - failed to send ping " + id, e)));
    }

    let promise = Promise.all(pingSendPromises);
    this._trackPendingPingTask(promise);
    yield promise;
  }),

  _onPingRequestFinished(success, startTime, id, isPersisted) {
    this._log.trace("_onPingRequestFinished - success: " + success + ", persisted: " + isPersisted);

    let sendId = success ? "TELEMETRY_SEND_SUCCESS" : "TELEMETRY_SEND_FAILURE";
    let hsend = Telemetry.getHistogramById(sendId);
    let hsuccess = Telemetry.getHistogramById("TELEMETRY_SUCCESS");

    hsend.add(monotonicNow() - startTime);
    hsuccess.add(success);

    if (!success) {
      // Let the scheduler know about send failures for triggering backoff timeouts.
      SendScheduler.notifySendsFailed();
    }

    if (success && isPersisted) {
      if (TelemetryStorage.isDeletionPing(id)) {
        return TelemetryStorage.removeDeletionPing();
      }
      return TelemetryStorage.removePendingPing(id);
    }
    return Promise.resolve();
  },

  _getSubmissionPath(ping) {
    // The new ping format contains an "application" section, the old one doesn't.
    let pathComponents;
    if (isV4PingFormat(ping)) {
      // We insert the Ping id in the URL to simplify server handling of duplicated
      // pings.
      let app = ping.application;
      pathComponents = [
        ping.id, ping.type, app.name, app.version, app.channel, app.buildId
      ];
    } else {
      // This is a ping in the old format.
      if (!("slug" in ping)) {
        // That's odd, we don't have a slug. Generate one so that TelemetryStorage.jsm works.
        ping.slug = Utils.generateUUID();
      }

      // Do we have enough info to build a submission URL?
      let payload = ("payload" in ping) ? ping.payload : null;
      if (payload && ("info" in payload)) {
        let info = ping.payload.info;
        pathComponents = [ ping.slug, info.reason, info.appName, info.appVersion,
                           info.appUpdateChannel, info.appBuildID ];
      } else {
        // Only use the UUID as the slug.
        pathComponents = [ ping.slug ];
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

    this._log.trace("_doPing - server: " + this._server + ", persisted: " + isPersisted +
                    ", id: " + id);

    const isNewPing = isV4PingFormat(ping);
    const version = isNewPing ? PING_FORMAT_VERSION : 1;
    const url = this._server + this._getSubmissionPath(ping) + "?v=" + version;

    let request = new ServiceRequest();
    request.mozBackgroundRequest = true;
    request.timeout = PING_SUBMIT_TIMEOUT_MS;

    request.open("POST", url, true);
    request.overrideMimeType("text/plain");
    request.setRequestHeader("Content-Type", "application/json; charset=UTF-8");
    request.setRequestHeader("Date", Policy.now().toUTCString());

    this._pendingPingRequests.set(id, request);

    // Prevent the request channel from running though URLClassifier (bug 1296802)
    request.channel.loadFlags &= ~Ci.nsIChannel.LOAD_CLASSIFY_URI;

    const monotonicStartTime = monotonicNow();
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
      this._onPingRequestFinished(success, monotonicStartTime, id, isPersisted)
        .then(() => onCompletion(),
              (error) => {
                this._log.error("_doPing - request success: " + success + ", error: " + error);
                onCompletion();
              });
    };

    let errorhandler = (event) => {
      this._log.error("_doPing - error making request to " + url + ": " + event.type);
      onRequestFinished(false, event);
    };
    request.onerror = errorhandler;
    request.ontimeout = errorhandler;
    request.onabort = errorhandler;

    request.onload = (event) => {
      let status = request.status;
      let statusClass = status - (status % 100);
      let success = false;

      if (statusClass === 200) {
        // We can treat all 2XX as success.
        this._log.info("_doPing - successfully loaded, status: " + status);
        success = true;
      } else if (statusClass === 400) {
        // 4XX means that something with the request was broken.
        this._log.error("_doPing - error submitting to " + url + ", status: " + status
                        + " - ping request broken?");
        Telemetry.getHistogramById("TELEMETRY_PING_EVICTED_FOR_SERVER_ERRORS").add();
        // TODO: we should handle this better, but for now we should avoid resubmitting
        // broken requests by pretending success.
        success = true;
      } else if (statusClass === 500) {
        // 5XX means there was a server-side error and we should try again later.
        this._log.error("_doPing - error submitting to " + url + ", status: " + status
                        + " - server error, should retry later");
      } else {
        // We received an unexpected status code.
        this._log.error("_doPing - error submitting to " + url + ", status: " + status
                        + ", type: " + event.type);
      }

      onRequestFinished(success, event);
    };

    // If that's a legacy ping format, just send its payload.
    let networkPayload = isNewPing ? ping : ping.payload;
    request.setRequestHeader("Content-Encoding", "gzip");
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    let startTime = new Date();
    let utf8Payload = converter.ConvertFromUnicode(JSON.stringify(networkPayload));
    utf8Payload += converter.Finish();
    Telemetry.getHistogramById("TELEMETRY_STRINGIFY").add(new Date() - startTime);

    // Check the size and drop pings which are too big.
    const pingSizeBytes = utf8Payload.length;
    if (pingSizeBytes > TelemetryStorage.MAXIMUM_PING_SIZE) {
      this._log.error("_doPing - submitted ping exceeds the size limit, size: " + pingSizeBytes);
      Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_SEND").add();
      Telemetry.getHistogramById("TELEMETRY_DISCARDED_SEND_PINGS_SIZE_MB")
               .add(Math.floor(pingSizeBytes / 1024 / 1024));
      // We don't need to call |request.abort()| as it was not sent yet.
      this._pendingPingRequests.delete(id);
      return TelemetryStorage.removePendingPing(id);
    }

    let payloadStream = Cc["@mozilla.org/io/string-input-stream;1"]
                        .createInstance(Ci.nsIStringInputStream);
    startTime = new Date();
    payloadStream.data = gzipCompressString(utf8Payload);
    Telemetry.getHistogramById("TELEMETRY_COMPRESS").add(new Date() - startTime);
    startTime = new Date();
    request.send(payloadStream);

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
   * Check if sending is disabled. If FHR is not allowed to upload,
   * pings are not sent to the server (Telemetry is a sub-feature of FHR). If trying
   * to send a deletion ping, don't block it.
   * If unified telemetry is off, don't send pings if Telemetry is disabled.
   *
   * @param {Object} [ping=null] A ping to be checked.
   * @return {Boolean} True if pings can be send to the servers, false otherwise.
   */
  sendingEnabled(ping = null) {
    // We only send pings from official builds, but allow overriding this for tests.
    if (!Telemetry.isOfficialTelemetry && !this._testMode) {
      return false;
    }

    // With unified Telemetry, the FHR upload setting controls whether we can send pings.
    // The Telemetry pref enables sending extended data sets instead.
    if (IS_UNIFIED_TELEMETRY) {
      // Deletion pings are sent even if the upload is disabled.
      if (ping && isDeletionPing(ping)) {
        return true;
      }
      return Preferences.get(PREF_FHR_UPLOAD_ENABLED, false);
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
    let p = Array.from(this._pendingPingActivity, p => p.catch(ex => {
      this._log.error("promisePendingPingActivity - ping activity had an error", ex);
    }));
    p.push(SendScheduler.waitOnSendTask());
    return Promise.all(p);
  },

  _persistCurrentPings: Task.async(function*() {
    for (let [id, ping] of this._currentPings) {
      try {
        yield savePing(ping);
        this._log.trace("_persistCurrentPings - saved ping " + id);
      } catch (ex) {
        this._log.error("_persistCurrentPings - failed to save ping " + id, ex);
      } finally {
        this._currentPings.delete(id);
      }
    }
  }),

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
};
