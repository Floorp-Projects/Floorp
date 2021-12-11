/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PREF_APP_UPDATE_LASTUPDATETIME_FMT = "app.update.lastUpdateTime.%ID%";
const PREF_APP_UPDATE_TIMERMINIMUMDELAY = "app.update.timerMinimumDelay";
const PREF_APP_UPDATE_TIMERFIRSTINTERVAL = "app.update.timerFirstInterval";
const PREF_APP_UPDATE_LOG = "app.update.log";

const CATEGORY_UPDATE_TIMER = "update-timer";

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gLogEnabled",
  PREF_APP_UPDATE_LOG,
  false
);

/**
 *  Logs a string to the error console.
 *  @param   string
 *           The string to write to the error console.
 *  @param   bool
 *           Whether to log even if logging is disabled.
 */
function LOG(string, alwaysLog = false) {
  if (alwaysLog || gLogEnabled) {
    dump("*** UTM:SVC " + string + "\n");
    Services.console.logStringMessage("UTM:SVC " + string);
  }
}

/**
 *  A manager for timers. Manages timers that fire over long periods of time
 *  (e.g. days, weeks, months).
 *  @constructor
 */
function TimerManager() {
  Services.obs.addObserver(this, "profile-before-change");
}
TimerManager.prototype = {
  /**
   * The Checker Timer
   */
  _timer: null,

  /**
   * The Checker Timer minimum delay interval as specified by the
   * app.update.timerMinimumDelay pref. If the app.update.timerMinimumDelay
   * pref doesn't exist this will default to 120000.
   */
  _timerMinimumDelay: null,

  /**
   * The set of registered timers.
   */
  _timers: {},

  /**
   * See nsIObserver.idl
   */
  observe: function TM_observe(aSubject, aTopic, aData) {
    // Prevent setting the timer interval to a value of less than 30 seconds.
    var minInterval = 30000;
    // Prevent setting the first timer interval to a value of less than 10
    // seconds.
    var minFirstInterval = 10000;
    switch (aTopic) {
      case "utm-test-init":
        // Enforce a minimum timer interval of 500 ms for tests and fall through
        // to profile-after-change to initialize the timer.
        minInterval = 500;
        minFirstInterval = 500;
      // fall through
      case "profile-after-change":
        this._timerMinimumDelay = Math.max(
          1000 *
            Services.prefs.getIntPref(PREF_APP_UPDATE_TIMERMINIMUMDELAY, 120),
          minInterval
        );
        // Prevent the timer delay between notifications to other consumers from
        // being greater than 5 minutes which is 300000 milliseconds.
        this._timerMinimumDelay = Math.min(this._timerMinimumDelay, 300000);
        // Prevent the first interval from being less than the value of minFirstInterval
        let firstInterval = Math.max(
          Services.prefs.getIntPref(PREF_APP_UPDATE_TIMERFIRSTINTERVAL, 30000),
          minFirstInterval
        );
        // Prevent the first interval from being greater than 2 minutes which is
        // 120000 milliseconds.
        firstInterval = Math.min(firstInterval, 120000);
        // Cancel the timer if it has already been initialized. This is primarily
        // for tests.
        this._canEnsureTimer = true;
        this._ensureTimer(firstInterval);
        break;
      case "profile-before-change":
        Services.obs.removeObserver(this, "profile-before-change");

        // Release everything we hold onto.
        this._cancelTimer();
        for (var timerID in this._timers) {
          delete this._timers[timerID];
        }
        this._timers = null;
        break;
    }
  },

  /**
   * Called when the checking timer fires.
   *
   * We only fire one notification each time, so that the operations are
   * staggered. We don't want too many to happen at once, which could
   * negatively impact responsiveness.
   *
   * @param   timer
   *          The checking timer that fired.
   */
  notify: function TM_notify(timer) {
    var nextDelay = null;
    function updateNextDelay(delay) {
      if (nextDelay === null || delay < nextDelay) {
        nextDelay = delay;
      }
    }

    // Each timer calls tryFire(), which figures out which is the one that
    // wanted to be called earliest. That one will be fired; the others are
    // skipped and will be done later.
    var now = Math.round(Date.now() / 1000);

    var callbackToFire = null;
    var earliestIntendedTime = null;
    var skippedFirings = false;
    var lastUpdateTime = null;
    var timerIDToFire = null;
    function tryFire(timerID, callback, intendedTime) {
      var selected = false;
      if (intendedTime <= now) {
        if (
          intendedTime < earliestIntendedTime ||
          earliestIntendedTime === null
        ) {
          timerIDToFire = timerID;
          callbackToFire = callback;
          earliestIntendedTime = intendedTime;
          selected = true;
        } else if (earliestIntendedTime !== null) {
          skippedFirings = true;
        }
      }
      // We do not need to updateNextDelay for the timer that actually fires;
      // we'll update right after it fires, with the proper intended time.
      // Note that we might select one, then select another later (with an
      // earlier intended time); it is still ok that we did not update for
      // the first one, since if we have skipped firings, the next delay
      // will be the minimum delay anyhow.
      if (!selected) {
        updateNextDelay(intendedTime - now);
      }
    }

    for (let { value } of Services.catMan.enumerateCategory(
      CATEGORY_UPDATE_TIMER
    )) {
      let [
        cid,
        method,
        timerID,
        prefInterval,
        defaultInterval,
        maxInterval,
      ] = value.split(",");

      defaultInterval = parseInt(defaultInterval);
      // cid and method are validated below when calling notify.
      if (!timerID || !defaultInterval || isNaN(defaultInterval)) {
        LOG(
          "TimerManager:notify - update-timer category registered" +
            (cid ? " for " + cid : "") +
            " without required parameters - " +
            "skipping"
        );
        continue;
      }

      let interval = Services.prefs.getIntPref(prefInterval, defaultInterval);
      // Allow the update-timer category to specify a maximum value to prevent
      // values larger than desired.
      maxInterval = parseInt(maxInterval);
      if (maxInterval && !isNaN(maxInterval)) {
        interval = Math.min(interval, maxInterval);
      }
      let prefLastUpdate = PREF_APP_UPDATE_LASTUPDATETIME_FMT.replace(
        /%ID%/,
        timerID
      );
      // Initialize the last update time to 0 when the preference isn't set so
      // the timer will be notified soon after a new profile's first use.
      lastUpdateTime = Services.prefs.getIntPref(prefLastUpdate, 0);

      // If the last update time is greater than the current time then reset
      // it to 0 and the timer manager will correct the value when it fires
      // next for this consumer.
      if (lastUpdateTime > now) {
        lastUpdateTime = 0;
      }

      if (lastUpdateTime == 0) {
        Services.prefs.setIntPref(prefLastUpdate, lastUpdateTime);
      }

      tryFire(
        timerID,
        function() {
          try {
            Cc[cid][method](Ci.nsITimerCallback).notify(timer);
            LOG("TimerManager:notify - notified " + cid);
          } catch (e) {
            LOG(
              "TimerManager:notify - error notifying component id: " +
                cid +
                " ,error: " +
                e
            );
          }
          lastUpdateTime = now;
          Services.prefs.setIntPref(prefLastUpdate, lastUpdateTime);
          updateNextDelay(lastUpdateTime + interval - now);
        },
        lastUpdateTime + interval
      );
    }

    for (let _timerID in this._timers) {
      let timerID = _timerID; // necessary for the closure to work properly
      let timerData = this._timers[timerID];
      // If the last update time is greater than the current time then reset
      // it to 0 and the timer manager will correct the value when it fires
      // next for this consumer.
      if (timerData.lastUpdateTime > now) {
        let prefLastUpdate = PREF_APP_UPDATE_LASTUPDATETIME_FMT.replace(
          /%ID%/,
          timerID
        );
        timerData.lastUpdateTime = 0;
        Services.prefs.setIntPref(prefLastUpdate, timerData.lastUpdateTime);
      }
      tryFire(
        timerID,
        function() {
          if (timerData.callback && timerData.callback.notify) {
            ChromeUtils.idleDispatch(() => {
              try {
                timerData.callback.notify(timer);
                LOG(`TimerManager:notify - notified timerID: ${timerID}`);
              } catch (e) {
                LOG(
                  `TimerManager:notify - error notifying timerID: ${timerID}, error: ${e}`
                );
              }
            });
          } else {
            LOG(
              `TimerManager:notify - timerID: ${timerID} doesn't implement nsITimerCallback - skipping`
            );
          }
          lastUpdateTime = now;
          timerData.lastUpdateTime = lastUpdateTime;
          let prefLastUpdate = PREF_APP_UPDATE_LASTUPDATETIME_FMT.replace(
            /%ID%/,
            timerID
          );
          Services.prefs.setIntPref(prefLastUpdate, lastUpdateTime);
          updateNextDelay(timerData.lastUpdateTime + timerData.interval - now);
        },
        timerData.lastUpdateTime + timerData.interval
      );
    }

    if (callbackToFire) {
      LOG(
        `TimerManager:notify - fire timerID: ${timerIDToFire} ` +
          `intended time: ${earliestIntendedTime} (${new Date(
            earliestIntendedTime * 1000
          ).toISOString()})`
      );
      callbackToFire();
    }

    if (nextDelay !== null) {
      if (skippedFirings) {
        timer.delay = this._timerMinimumDelay;
      } else {
        timer.delay = Math.max(nextDelay * 1000, this._timerMinimumDelay);
      }
      this.lastTimerReset = Date.now();
    } else {
      this._cancelTimer();
    }
  },

  /**
   * Starts the timer, if necessary, and ensures that it will fire soon enough
   * to happen after time |interval| (in milliseconds).
   */
  _ensureTimer(interval) {
    if (!this._canEnsureTimer) {
      return;
    }
    if (!this._timer) {
      this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._timer.initWithCallback(
        this,
        interval,
        Ci.nsITimer.TYPE_REPEATING_SLACK
      );
      this.lastTimerReset = Date.now();
    } else if (
      Date.now() + interval <
      this.lastTimerReset + this._timer.delay
    ) {
      this._timer.delay = Math.max(
        this.lastTimerReset + interval - Date.now(),
        0
      );
    }
  },

  /**
   * Stops the timer, if it is running.
   */
  _cancelTimer() {
    if (this._timer) {
      this._timer.cancel();
      this._timer = null;
    }
  },

  /**
   * See nsIUpdateTimerManager.idl
   */
  registerTimer: function TM_registerTimer(id, callback, interval, skipFirst) {
    LOG(
      `TimerManager:registerTimer - timerID: ${id} interval: ${interval} skipFirst: ${skipFirst}`
    );
    if (this._timers === null) {
      // Use normal logging since reportError is not available while shutting
      // down.
      LOG(
        "TimerManager:registerTimer called after profile-before-change " +
          "notification. Ignoring timer registration for id: " +
          id,
        true
      );
      return;
    }
    if (id in this._timers && callback != this._timers[id].callback) {
      LOG(
        "TimerManager:registerTimer - Ignoring second registration for " + id
      );
      return;
    }
    let prefLastUpdate = PREF_APP_UPDATE_LASTUPDATETIME_FMT.replace(/%ID%/, id);
    // Initialize the last update time to 0 when the preference isn't set so
    // the timer will be notified soon after a new profile's first use.
    let lastUpdateTime = Services.prefs.getIntPref(prefLastUpdate, 0);
    let now = Math.round(Date.now() / 1000);
    if (lastUpdateTime > now) {
      lastUpdateTime = 0;
    }
    if (lastUpdateTime == 0) {
      if (skipFirst) {
        lastUpdateTime = now;
      }
      Services.prefs.setIntPref(prefLastUpdate, lastUpdateTime);
    }
    this._timers[id] = { callback, interval, lastUpdateTime };

    this._ensureTimer(interval * 1000);
  },

  unregisterTimer: function TM_unregisterTimer(id) {
    LOG("TimerManager:unregisterTimer - id: " + id);
    if (id in this._timers) {
      delete this._timers[id];
    } else {
      LOG(
        "TimerManager:unregisterTimer - Ignoring unregistration request for " +
          "unknown id: " +
          id
      );
    }
  },

  classID: Components.ID("{B322A5C0-A419-484E-96BA-D7182163899F}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsIUpdateTimerManager",
    "nsITimerCallback",
    "nsIObserver",
  ]),
};

var EXPORTED_SYMBOLS = ["TimerManager"];
