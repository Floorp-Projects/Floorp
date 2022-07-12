/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TelemetryScheduler", "Policy"];

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { TelemetrySession } = ChromeUtils.import(
  "resource://gre/modules/TelemetrySession.jsm"
);
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { clearTimeout, setTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);
// Other pings
const { TelemetryPrioPing } = ChromeUtils.import(
  "resource://gre/modules/PrioPing.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyServiceGetters(lazy, {
  idleService: ["@mozilla.org/widget/useridleservice;1", "nsIUserIdleService"],
});

const MIN_SUBSESSION_LENGTH_MS =
  Services.prefs.getIntPref("toolkit.telemetry.minSubsessionLength", 5 * 60) *
  1000;

const LOGGER_NAME = "Toolkit.Telemetry";

// Seconds of idle time before pinging.
// On idle-daily a gather-telemetry notification is fired, during it probes can
// start asynchronous tasks to gather data.
const IDLE_TIMEOUT_SECONDS = Services.prefs.getIntPref(
  "toolkit.telemetry.idleTimeout",
  5 * 60
);

// Execute a scheduler tick every 5 minutes.
const SCHEDULER_TICK_INTERVAL_MS =
  Services.prefs.getIntPref(
    "toolkit.telemetry.scheduler.tickInterval",
    5 * 60
  ) * 1000;
// When user is idle, execute a scheduler tick every 60 minutes.
const SCHEDULER_TICK_IDLE_INTERVAL_MS =
  Services.prefs.getIntPref(
    "toolkit.telemetry.scheduler.idleTickInterval",
    60 * 60
  ) * 1000;

// The maximum time (ms) until the tick should moved from the idle
// queue to the regular queue if it hasn't been executed yet.
const SCHEDULER_TICK_MAX_IDLE_DELAY_MS = 60 * 1000;

// The frequency at which we persist session data to the disk to prevent data loss
// in case of aborted sessions (currently 5 minutes).
const ABORTED_SESSION_UPDATE_INTERVAL_MS = 5 * 60 * 1000;

// The tolerance we have when checking if it's midnight (15 minutes).
const SCHEDULER_MIDNIGHT_TOLERANCE_MS = 15 * 60 * 1000;

/**
 * This is a policy object used to override behavior for testing.
 */
var Policy = {
  now: () => new Date(),
  setSchedulerTickTimeout: (callback, delayMs) => setTimeout(callback, delayMs),
  clearSchedulerTickTimeout: id => clearTimeout(id),
  prioEncode: (batchID, prioParams) => PrioEncoder.encode(batchID, prioParams),
};

/**
 * TelemetryScheduler contains a single timer driving all regularly-scheduled
 * Telemetry related jobs. Having a single place with this logic simplifies
 * reasoning about scheduling actions in a single place, making it easier to
 * coordinate jobs and coalesce them.
 */
