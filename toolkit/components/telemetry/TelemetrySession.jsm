/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/debug.js", this);
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/DeferredTask.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);

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

const MS_IN_ONE_HOUR  = 60 * 60 * 1000;
const MIN_SUBSESSION_LENGTH_MS = 10 * 60 * 1000;

// This is the HG changeset of the Histogram.json file, used to associate
// submitted ping data with its histogram definition (bug 832007)
#expand const HISTOGRAMS_FILE_VERSION = "__HISTOGRAMS_FILE_VERSION__";

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetrySession" + (Utils.isContentProcess ? "#content::" : "::");

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_PREVIOUS_BUILDID = PREF_BRANCH + "previousBuildID";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";
const PREF_ASYNC_PLUGIN_INIT = "dom.ipc.plugins.asyncInit.enabled";
const PREF_UNIFIED = PREF_BRANCH + "unified";


const MESSAGE_TELEMETRY_PAYLOAD = "Telemetry:Payload";
const MESSAGE_TELEMETRY_GET_CHILD_PAYLOAD = "Telemetry:GetChildPayload";

const DATAREPORTING_DIRECTORY = "datareporting";
const ABORTED_SESSION_FILE_NAME = "aborted-session-ping";

const SESSION_STATE_FILE_NAME = "session-state.json";

// Whether the FHR/Telemetry unification features are enabled.
// Changing this pref requires a restart.
const IS_UNIFIED_TELEMETRY = Preferences.get(PREF_UNIFIED, false);

// Maximum number of content payloads that we are willing to store.
const MAX_NUM_CONTENT_PAYLOADS = 10;

// Do not gather data more than once a minute
const TELEMETRY_INTERVAL = 60000;
// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY = 60000;
// Delay before initializing telemetry if we're testing (ms)
const TELEMETRY_TEST_DELAY = 100;
// Execute a scheduler tick every 5 minutes.
const SCHEDULER_TICK_INTERVAL_MS = 5 * 60 * 1000;
// When user is idle, execute a scheduler tick every 60 minutes.
const SCHEDULER_TICK_IDLE_INTERVAL_MS = 60 * 60 * 1000;
// The maximum number of times a scheduled operation can fail.
const SCHEDULER_RETRY_ATTEMPTS = 3;

// The tolerance we have when checking if it's midnight (15 minutes).
const SCHEDULER_MIDNIGHT_TOLERANCE_MS = 15 * 60 * 1000;

// Coalesce the daily and aborted-session pings if they are both due within
// two minutes from each other.
const SCHEDULER_COALESCE_THRESHOLD_MS = 2 * 60 * 1000;

// Seconds of idle time before pinging.
// On idle-daily a gather-telemetry notification is fired, during it probes can
// start asynchronous tasks to gather data.
const IDLE_TIMEOUT_SECONDS = 5 * 60;

// The frequency at which we persist session data to the disk to prevent data loss
// in case of aborted sessions (currently 5 minutes).
const ABORTED_SESSION_UPDATE_INTERVAL_MS = 5 * 60 * 1000;