var TelemetryScheduler = {
  // Tracks the main ping
  _lastDailyPingTime: 0,
  // Tracks the aborted session ping
  _lastSessionCheckpointTime: 0,
  // Tracks all other pings at regular intervals
  _lastPeriodicPingTime: 0,

  _log: null,

  // The timer which drives the scheduler.
  _schedulerTimer: null,
  // The interval used by the scheduler timer.
  _schedulerInterval: 0,
  _shuttingDown: true,
  _isUserIdle: false,

  /**
   * Initialises the scheduler and schedules the first daily/aborted session pings.
   */
  init() {
    this._log = Log.repository.getLoggerWithMessagePrefix(
      LOGGER_NAME,
      "TelemetryScheduler::"
    );
    this._log.trace("init");
    this._shuttingDown = false;
    this._isUserIdle = false;

    // Initialize the last daily ping and aborted session last due times to the current time.
    // Otherwise, we might end up sending daily pings even if the subsession is not long enough.
    let now = Policy.now();
    this._lastDailyPingTime = now.getTime();
    this._lastPeriodicPingTime = now.getTime();
    this._lastSessionCheckpointTime = now.getTime();
    this._rescheduleTimeout();

    lazy.idleService.addIdleObserver(this, IDLE_TIMEOUT_SECONDS);
    Services.obs.addObserver(this, "wake_notification");
  },

  /**
   * Stops the scheduler.
   */
  shutdown() {
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

    lazy.idleService.removeIdleObserver(this, IDLE_TIMEOUT_SECONDS);
    Services.obs.removeObserver(this, "wake_notification");

    this._shuttingDown = true;
  },

  _clearTimeout() {
    if (this._schedulerTimer) {
      Policy.clearSchedulerTickTimeout(this._schedulerTimer);
    }
  },

  /**
   * Reschedules the tick timer.
   */
  _rescheduleTimeout() {
    this._log.trace("_rescheduleTimeout - isUserIdle: " + this._isUserIdle);
    if (this._shuttingDown) {
      this._log.warn("_rescheduleTimeout - already shutdown");
      return;
    }

    this._clearTimeout();

    const now = Policy.now();
    let timeout = SCHEDULER_TICK_INTERVAL_MS;

    // When the user is idle we want to fire the timer less often.
    if (this._isUserIdle) {
      timeout = SCHEDULER_TICK_IDLE_INTERVAL_MS;
      // We need to make sure though that we don't miss sending pings around
      // midnight when we use the longer idle intervals.
      const nextMidnight = TelemetryUtils.getNextMidnight(now);
      timeout = Math.min(timeout, nextMidnight.getTime() - now.getTime());
    }

    this._log.trace(
      "_rescheduleTimeout - scheduling next tick for " +
        new Date(now.getTime() + timeout)
    );
    this._schedulerTimer = Policy.setSchedulerTickTimeout(
      () => this._onSchedulerTick(),
      timeout
    );
  },

  _sentPingToday(pingTime, nowDate) {
    // This is today's date and also the previous midnight (0:00).
    const todayDate = TelemetryUtils.truncateToDays(nowDate);
    // We consider a ping sent for today if it occured after or at 00:00 today.
    return pingTime >= todayDate.getTime();
  },

  /**
   * Checks if we can send a daily ping or not.
   * @param {Object} nowDate A date object.
   * @return {Boolean} True if we can send the daily ping, false otherwise.
   */
  _isDailyPingDue(nowDate) {
    // The daily ping is not due if we already sent one today.
    if (this._sentPingToday(this._lastDailyPingTime, nowDate)) {
      this._log.trace("_isDailyPingDue - already sent one today");
      return false;
    }

    // Avoid overly short sessions.
    const timeSinceLastDaily = nowDate.getTime() - this._lastDailyPingTime;
    if (timeSinceLastDaily < MIN_SUBSESSION_LENGTH_MS) {
      this._log.trace(
        "_isDailyPingDue - delaying daily to keep minimum session length"
      );
      return false;
    }

    this._log.trace("_isDailyPingDue - is due");
    return true;
  },

  /**
   * Checks if we can send a regular ping or not.
   * @param {Object} nowDate A date object.
   * @return {Boolean} True if we can send the regular pings, false otherwise.
   */
  _isPeriodicPingDue(nowDate) {
    // The periodic ping is not due if we already sent one today.
    if (this._sentPingToday(this._lastPeriodicPingTime, nowDate)) {
      this._log.trace("_isPeriodicPingDue - already sent one today");
      return false;
    }

    this._log.trace("_isPeriodicPingDue - is due");
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
  _saveAbortedPing(now, competingPayload = null) {
    this._lastSessionCheckpointTime = now;
    return TelemetrySession.saveAbortedSessionPing(competingPayload).catch(e =>
      this._log.error("_saveAbortedPing - Failed", e)
    );
  },

  /**
   * The notifications handler.
   */
  observe(aSubject, aTopic, aData) {
    this._log.trace("observe - aTopic: " + aTopic);
    switch (aTopic) {
      case "idle":
        // If the user is idle, increase the tick interval.
        this._isUserIdle = true;
        return this._onSchedulerTick();
      case "active":
        // User is back to work, restore the original tick interval.
        this._isUserIdle = false;
        return this._onSchedulerTick(true);
      case "wake_notification":
        // The machine woke up from sleep, trigger a tick to avoid sessions
        // spanning more than a day.
        // This is needed because sleep time does not count towards timeouts
        // on Mac & Linux - see bug 1262386, bug 1204823 et al.
        return this._onSchedulerTick(true);
    }
    return undefined;
  },

  /**
   * Creates an object with a method `dispatch` that will call `dispatchFn` unless
   * the method `cancel` is called beforehand.
   *
   * This is used to wrap main thread idle dispatch since it does not provide a
   * cancel mechanism.
   */
  _makeIdleDispatch(dispatchFn) {
    this._log.trace("_makeIdleDispatch");
    let fn = dispatchFn;
    let l = msg => this._log.trace(msg); // need to bind `this`
    return {
      cancel() {
        fn = undefined;
      },
      dispatch(resolve, reject) {
        l("_makeIdleDispatch.dispatch - !!fn: " + !!fn);
        if (!fn) {
          return Promise.resolve().then(resolve, reject);
        }
        return fn(resolve, reject);
      },
    };
  },

  /**
   * Performs a scheduler tick. This function manages Telemetry recurring operations.
   * @param {Boolean} [dispatchOnIdle=false] If true, the tick is dispatched in the
   *                  next idle cycle of the main thread.
   * @return {Promise} A promise, only used when testing, resolved when the scheduled
   *                   operation completes.
   */
  _onSchedulerTick(dispatchOnIdle = false) {
    this._log.trace("_onSchedulerTick - dispatchOnIdle: " + dispatchOnIdle);
    // This call might not be triggered from a timeout. In that case we don't want to
    // leave any previously scheduled timeouts pending.
    this._clearTimeout();

    if (this._idleDispatch) {
      this._idleDispatch.cancel();
    }

    if (this._shuttingDown) {
      this._log.warn("_onSchedulerTick - already shutdown.");
      return Promise.reject(new Error("Already shutdown."));
    }

    let promise = Promise.resolve();
    try {
      if (dispatchOnIdle) {
        this._idleDispatch = this._makeIdleDispatch((resolve, reject) => {
          this._log.trace(
            "_onSchedulerTick - ildeDispatchToMainThread dispatch"
          );
          return this._schedulerTickLogic().then(resolve, reject);
        });
        promise = new Promise((resolve, reject) =>
          Services.tm.idleDispatchToMainThread(() => {
            return this._idleDispatch
              ? this._idleDispatch.dispatch(resolve, reject)
              : Promise.resolve().then(resolve, reject);
          }, SCHEDULER_TICK_MAX_IDLE_DELAY_MS)
        );
      } else {
        promise = this._schedulerTickLogic();
      }
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
  _schedulerTickLogic() {
    this._log.trace("_schedulerTickLogic");

    let nowDate = Policy.now();
    let now = nowDate.getTime();

    // Check if the daily ping is due.
    const shouldSendDaily = this._isDailyPingDue(nowDate);
    // Check if other regular pings are due.
    const shouldSendPeriodic = this._isPeriodicPingDue(nowDate);

    if (shouldSendPeriodic) {
      this._log.trace("_schedulerTickLogic - Periodic ping due.");
      this._lastPeriodicPingTime = now;
      // Send other pings.
      TelemetryPrioPing.periodicPing();
    }

    if (shouldSendDaily) {
      this._log.trace("_schedulerTickLogic - Daily ping due.");
      this._lastDailyPingTime = now;
      return TelemetrySession.sendDailyPing();
    }

    // Check if the aborted-session ping is due. If a daily ping was saved above, it was
    // already duplicated as an aborted-session ping.
    const isAbortedPingDue =
      now - this._lastSessionCheckpointTime >=
      ABORTED_SESSION_UPDATE_INTERVAL_MS;
    if (isAbortedPingDue) {
      this._log.trace("_schedulerTickLogic - Aborted session ping due.");
      return this._saveAbortedPing(now);
    }

    // No ping is due.
    this._log.trace("_schedulerTickLogic - No ping due.");
    return Promise.resolve();
  },

  /**
   * Re-schedule the daily ping if some other equivalent ping was sent.
   *
   * This is only called from TelemetrySession when a main ping with reason 'environment-change'
   * is sent.
   *
   * @param {Object} [payload] The payload of the ping that was sent,
   *                           to be stored as an aborted-session ping.
   */
  rescheduleDailyPing(payload) {
    if (this._shuttingDown) {
      this._log.error("rescheduleDailyPing - already shutdown");
      return;
    }

    this._log.trace("rescheduleDailyPing");
    let now = Policy.now();

    // We just generated an environment-changed ping, save it as an aborted session and
    // update the schedules.
    this._saveAbortedPing(now.getTime(), payload);

    // If we're close to midnight, skip today's daily ping and reschedule it for tomorrow.
    let nearestMidnight = TelemetryUtils.getNearestMidnight(
      now,
      SCHEDULER_MIDNIGHT_TOLERANCE_MS
    );
    if (nearestMidnight) {
      this._lastDailyPingTime = now.getTime();
    }
  },
};