const TOPIC_CYCLE_COLLECTOR_BEGIN = "cycle-collector-begin";

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
XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");
XPCOMUtils.defineLazyServiceGetter(this, "cpml",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageListenerManager");
XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");
XPCOMUtils.defineLazyServiceGetter(this, "ppml",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManagerPrivate",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryController",
                                  "resource://gre/modules/TelemetryController.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryStorage",
                                  "resource://gre/modules/TelemetryStorage.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryLog",
                                  "resource://gre/modules/TelemetryLog.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ThirdPartyCookieProbe",
                                  "resource://gre/modules/ThirdPartyCookieProbe.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry",
                                  "resource://gre/modules/UITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UpdateChannel",
                                  "resource://gre/modules/UpdateChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryEnvironment",
                                  "resource://gre/modules/TelemetryEnvironment.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils",
                                  "resource://services-common/utils.js");

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

function getMsSinceProcessStart() {
  try {
    return Telemetry.msSinceProcessStart();
  } catch (ex) {
    // If this fails return a special value.
    return -1;
  }
}

/**
 * This is a policy object used to override behavior for testing.
 */
let Policy = {
  now: () => new Date(),
  monotonicNow: getMsSinceProcessStart,
  generateSessionUUID: () => generateUUID(),
  generateSubsessionUUID: () => generateUUID(),
  setSchedulerTickTimeout: (callback, delayMs) => setTimeout(callback, delayMs),
  clearSchedulerTickTimeout: id => clearTimeout(id),
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
 * Date.toISOString() gives us UTC times, this gives us local times in the ISO date format.
 * http://www.w3.org/TR/NOTE-datetime
 */
function toLocalTimeISOString(date) {
  function padNumber(number, places) {
    number = number.toString();
    while (number.length < places) {
      number = "0" + number;
    }
    return number;
  }

  let sign = (n) => n >= 0 ? "+" : "-";
  // getTimezoneOffset counter-intuitively returns -60 for UTC+1.
  let tzOffset = - date.getTimezoneOffset();

  // YYYY-MM-DDThh:mm:ss.sTZD (eg 1997-07-16T19:20:30.45+01:00)
  return    padNumber(date.getFullYear(), 4)
    + "-" + padNumber(date.getMonth() + 1, 2)
    + "-" + padNumber(date.getDate(), 2)
    + "T" + padNumber(date.getHours(), 2)
    + ":" + padNumber(date.getMinutes(), 2)
    + ":" + padNumber(date.getSeconds(), 2)
    + "." + date.getMilliseconds()
    + sign(tzOffset) + padNumber(Math.floor(Math.abs(tzOffset / 60)), 2)
    + ":" + padNumber(Math.abs(tzOffset % 60), 2);
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
      Cu.import("resource://gre/modules/ctypes.jsm");
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

/**
 * This object allows the serialisation of asynchronous tasks. This is particularly
 * useful to serialise write access to the disk in order to prevent race conditions
 * to corrupt the data being written.
 * We are using this to synchronize saving to the file that TelemetrySession persists
 * its state in.
 */
function SaveSerializer() {
  this._queuedOperations = [];
  this._queuedInProgress = false;
  this._log = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX);
}

SaveSerializer.prototype = {
  /**
   * Enqueues an operation to a list to serialise their execution in order to prevent race
   * conditions. Useful to serialise access to disk.
   *
   * @param {Function} aFunction The task function to enqueue. It must return a promise.
   * @return {Promise} A promise resolved when the enqueued task completes.
   */
  enqueueTask: function (aFunction) {
    let promise = new Promise((resolve, reject) =>
      this._queuedOperations.push([aFunction, resolve, reject]));

    if (this._queuedOperations.length == 1) {
      this._popAndPerformQueuedOperation();
    }
    return promise;
  },

  /**
   * Make sure to flush all the pending operations.
   * @return {Promise} A promise resolved when all the pending operations have completed.
   */
  flushTasks: function () {
    let dummyTask = () => new Promise(resolve => resolve());
    return this.enqueueTask(dummyTask);
  },

  /**
   * Pop a task from the queue, executes it and continue to the next one.
   * This function recursively pops all the tasks.
   */
  _popAndPerformQueuedOperation: function () {
    if (!this._queuedOperations.length || this._queuedInProgress) {
      return;
    }

    this._log.trace("_popAndPerformQueuedOperation - Performing queued operation.");
    let [func, resolve, reject] = this._queuedOperations.shift();
    let promise;

    try {
      this._queuedInProgress = true;
      promise = func();
    } catch (ex) {
      this._log.warn("_popAndPerformQueuedOperation - Queued operation threw during execution. ",
                     ex);
      this._queuedInProgress = false;
      reject(ex);
      this._popAndPerformQueuedOperation();
      return;
    }

    if (!promise || typeof(promise.then) != "function") {
      let msg = "Queued operation did not return a promise: " + func;
      this._log.warn("_popAndPerformQueuedOperation - " + msg);

      this._queuedInProgress = false;
      reject(new Error(msg));
      this._popAndPerformQueuedOperation();
      return;
    }

    promise.then(result => {
        this._log.trace("_popAndPerformQueuedOperation - Queued operation completed.");
        this._queuedInProgress = false;
        resolve(result);
        this._popAndPerformQueuedOperation();
      },
      error => {
        this._log.warn("_popAndPerformQueuedOperation - Failure when performing queued operation.",
                       error);
        this._queuedInProgress = false;
        reject(error);
        this._popAndPerformQueuedOperation();
      });
  },
};

/**
 * TelemetryScheduler contains a single timer driving all regularly-scheduled
 * Telemetry related jobs. Having a single place with this logic simplifies
 * reasoning about scheduling actions in a single place, making it easier to
 * coordinate jobs and coalesce them.
 */
let TelemetryScheduler = {
  _lastDailyPingTime: 0,
  _lastSessionCheckpointTime: 0,

  // For sanity checking.
  _lastAdhocPingTime: 0,
  _lastTickTime: 0,

  _log: null,

  // The number of times a daily ping fails.
  _dailyPingRetryAttempts: 0,

  // The timer which drives the scheduler.
  _schedulerTimer: null,
  // The interval used by the scheduler timer.
  _schedulerInterval: 0,
  _shuttingDown: true,
  _isUserIdle: false,

  /**
   * Initialises the scheduler and schedules the first daily/aborted session pings.
   */
  init: function() {
    this._log = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, "TelemetryScheduler::");
    this._log.trace("init");
    this._shuttingDown = false;
    this._isUserIdle = false;
    // Initialize the last daily ping and aborted session last due times to the current time.
    // Otherwise, we might end up sending daily pings even if the subsession is not long enough.
    let now = Policy.now();
    this._lastDailyPingTime = now.getTime();
    this._lastSessionCheckpointTime = now.getTime();
    this._rescheduleTimeout();
    idleService.addIdleObserver(this, IDLE_TIMEOUT_SECONDS);
  },

  /**
   * Reschedules the tick timer.
   */
  _rescheduleTimeout: function() {
    this._log.trace("_rescheduleTimeout - isUserIdle: " + this._isUserIdle);
    if (this._shuttingDown) {
      this._log.warn("_rescheduleTimeout - already shutdown");
      return;
    }

    if (this._schedulerTimer) {
      Policy.clearSchedulerTickTimeout(this._schedulerTimer);
    }

    const now = Policy.now();
    let timeout = SCHEDULER_TICK_INTERVAL_MS;

    // When the user is idle we want to fire the timer less often.
    if (this._isUserIdle) {
      timeout = SCHEDULER_TICK_IDLE_INTERVAL_MS;
      // We need to make sure though that we don't miss sending pings around
      // midnight when we use the longer idle intervals.
      const nextMidnight = Utils.getNextMidnight(now);
      timeout = Math.min(timeout, nextMidnight.getTime() - now.getTime());
    }

    this._log.trace("_rescheduleTimeout - scheduling next tick for " + new Date(now.getTime() + timeout));
    this._schedulerTimer =
      Policy.setSchedulerTickTimeout(() => this._onSchedulerTick(), timeout);
  },

  _sentDailyPingToday: function(nowDate) {
    // This is today's date and also the previous midnight (0:00).
    const todayDate = Utils.truncateToDays(nowDate);
    // We consider a ping sent for today if it occured after or at 00:00 today.
    return (this._lastDailyPingTime >= todayDate.getTime());
  },

  /**
   * Checks if we can send a daily ping or not.
   * @param {Object} nowDate A date object.
   * @return {Boolean} True if we can send the daily ping, false otherwise.
   */
  _isDailyPingDue: function(nowDate) {
    // The daily ping is not due if we already sent one today.
    if (this._sentDailyPingToday(nowDate)) {
      this._log.trace("_isDailyPingDue - already sent one today");
      return false;
    }

    // Avoid overly short sessions.
    const timeSinceLastDaily = nowDate.getTime() - this._lastDailyPingTime;
    if (timeSinceLastDaily < MIN_SUBSESSION_LENGTH_MS) {
      this._log.trace("_isDailyPingDue - delaying daily to keep minimum session length");
      return false;
    }

    this._log.trace("_isDailyPingDue - is due");
    return true;
  },

  /**
   * An helper function to save an aborted-session ping.
   * @param {Number} now The current time, in milliseconds.
   * @param {Object} [competingPayload=null] If we are coalescing the daily and the
   *                 aborted-session pings, this is the payload for the former. Note
   *                 that the reason field of this payload will be changed.
   * @return {Promise} A promise resolved when the ping is saved.
   */
  _saveAbortedPing: function(now, competingPayload=null) {
    this._lastSessionCheckpointTime = now;
    return Impl._saveAbortedSessionPing(competingPayload)
                .catch(e => this._log.error("_saveAbortedPing - Failed", e));
  },

  /**
   * The notifications handler.
   */
  observe: function(aSubject, aTopic, aData) {
    this._log.trace("observe - aTopic: " + aTopic);
    switch(aTopic) {
      case "idle":
        // If the user is idle, increase the tick interval.
        this._isUserIdle = true;
        return this._onSchedulerTick();
        break;
      case "active":
        // User is back to work, restore the original tick interval.
        this._isUserIdle = false;
        return this._onSchedulerTick();
        break;
    }
  },

  /**
   * Performs a scheduler tick. This function manages Telemetry recurring operations.
   * @return {Promise} A promise, only used when testing, resolved when the scheduled
   *                   operation completes.
   */
  _onSchedulerTick: function() {
    if (this._shuttingDown) {
      this._log.warn("_onSchedulerTick - already shutdown.");
      return Promise.reject(new Error("Already shutdown."));
    }

    let promise = Promise.resolve();
    try {
      promise = this._schedulerTickLogic();
    } catch (e) {
      this._log.error("_onSchedulerTick - There was an exception", e);
    } finally {
      this._rescheduleTimeout();
    }

    // This promise is returned to make testing easier.
    return promise;
  },

  /**
   * Implements the scheduler logic.
   * @return {Promise} Resolved when the scheduled task completes. Only used in tests.
   */
  _schedulerTickLogic: function() {
    this._log.trace("_schedulerTickLogic");

    let nowDate = Policy.now();
    let now = nowDate.getTime();

    if (now - this._lastTickTime > 1.1 * SCHEDULER_TICK_INTERVAL_MS) {
      this._log.trace("_schedulerTickLogic - First scheduler tick after sleep or startup.");
    }
    this._lastTickTime = now;

    // Check if the daily ping is due.
    const shouldSendDaily = this._isDailyPingDue(nowDate);

    if (shouldSendDaily) {
      this._log.trace("_schedulerTickLogic - Daily ping due.");
      return Impl._sendDailyPing().then(() => this._dailyPingSucceeded(now),
                                        () => this._dailyPingFailed(now));
    }

    // Check if the aborted-session ping is due. If a daily ping was saved above, it was
    // already duplicated as an aborted-session ping.
    const isAbortedPingDue =
      (now - this._lastSessionCheckpointTime) >= ABORTED_SESSION_UPDATE_INTERVAL_MS;
    if (isAbortedPingDue) {
      this._log.trace("_schedulerTickLogic - Aborted session ping due.");
      return this._saveAbortedPing(now);
    }

    // No ping is due.
    this._log.trace("_schedulerTickLogic - No ping due.");
    // It's possible, because of sleeps, that we're no longer within midnight tolerance for
    // daily pings. Because of that, daily retry attempts would not be 0 on the next midnight.
    // Reset that count on do-nothing ticks.
    this._dailyPingRetryAttempts = 0;
    return Promise.resolve();
  },

  /**
   * Update the scheduled pings if some other ping was sent.
   * @param {String} reason The reason of the ping that was sent.
   * @param {Object} [competingPayload=null] The payload of the ping that was sent. The
   *                 reason of this payload will be changed.
   */
  reschedulePings: function(reason, competingPayload = null) {
    if (this._shuttingDown) {
      this._log.error("reschedulePings - already shutdown");
      return;
    }

    this._log.trace("reschedulePings - reason: " + reason);
    let now = Policy.now();
    this._lastAdhocPingTime = now.getTime();
    if (reason == REASON_ENVIRONMENT_CHANGE) {
      // We just generated an environment-changed ping, save it as an aborted session and
      // update the schedules.
      this._saveAbortedPing(now.getTime(), competingPayload);
      // If we're close to midnight, skip today's daily ping and reschedule it for tomorrow.
      let nearestMidnight = Utils.getNearestMidnight(now, SCHEDULER_MIDNIGHT_TOLERANCE_MS);
      if (nearestMidnight) {
        this._lastDailyPingTime = now.getTime();
      }
    }

    this._rescheduleTimeout();
  },

  /**
   * Called when a scheduled operation successfully completes (ping sent or saved).
   * @param {Number} now The current time, in milliseconds.
   */
  _dailyPingSucceeded: function(now) {
    this._log.trace("_dailyPingSucceeded");
    this._lastDailyPingTime = now;
    this._dailyPingRetryAttempts = 0;
  },

  /**
   * Called when a scheduled operation fails (ping sent or saved).
   * @param {Number} now The current time, in milliseconds.
   */
  _dailyPingFailed: function(now) {
    this._log.error("_dailyPingFailed");
    this._dailyPingRetryAttempts++;

    // If we reach the maximum number of retry attempts for a daily ping, log the error
    // and skip this daily ping.
    if (this._dailyPingRetryAttempts >= SCHEDULER_RETRY_ATTEMPTS) {
      this._log.error("_pingFailed - The daily ping failed too many times. Skipping it.");
      this._dailyPingRetryAttempts = 0;
      this._lastDailyPingTime = now;
    }
  },

  /**
   * Stops the scheduler.
   */
  shutdown: function() {
    if (this._shuttingDown) {
      if (this._log) {
        this._log.error("shutdown - Already shut down");
      } else {
        Cu.reportError("TelemetryScheduler.shutdown - Already shut down");
      }
      return;
    }

    this._log.trace("shutdown");
    if (this._schedulerTimer) {
      Policy.clearSchedulerTickTimeout(this._schedulerTimer);
      this._schedulerTimer = null;
    }

    idleService.removeIdleObserver(this, IDLE_TIMEOUT_SECONDS);

    this._shuttingDown = true;
  }
};

this.EXPORTED_SYMBOLS = ["TelemetrySession"];

this.TelemetrySession = Object.freeze({
  Constants: Object.freeze({
    PREF_PREVIOUS_BUILDID: PREF_PREVIOUS_BUILDID,
  }),
  /**
   * Send a ping to a test server. Used only for testing.
   */
  testPing: function() {
    return Impl.testPing();
  },
  /**
   * Returns the current telemetry payload.
   * @param reason Optional, the reason to trigger the payload.
   * @param clearSubsession Optional, whether to clear subsession specific data.
   * @returns Object
   */
  getPayload: function(reason, clearSubsession = false) {
    return Impl.getPayload(reason, clearSubsession);
  },
  /**
   * Asks the content processes to send their payloads.
   * @returns Object
   */
  requestChildPayloads: function() {
    return Impl.requestChildPayloads();
  },
  /**
   * Save the session state to a pending file.
   * Used only for testing purposes.
   */
  testSavePendingPing: function() {
    return Impl.testSavePendingPing();
  },
  /**
   * Collect and store information about startup.
   */
  gatherStartup: function() {
    return Impl.gatherStartup();
  },
  /**
   * Inform the ping which AddOns are installed.
   *
   * @param aAddOns - The AddOns.
   */
  setAddOns: function(aAddOns) {
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
  getMetadata: function(reason) {
    return Impl.getMetadata(reason);
  },
  /**
   * Used only for testing purposes.
   */
  reset: function() {
    Impl._sessionId = null;
    Impl._subsessionId = null;
    Impl._previousSessionId = null;
    Impl._previousSubsessionId = null;
    Impl._subsessionCounter = 0;
    Impl._profileSubsessionCounter = 0;
    Impl._subsessionStartActiveTicks = 0;
    Impl._subsessionStartTimeMonotonic = 0;
    this.uninstall();
    return this.setup();
  },
  /**
   * Used only for testing purposes.
   * @param {Boolean} [aForceSavePending=true] If true, always saves the ping whether Telemetry
   *        can send pings or not, which is used for testing.
   */
  shutdown: function(aForceSavePending = true) {
    return Impl.shutdownChromeProcess(aForceSavePending);
  },
  /**
   * Used only for testing purposes.
   */
  setup: function() {
    return Impl.setupChromeProcess(true);
  },
  /**
   * Used only for testing purposes.
   */
  setupContent: function() {
    return Impl.setupContentProcess(true);
  },
  /**
   * Used only for testing purposes.
   */
  uninstall: function() {
    try {
      Impl.uninstall();
    } catch (ex) {
      // Ignore errors
    }
  },
  /**
   * Send a notification.
   */
  observe: function (aSubject, aTopic, aData) {
    return Impl.observe(aSubject, aTopic, aData);
  },
  /**
   * The client id send with the telemetry ping.
   *
   * @return The client id as string, or null.
   */
   get clientID() {
    return Impl.clientID;
   },
});

let Impl = {
  _histograms: {},
  _initialized: false,
  _logger: null,
  _prevValues: {},
  // Regex that matches histograms we care about during startup.
  // Keep this in sync with gen-histogram-bucket-ranges.py.
  _startupHistogramRegex: /SQLITE|HTTP|SPDY|CACHE|DNS/,
  _slowSQLStartup: {},
  _hasWindowRestoredObserver: false,
  _hasXulWindowVisibleObserver: false,
  _startupIO : {},
  // The previous build ID, if this is the first run with a new build.
  // Null if this is the first run, or the previous build ID is unknown.
  _previousBuildId: null,
  // Telemetry payloads sent by child processes.
  // Each element is in the format {source: <weak-ref>, payload: <object>},
  // where source is a weak reference to the child process,
  // and payload is the telemetry payload from that child process.
  _childTelemetry: [],
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
  // A task performing delayed initialization of the chrome process
  _delayedInitTask: null,
  // The deferred promise resolved when the initialization task completes.
  _delayedInitTaskDeferred: null,
  // Used to serialize session state writes to disk.
  _stateSaveSerializer: new SaveSerializer(),

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
    this._log.trace("getSimpleMeasurements");

    let si = Services.startup.getStartupInfo();

    // Measurements common to chrome and content processes.
    let elapsedTime = Date.now() - si.process;
    var ret = {
      totalTime: Math.round(elapsedTime / 1000), // totalTime, in seconds
      uptime: Math.round(elapsedTime / 60000) // uptime in minutes
    }

    // Look for app-specific timestamps
    var appTimestamps = {};
    try {
      let o = {};
      Cu.import("resource://gre/modules/TelemetryTimestamps.jsm", o);
      appTimestamps = o.TelemetryTimestamps.get();
    } catch (ex) {}

    // Only submit this if the extended set is enabled.
    if (!Utils.isContentProcess && Telemetry.canRecordExtended) {
      try {
        ret.addonManager = AddonManagerPrivate.getSimpleMeasures();
        ret.UITelemetry = UITelemetry.getSimpleMeasures();
      } catch (ex) {}
    }

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

    ret.startupInterrupted = Number(Services.startup.interrupted);

    ret.js = Cu.getJSEngineTelemetryValue();

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

    let hasPingBeenSent = false;
    try {
      hasPingBeenSent = Telemetry.getHistogramById("TELEMETRY_SUCCESS").snapshot().sum > 0;
    } catch(e) {
    }

    ret.savedPings = TelemetryStorage.pendingPingCount;

    ret.activeTicks = -1;
    let sr = TelemetryController.getSessionRecorder();
    if (sr) {
      let activeTicks = sr.activeTicks;
      if (isSubsession) {
        activeTicks = sr.activeTicks - this._subsessionStartActiveTicks;
      }

      if (clearSubsession) {
        this._subsessionStartActiveTicks = activeTicks;
      }

      ret.activeTicks = activeTicks;
    }

    ret.pingsOverdue = TelemetrySend.overduePingsCount;

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

  /**
   * Get the type of the dataset that needs to be collected, based on the preferences.
   * @return {Integer} A value from nsITelemetry.DATASET_*.
   */
  getDatasetType: function() {
    return Telemetry.canRecordExtended ? Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN
                                       : Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTOUT;
  },

  getHistograms: function getHistograms(subsession, clearSubsession) {
    this._log.trace("getHistograms - subsession: " + subsession + ", clearSubsession: " + clearSubsession);

    let registered =
      Telemetry.registeredHistograms(this.getDatasetType(), []);
    let hls = subsession ? Telemetry.snapshotSubsessionHistograms(clearSubsession)
                         : Telemetry.histogramSnapshots;
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
    this._log.trace("getAddonHistograms");

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

  getKeyedHistograms: function(subsession, clearSubsession) {
    this._log.trace("getKeyedHistograms - subsession: " + subsession + ", clearSubsession: " + clearSubsession);

    let registered =
      Telemetry.registeredKeyedHistograms(this.getDatasetType(), []);
    let ret = {};

    for (let id of registered) {
      ret[id] = {};
      let keyed = Telemetry.getKeyedHistogramById(id);
      let snapshot = null;
      if (subsession) {
        snapshot = clearSubsession ? keyed.snapshotSubsessionAndClear()
                                   : keyed.subsessionSnapshot();
      } else {
        snapshot = keyed.snapshot();
      }
      for (let key of Object.keys(snapshot)) {
        ret[id][key] = this.packHistogram(snapshot[key]);
      }
    }

    return ret;
  },

  getThreadHangStats: function getThreadHangStats(stats) {
    this._log.trace("getThreadHangStats");

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
    this._log.trace("getMetadata - Reason " + reason);

    const sessionStartDate = toLocalTimeISOString(Utils.truncateToDays(this._sessionStartDate));
    const subsessionStartDate = toLocalTimeISOString(Utils.truncateToDays(this._subsessionStartDate));
    const monotonicNow = Policy.monotonicNow();

    let ret = {
      reason: reason,
      revision: HISTOGRAMS_FILE_VERSION,
      asyncPluginInit: Preferences.get(PREF_ASYNC_PLUGIN_INIT, false),

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

      sessionStartDate: sessionStartDate,
      subsessionStartDate: subsessionStartDate,

      // Compute the session and subsession length in seconds.
      // We use monotonic clocks as Date() is affected by jumping clocks (leading
      // to negative lengths and other issues).
      sessionLength: Math.floor(monotonicNow / 1000),
      subsessionLength:
        Math.floor((monotonicNow - this._subsessionStartTimeMonotonic) / 1000),
    };

    // TODO: Remove this when bug 1124128 lands.
    if (this._addons)
      ret.addons = this._addons;

    // TODO: Remove this when bug 1124128 lands.
    let flashVersion = this.getFlashVersion();
    if (flashVersion)
      ret.flashVersion = flashVersion;

    return ret;
  },

  /**
   * Pull values from about:memory into corresponding histograms
   */
  gatherMemory: function gatherMemory() {
    if (!Telemetry.canRecordExtended) {
      this._log.trace("gatherMemory - Extended data recording disabled, skipping.");
      return;
    }

    this._log.trace("gatherMemory");

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
    // test_TelemetryController.js relies on some of these histograms being
    // here.  If you remove any of the following histograms from here, you'll
    // have to modify test_TelemetryController.js:
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

  getChildPayloads: function getChildPayloads() {
    return [for (child of this._childTelemetry) child.payload];
  },

  /**
   * Make a copy of interesting histograms at startup.
   */
  gatherStartupHistograms: function gatherStartupHistograms() {
    this._log.trace("gatherStartupHistograms");

    let info =
      Telemetry.registeredHistograms(this.getDatasetType(), []);
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
  assemblePayloadWithMeasurements: function(simpleMeasurements, info, reason, clearSubsession) {
    const isSubsession = IS_UNIFIED_TELEMETRY && !this._isClassicReason(reason);
    clearSubsession = IS_UNIFIED_TELEMETRY && clearSubsession;
    this._log.trace("assemblePayloadWithMeasurements - reason: " + reason +
                    ", submitting subsession data: " + isSubsession);

    // Payload common to chrome and content processes.
    let payloadObj = {
      ver: PAYLOAD_VERSION,
      simpleMeasurements: simpleMeasurements,
      histograms: this.getHistograms(isSubsession, clearSubsession),
      keyedHistograms: this.getKeyedHistograms(isSubsession, clearSubsession),
    };

    // Add extended set measurements common to chrome & content processes
    if (Telemetry.canRecordExtended) {
      payloadObj.chromeHangs = Telemetry.chromeHangs;
      payloadObj.threadHangStats = this.getThreadHangStats(Telemetry.threadHangStats);
      payloadObj.log = TelemetryLog.entries();
    }

    if (Utils.isContentProcess) {
      return payloadObj;
    }

    // Additional payload for chrome process.
    payloadObj.info = info;

    // Add extended set measurements for chrome process.
    if (Telemetry.canRecordExtended) {
      payloadObj.slowSQL = Telemetry.slowSQL;
      payloadObj.fileIOReports = Telemetry.fileIOReports;
      payloadObj.lateWrites = Telemetry.lateWrites;

      // Add the addon histograms if they are present
      let addonHistograms = this.getAddonHistograms();
      if (Object.keys(addonHistograms).length > 0) {
        payloadObj.addonHistograms = addonHistograms;
      }

      payloadObj.addonDetails = AddonManagerPrivate.getTelemetryDetails();
      payloadObj.UIMeasurements = UITelemetry.getUIMeasurements();

      if (Object.keys(this._slowSQLStartup).length != 0 &&
          (Object.keys(this._slowSQLStartup.mainThread).length ||
           Object.keys(this._slowSQLStartup.otherThreads).length)) {
        payloadObj.slowSQLStartup = this._slowSQLStartup;
      }
    }

    if (this._childTelemetry.length) {
      payloadObj.childPayloads = this.getChildPayloads();
    }

    return payloadObj;
  },

  /**
   * Start a new subsession.
   */
  startNewSubsession: function () {
    this._subsessionStartDate = Policy.now();
    this._subsessionStartTimeMonotonic = Policy.monotonicNow();
    this._previousSubsessionId = this._subsessionId;
    this._subsessionId = Policy.generateSubsessionUUID();
    this._subsessionCounter++;
    this._profileSubsessionCounter++;
  },

  getSessionPayload: function getSessionPayload(reason, clearSubsession) {
    this._log.trace("getSessionPayload - reason: " + reason + ", clearSubsession: " + clearSubsession);
#if defined(MOZ_WIDGET_GONK) || defined(MOZ_WIDGET_ANDROID)
    clearSubsession = false;
    const isSubsession = false;
#else
    const isSubsession = !this._isClassicReason(reason);
#endif

    let measurements =
      this.getSimpleMeasurements(reason == REASON_SAVED_SESSION, isSubsession, clearSubsession);
    let info = !Utils.isContentProcess ? this.getMetadata(reason) : null;
    let payload = this.assemblePayloadWithMeasurements(measurements, info, reason, clearSubsession);

    if (!Utils.isContentProcess && clearSubsession) {
      this.startNewSubsession();
      // Persist session data to disk (don't wait until it completes).
      let sessionData = this._getSessionDataObject();
      this._stateSaveSerializer.enqueueTask(() => this._saveSessionData(sessionData));
    }

    return payload;
  },

  /**
   * Send data to the server. Record success/send-time in histograms
   */
  send: function send(reason) {
    this._log.trace("send - Reason " + reason);
    // populate histograms one last time
    this.gatherMemory();

    const isSubsession = !this._isClassicReason(reason);
    let payload = this.getSessionPayload(reason, isSubsession);
    let options = {
      addClientId: true,
      addEnvironment: true,
    };
    return TelemetryController.submitExternalPing(getPingType(payload), payload, options);
  },

  attachObservers: function attachObservers() {
    if (!this._initialized)
      return;
    Services.obs.addObserver(this, "idle-daily", false);
    if (Telemetry.canRecordExtended) {
      Services.obs.addObserver(this, TOPIC_CYCLE_COLLECTOR_BEGIN, false);
    }
  },

  detachObservers: function detachObservers() {
    if (!this._initialized)
      return;
    Services.obs.removeObserver(this, "idle-daily");
    try {
      // Tests may flip Telemetry.canRecordExtended on and off. Just try to remove this
      // observer and catch if it fails because the observer was not added.
      Services.obs.removeObserver(this, TOPIC_CYCLE_COLLECTOR_BEGIN);
    } catch (e) {
      this._log.warn("detachObservers - Failed to remove " + TOPIC_CYCLE_COLLECTOR_BEGIN, e);
    }
  },

  /**
   * Initializes telemetry within a timer.
   */
  setupChromeProcess: function setupChromeProcess(testing) {
    this._initStarted = true;
    this._log.trace("setupChromeProcess");

    if (this._delayedInitTask) {
      this._log.error("setupChromeProcess - init task already running");
      return this._delayedInitTaskDeferred.promise;
    }

    if (this._initialized && !testing) {
      this._log.error("setupChromeProcess - already initialized");
      return Promise.resolve();
    }

    if (!Telemetry.canRecordBase && !testing) {
      this._log.config("setupChromeProcess - Telemetry recording is disabled, skipping Chrome process setup.");
      return Promise.resolve();
    }

    // Generate a unique id once per session so the server can cope with duplicate
    // submissions, orphaning and other oddities. The id is shared across subsessions.
    this._sessionId = Policy.generateSessionUUID();
    this.startNewSubsession();
    // startNewSubsession sets |_subsessionStartDate| to the current date/time. Use
    // the very same value for |_sessionStartDate|.
    this._sessionStartDate = this._subsessionStartDate;

    // Initialize some probes that are kept in their own modules
    this._thirdPartyCookies = new ThirdPartyCookieProbe();
    this._thirdPartyCookies.init();

    // Record old value and update build ID preference if this is the first
    // run with a new build ID.
    let previousBuildId = Preferences.get(PREF_PREVIOUS_BUILDID, null);
    let thisBuildID = Services.appinfo.appBuildID;
    // If there is no previousBuildId preference, we send null to the server.
    if (previousBuildId != thisBuildID) {
      this._previousBuildId = previousBuildId;
      Preferences.set(PREF_PREVIOUS_BUILDID, thisBuildID);
    }

    TelemetryController.shutdown.addBlocker("TelemetrySession: shutting down",
                                      () => this.shutdownChromeProcess(),
                                      () => this._getState());

    Services.obs.addObserver(this, "sessionstore-windows-restored", false);
#ifdef MOZ_WIDGET_ANDROID
    Services.obs.addObserver(this, "application-background", false);
#endif
    Services.obs.addObserver(this, "xul-window-visible", false);
    this._hasWindowRestoredObserver = true;
    this._hasXulWindowVisibleObserver = true;

    ppml.addMessageListener(MESSAGE_TELEMETRY_PAYLOAD, this);

    // Delay full telemetry initialization to give the browser time to
    // run various late initializers. Otherwise our gathered memory
    // footprint and other numbers would be too optimistic.
    this._delayedInitTaskDeferred = Promise.defer();
    this._delayedInitTask = new DeferredTask(function* () {
      try {
        this._initialized = true;

        yield this._loadSessionData();
        // Update the session data to keep track of new subsessions created before
        // the initialization.
        yield this._saveSessionData(this._getSessionDataObject());

        this.attachObservers();
        this.gatherMemory();

        Telemetry.asyncFetchTelemetryData(function () {});

        if (IS_UNIFIED_TELEMETRY) {
          // Check for a previously written aborted session ping.
          yield TelemetryController.checkAbortedSessionPing();

          // Write the first aborted-session ping as early as possible. Just do that
          // if we are not testing, since calling Telemetry.reset() will make a previous
          // aborted ping a pending ping.
          if (!testing) {
            yield this._saveAbortedSessionPing();
          }

          TelemetryEnvironment.registerChangeListener(ENVIRONMENT_CHANGE_LISTENER,
                                 (reason, data) => this._onEnvironmentChange(reason, data));

          // Start the scheduler.
          // We skip this if unified telemetry is off, so we don't
          // trigger the new unified ping types.
          TelemetryScheduler.init();
        }

        this._delayedInitTaskDeferred.resolve();
      } catch (e) {
        this._delayedInitTaskDeferred.reject(e);
      } finally {
        this._delayedInitTask = null;
        this._delayedInitTaskDeferred = null;
      }
    }.bind(this), testing ? TELEMETRY_TEST_DELAY : TELEMETRY_DELAY);

    this._delayedInitTask.arm();
    return this._delayedInitTaskDeferred.promise;
  },

  /**
   * Initializes telemetry for a content process.
   */
  setupContentProcess: function setupContentProcess(testing) {
    this._log.trace("setupContentProcess");

    if (!Telemetry.canRecordBase) {
      this._log.trace("setupContentProcess - base recording is disabled, not initializing");
      return;
    }

    Services.obs.addObserver(this, "content-child-shutdown", false);
    cpml.addMessageListener(MESSAGE_TELEMETRY_GET_CHILD_PAYLOAD, this);

    this.gatherStartupHistograms();

    let delayedTask = new DeferredTask(function* () {
      this._initialized = true;

      this.attachObservers();
      this.gatherMemory();
    }.bind(this), testing ? TELEMETRY_TEST_DELAY : TELEMETRY_DELAY);

    delayedTask.arm();
  },

  getFlashVersion: function getFlashVersion() {
    this._log.trace("getFlashVersion");
    let host = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let tags = host.getPluginTags();

    for (let i = 0; i < tags.length; i++) {
      if (tags[i].name == "Shockwave Flash")
        return tags[i].version;
    }

    return null;
  },

  receiveMessage: function receiveMessage(message) {
    this._log.trace("receiveMessage - Message name " + message.name);
    switch (message.name) {
    case MESSAGE_TELEMETRY_PAYLOAD:
    {
      let source = message.data.childUUID;
      delete message.data.childUUID;

      for (let child of this._childTelemetry) {
        if (child.source === source) {
          // Update existing telemetry data.
          child.payload = message.data;
          return;
        }
      }
      // Did not find existing child in this._childTelemetry.
      this._childTelemetry.push({
        source: source,
        payload: message.data,
      });

      if (this._childTelemetry.length == MAX_NUM_CONTENT_PAYLOADS + 1) {
        this._childTelemetry.shift();
        Telemetry.getHistogramById("TELEMETRY_DISCARDED_CONTENT_PINGS_COUNT").add();
      }

      break;
    }
    case MESSAGE_TELEMETRY_GET_CHILD_PAYLOAD:
    {
      this.sendContentProcessPing("saved-session");
      break;
    }
    default:
      throw new Error("Telemetry.receiveMessage: bad message name");
    }
  },

  _processUUID: generateUUID(),

  sendContentProcessPing: function sendContentProcessPing(reason) {
    this._log.trace("sendContentProcessPing - Reason " + reason);
    const isSubsession = !this._isClassicReason(reason);
    let payload = this.getSessionPayload(reason, isSubsession);
    payload.childUUID = this._processUUID;
    cpmm.sendAsyncMessage(MESSAGE_TELEMETRY_PAYLOAD, payload);
  },

   /**
    * Save both the "saved-session" and the "shutdown" pings to disk.
    */
  saveShutdownPings: function() {
    this._log.trace("saveShutdownPings");

    // We don't wait for "shutdown" pings to be written to disk before gathering the
    // "saved-session" payload. Instead we append the promises to this list and wait
    // on both to be saved after kicking off their collection.
    let p = [];

    if (IS_UNIFIED_TELEMETRY) {
      let shutdownPayload = this.getSessionPayload(REASON_SHUTDOWN, false);

      let options = {
        addClientId: true,
        addEnvironment: true,
        overwrite: true,
      };
      p.push(TelemetryController.addPendingPing(getPingType(shutdownPayload), shutdownPayload, options)
                                .catch(e => this._log.error("saveShutdownPings - failed to submit shutdown ping", e)));
     }

    // As a temporary measure, we want to submit saved-session too if extended Telemetry is enabled
    // to keep existing performance analysis working.
    if (Telemetry.canRecordExtended) {
      let payload = this.getSessionPayload(REASON_SAVED_SESSION, false);

      let options = {
        addClientId: true,
        addEnvironment: true,
      };
      p.push(TelemetryController.addPendingPing(getPingType(payload), payload, options)
                                .catch (e => this._log.error("saveShutdownPings - failed to submit saved-session ping", e)));
    }

    // Wait on pings to be saved.
    return Promise.all(p);
  },


  testSavePendingPing: function testSaveHistograms() {
    this._log.trace("testSaveHistograms");
    let payload = this.getSessionPayload(REASON_SAVED_SESSION, false);
    let options = {
      addClientId: true,
      addEnvironment: true,
      overwrite: true,
    };
    return TelemetryController.addPendingPing(getPingType(payload), payload, options);
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
#ifdef MOZ_WIDGET_ANDROID
    Services.obs.removeObserver(this, "application-background", false);
#endif
  },

  getPayload: function getPayload(reason, clearSubsession) {
    this._log.trace("getPayload - clearSubsession: " + clearSubsession);
    reason = reason || REASON_GATHER_PAYLOAD;
    // This function returns the current Telemetry payload to the caller.
    // We only gather startup info once.
    if (Object.keys(this._slowSQLStartup).length == 0) {
      this.gatherStartupHistograms();
      this._slowSQLStartup = Telemetry.slowSQL;
    }
    this.gatherMemory();
    return this.getSessionPayload(reason, clearSubsession);
  },

  requestChildPayloads: function() {
    this._log.trace("requestChildPayloads");
    ppmm.broadcastAsyncMessage(MESSAGE_TELEMETRY_GET_CHILD_PAYLOAD, {});
  },

  gatherStartup: function gatherStartup() {
    this._log.trace("gatherStartup");
    let counters = processInfo.getCounters();
    if (counters) {
      [this._startupIO.startupSessionRestoreReadBytes,
        this._startupIO.startupSessionRestoreWriteBytes] = counters;
    }
    this.gatherStartupHistograms();
    this._slowSQLStartup = Telemetry.slowSQL;
  },

  setAddOns: function setAddOns(aAddOns) {
    this._addons = aAddOns;
  },

  testPing: function testPing() {
    return this.send(REASON_TEST_PING);
  },

  /**
   * This observer drives telemetry.
   */
  observe: function (aSubject, aTopic, aData) {
    // Prevent the cycle collector begin topic from cluttering the log.
    if (aTopic != TOPIC_CYCLE_COLLECTOR_BEGIN) {
      this._log.trace("observe - " + aTopic + " notified.");
    }

    switch (aTopic) {
    case "profile-after-change":
      // profile-after-change is only registered for chrome processes.
      return this.setupChromeProcess();
    case "app-startup":
      // app-startup is only registered for content processes.
      return this.setupContentProcess();
    case "content-child-shutdown":
      // content-child-shutdown is only registered for content processes.
      Services.obs.removeObserver(this, "content-child-shutdown");
      this.uninstall();

      if (Telemetry.isOfficialTelemetry) {
        this.sendContentProcessPing(REASON_SAVED_SESSION);
      }
      break;
    case TOPIC_CYCLE_COLLECTOR_BEGIN:
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
        // Notify that data should be gathered now.
        // TODO: We are keeping this behaviour for now but it will be removed as soon as
        // bug 1127907 lands.
        Services.obs.notifyObservers(null, "gather-telemetry", null);
      }).bind(this), Ci.nsIThread.DISPATCH_NORMAL);
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
      if (Telemetry.isOfficialTelemetry) {
        let payload = this.getSessionPayload(REASON_SAVED_SESSION, false);
        let options = {
          addClientId: true,
          addEnvironment: true,
          overwrite: true,
        };
        TelemetryController.addPendingPing(getPingType(payload), payload, options);
      }
      break;
#endif
    }
  },

  /**
   * This tells TelemetrySession to uninitialize and save any pending pings.
   * @param testing Optional. If true, always saves the ping whether Telemetry
   *                can send pings or not, which is used for testing.
   */
  shutdownChromeProcess: function(testing = false) {
    this._log.trace("shutdownChromeProcess - testing: " + testing);

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

      if (Telemetry.isOfficialTelemetry || testing) {
        return Task.spawn(function*() {
          yield this.saveShutdownPings();
          yield this._stateSaveSerializer.flushTasks();

          if (IS_UNIFIED_TELEMETRY) {
            yield TelemetryController.removeAbortedSessionPing();
          }

          reset();
        }.bind(this));
      }

      reset();
      return Promise.resolve();
    };

    // We can be in one the following states here:
    // 1) setupChromeProcess was never called
    // or it was called and
    //   2) _delayedInitTask was scheduled, but didn't run yet.
    //   3) _delayedInitTask is running now.
    //   4) _delayedInitTask finished running already.

    // This handles 1).
    if (!this._initStarted) {
      return Promise.resolve();
     }

    // This handles 4).
    if (!this._delayedInitTask) {
      // We already ran the delayed initialization.
      return cleanup();
     }

    // This handles 2) and 3).
    return this._delayedInitTask.finalize().then(cleanup);
   },

  /**
   * Gather and send a daily ping.
   * @return {Promise} Resolved when the ping is sent.
   */
  _sendDailyPing: function() {
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
      let abortedPromise = this._saveAbortedSessionPing(payload);
      promise = promise.then(() => abortedPromise);
    }

    return promise;
  },

  /**
   * Loads session data from the session data file.
   * @return {Promise<boolean>} A promise which is resolved with a true argument when
   *                            loading has completed, with false otherwise.
   */
  _loadSessionData: Task.async(function* () {
    const dataFile = OS.Path.join(OS.Constants.Path.profileDir, DATAREPORTING_DIRECTORY,
                                  SESSION_STATE_FILE_NAME);

    let content;
    try {
      content = yield OS.File.read(dataFile, { encoding: "utf-8" });
    } catch (ex) {
      this._log.info("_loadSessionData - can not load session data file", ex);
      Telemetry.getHistogramById("TELEMETRY_SESSIONDATA_FAILED_LOAD").add(1);
      return false;
    }

    let data;
    try {
      data = JSON.parse(content);
    } catch (ex) {
      this._log.error("_loadSessionData - failed to parse session data", ex);
      Telemetry.getHistogramById("TELEMETRY_SESSIONDATA_FAILED_PARSE").add(1);
      return false;
    }

    if (!data ||
        !("profileSubsessionCounter" in data) ||
        !(typeof(data.profileSubsessionCounter) == "number") ||
        !("subsessionId" in data) || !("sessionId" in data)) {
      this._log.error("_loadSessionData - session data is invalid");
      Telemetry.getHistogramById("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").add(1);
      return false;
    }

    this._previousSessionId = data.sessionId;
    this._previousSubsessionId = data.subsessionId;
    // Add |_subsessionCounter| to the |_profileSubsessionCounter| to account for
    // new subsession while loading still takes place. This will always be exactly
    // 1 - the current subsessions.
    this._profileSubsessionCounter = data.profileSubsessionCounter +
                                     this._subsessionCounter;

    return true;
  }),

  /**
   * Get the session data object to serialise to disk.
   */
  _getSessionDataObject: function() {
    return {
      sessionId: this._sessionId,
      subsessionId: this._subsessionId,
      profileSubsessionCounter: this._profileSubsessionCounter,
    };
  },

  /**
   * Saves session data to disk.
   */
  _saveSessionData: Task.async(function* (sessionData) {
    let dataDir = OS.Path.join(OS.Constants.Path.profileDir, DATAREPORTING_DIRECTORY);
    yield OS.File.makeDir(dataDir);

    let filePath = OS.Path.join(dataDir, SESSION_STATE_FILE_NAME);
    try {
      yield CommonUtils.writeJSON(sessionData, filePath);
    } catch(e) {
      this._log.error("_saveSessionData - Failed to write session data to " + filePath, e);
      Telemetry.getHistogramById("TELEMETRY_SESSIONDATA_FAILED_SAVE").add(1);
    }
  }),

  _onEnvironmentChange: function(reason, oldEnvironment) {
    this._log.trace("_onEnvironmentChange", reason);
    let payload = this.getSessionPayload(REASON_ENVIRONMENT_CHANGE, true);

    TelemetryScheduler.reschedulePings(REASON_ENVIRONMENT_CHANGE, payload);

    let options = {
      addClientId: true,
      addEnvironment: true,
      overrideEnvironment: oldEnvironment,
    };
    TelemetryController.submitExternalPing(getPingType(payload), payload, options);
  },

  _isClassicReason: function(reason) {
    const classicReasons = [
      REASON_SAVED_SESSION,
      REASON_GATHER_PAYLOAD,
      REASON_TEST_PING,
    ];
    return classicReasons.indexOf(reason) != -1;
  },

  /**
   * Get an object describing the current state of this module for AsyncShutdown diagnostics.
   */
  _getState: function() {
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
  _saveAbortedSessionPing: function(aProvidedPayload = null) {
    const FILE_PATH = OS.Path.join(OS.Constants.Path.profileDir, DATAREPORTING_DIRECTORY,
                                   ABORTED_SESSION_FILE_NAME);
    this._log.trace("_saveAbortedSessionPing - ping path: " + FILE_PATH);

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
};
