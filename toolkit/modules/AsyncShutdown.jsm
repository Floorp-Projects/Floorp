/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Managing safe shutdown of asynchronous services.
 *
 *     THIS API IS EXPERIMENTAL AND SUBJECT TO CHANGE WITHOUT PRIOR NOTICE
 *        IF YOUR CODE USES IT, IT MAY HAVE STOPPED WORKING ALREADY
 *                          YOU HAVE BEEN WARNED
 *
 *
 * Firefox shutdown is composed of phases that take place
 * sequentially. Typically, each shutdown phase removes some
 * capabilities from the application. For instance, at the end of
 * phase profileBeforeChange, no service is permitted to write to the
 * profile directory (with the exception of Telemetry). Consequently,
 * if any service has requested I/O to the profile directory before or
 * during phase profileBeforeChange, the system must be informed that
 * these requests need to be completed before the end of phase
 * profileBeforeChange. Failing to inform the system of this
 * requirement can (and has been known to) cause data loss.
 *
 * Example: At some point during shutdown, the Add-On Manager needs to
 * ensure that all add-ons have safely written their data to disk,
 * before writing its own data. Since the data is saved to the
 * profile, this must be completed during phase profileBeforeChange.
 *
 * AsyncShutdown.profileBeforeChange.addBlocker(
 *   "Add-on manager: shutting down",
 *   function condition() {
 *     // Do things.
 *     // Perform I/O that must take place during phase profile-before-change
 *     return promise;
 *   }
 * });
 *
 * In this example, function |condition| will be called at some point
 * during phase profileBeforeChange and phase profileBeforeChange
 * itself is guaranteed to not terminate until |promise| is either
 * resolved or rejected.
 */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "gDebug",
  "@mozilla.org/xpcom/debug;1", "nsIDebug");
Object.defineProperty(this, "gCrashReporter", {
  get: function() {
    delete this.gCrashReporter;
    try {
      let reporter = Cc["@mozilla.org/xre/app-info;1"].
            getService(Ci.nsICrashReporter);
      return this.gCrashReporter = reporter;
    } catch (ex) {
      return this.gCrashReporter = null;
    }
  },
  configurable: true
});

// Display timeout warnings after 10 seconds
const DELAY_WARNING_MS = 10 * 1000;


// Crash the process if shutdown is really too long
// (allowing for sleep).
const PREF_DELAY_CRASH_MS = "toolkit.asyncshutdown.crash_timeout";
let DELAY_CRASH_MS = 60 * 1000; // One minute
try {
  DELAY_CRASH_MS = Services.prefs.getIntPref(PREF_DELAY_CRASH_MS);
} catch (ex) {
  // Ignore errors
}
Services.prefs.addObserver(PREF_DELAY_CRASH_MS, function() {
  DELAY_CRASH_MS = Services.prefs.getIntPref(PREF_DELAY_CRASH_MS);
}, false);


/**
 * Display a warning.
 *
 * As this code is generally used during shutdown, there are chances
 * that the UX will not be available to display warnings on the
 * console. We therefore use dump() rather than Cu.reportError().
 */
function log(msg, prefix = "", error = null) {
  dump(prefix + msg + "\n");
  if (error) {
    dump(prefix + error + "\n");
    if (typeof error == "object" && "stack" in error) {
      dump(prefix + error.stack + "\n");
    }
  }
}
function warn(msg, error = null) {
  return log(msg, "WARNING: ", error);
}
function err(msg, error = null) {
  return log(msg, "ERROR: ", error);
}

/**
 * Countdown for a given duration, skipping beats if the computer is too busy,
 * sleeping or otherwise unavailable.
 *
 * @param {number} delay An approximate delay to wait in milliseconds (rounded
 * up to the closest second).
 *
 * @return Deferred
 */
function looseTimer(delay) {
  let DELAY_BEAT = 1000;
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  let beats = Math.ceil(delay / DELAY_BEAT);
  let deferred = Promise.defer();
  timer.initWithCallback(function() {
    if (beats <= 0) {
      deferred.resolve();
    }
    --beats;
  }, DELAY_BEAT, Ci.nsITimer.TYPE_REPEATING_PRECISE_CAN_SKIP);
  // Ensure that the timer is both canceled once we are done with it
  // and not garbage-collected until then.
  deferred.promise.then(() => timer.cancel(), () => timer.cancel());
  return deferred;
}

this.EXPORTED_SYMBOLS = ["AsyncShutdown"];

/**
 * {string} topic -> phase
 */
let gPhases = new Map();

this.AsyncShutdown = {
  /**
   * Access function getPhase. For testing purposes only.
   */
  get _getPhase() {
    let accepted = false;
    try {
      accepted = Services.prefs.getBoolPref("toolkit.asyncshutdown.testing");
    } catch (ex) {
      // Ignore errors
    }
    if (accepted) {
      return getPhase;
    }
    return undefined;
  }
};

/**
 * Register a new phase.
 *
 * @param {string} topic The notification topic for this Phase.
 * @see {https://developer.mozilla.org/en-US/docs/Observer_Notifications}
 */
function getPhase(topic) {
  let phase = gPhases.get(topic);
  if (phase) {
    return phase;
  }
  let spinner = new Spinner(topic);
  phase = Object.freeze({
    /**
     * Register a blocker for the completion of a phase.
     *
     * @param {string} name The human-readable name of the blocker. Used
     * for debugging/error reporting. Please make sure that the name
     * respects the following model: "Some Service: some action in progress" -
     * for instance "OS.File: flushing all pending I/O";
     * @param {function|promise|*} condition A condition blocking the
     * completion of the phase. Generally, this is a function
     * returning a promise. This function is evaluated during the
     * phase and the phase is guaranteed to not terminate until the
     * resulting promise is either resolved or rejected. If
     * |condition| is not a function but another value |v|, it behaves
     * as if it were a function returning |v|.
     *
     * Examples:
     * AsyncShutdown.profileBeforeChange.addBlocker("Module: just a promise",
     *      promise); // profileBeforeChange will not complete until
     *                // promise is resolved or rejected
     *
     * AsyncShutdown.profileBeforeChange.addBlocker("Module: a callback",
     *     function callback() {
     *       // ...
     *       // Execute this code during profileBeforeChange
     *       return promise;
     *       // profileBeforeChange will not complete until promise
     *       // is resolved or rejected
     * });
     *
     * AsyncShutdown.profileBeforeChange.addBlocker("Module: trivial callback",
     *     function callback() {
     *       // ...
     *       // Execute this code during profileBeforeChange
     *       // No specific guarantee about completion of profileBeforeChange
     * });
     *
     */
    addBlocker: function(name, condition) {
      if (typeof name != "string") {
        throw new TypeError("Expected a human-readable name as first argument");
      }
      spinner.addBlocker({name: name, condition: condition});
    }
  });
  gPhases.set(topic, phase);
  return phase;
}

/**
 * Utility class used to spin the event loop until all blockers for a
 * Phase are satisfied.
 *
 * @param {string} topic The xpcom notification for that phase.
 */
function Spinner(topic) {
  this._topic = topic;
  this._conditions = new Set(); // set to |null| once it is too late to register
  Services.obs.addObserver(this, topic, false);
}

Spinner.prototype = {
  /**
   * Register a new condition for this phase.
   *
   * @param {object} condition A Condition that must be fulfilled before
   * we complete this Phase.
   * Must contain fields:
   * - {string} name The human-readable name of the condition. Used
   * for debugging/error reporting.
   * - {function} action An action that needs to be completed
   * before we proceed to the next runstate. If |action| returns a promise,
   * we wait until the promise is resolved/rejected before proceeding
   * to the next runstate.
   */
  addBlocker: function(condition) {
    if (!this._conditions) {
      throw new Error("Phase " + this._topic +
                      " has already begun, it is too late to register" +
                      " completion conditions.");
    }
    this._conditions.add(condition);
  },

  observe: function() {
    let topic = this._topic;
    Services.obs.removeObserver(this, topic);

    let conditions = this._conditions;
    this._conditions = null; // Too late to register

    if (conditions.size == 0) {
      // No need to spin anything
      return;
    }

    // The promises for which we are waiting.
    let allPromises = [];

    // Information to determine and report to the user which conditions
    // are not satisfied yet.
    let allMonitors = [];

    for (let {condition, name} of conditions) {
      // Gather all completion conditions

      try {
        if (typeof condition == "function") {
          // Normalize |condition| to the result of the function.
          try {
            condition = condition(topic);
          } catch (ex) {
            condition = Promise.reject(ex);
          }
        }
        // Normalize to a promise. Of course, if |condition| was not a
        // promise in the first place (in particular if the above
        // function returned |undefined| or failed), that new promise
        // isn't going to be terribly interesting, but it will behave
        // as a promise.
        condition = Promise.resolve(condition);

        // If the promise takes too long to be resolved/rejected,
        // we need to notify the user.
        //
        // If it takes way too long, we need to crash.

        let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        timer.initWithCallback(function() {
          let msg = "A phase completion condition is" +
            " taking too long to complete." +
            " Condition: " + monitor.name +
            " Phase: " + topic;
          warn(msg);
        }, DELAY_WARNING_MS, Ci.nsITimer.TYPE_ONE_SHOT);

        let monitor = {
          isFrozen: true,
          name: name
        };
        condition = condition.then(function onSuccess() {
            timer.cancel(); // As a side-effect, this prevents |timer| from
                            // being garbage-collected too early.
            monitor.isFrozen = false;
          }, function onError(error) {
            timer.cancel();
            let msg = "A completion condition encountered an error" +
                " while we were spinning the event loop." +
                " Condition: " + name +
                " Phase: " + topic;
            warn(msg, error);
            monitor.isFrozen = false;
        });
        allMonitors.push(monitor);
        allPromises.push(condition);

      } catch (error) {
          let msg = "A completion condition encountered an error" +
                " while we were initializing the phase." +
                " Condition: " + name +
                " Phase: " + topic;
          warn(msg, error);
      }

    }
    conditions = null;

    let promise = Promise.all(allPromises);
    allPromises = null;

    promise = promise.then(null, function onError(error) {
      // I don't think that this can happen.
      // However, let's be overcautious with async/shutdown error reporting.
      let msg = "An uncaught error appeared while completing the phase." +
            " Phase: " + topic;
      warn(msg, error);
    });

    let satisfied = false; // |true| once we have satisfied all conditions

    // If after DELAY_CRASH_MS (approximately one minute, adjusted to take
    // into account sleep and otherwise busy computer) we have not finished
    // this shutdown phase, we assume that the shutdown is somehow frozen,
    // presumably deadlocked. At this stage, the only thing we can do to
    // avoid leaving the user's computer in an unstable (and battery-sucking)
    // situation is report the issue and crash.
    let timeToCrash = looseTimer(DELAY_CRASH_MS);
    timeToCrash.promise.then(
      function onTimeout() {
        // Report the problem as best as we can, then crash.
        let frozen = [];
        for (let {name, isFrozen} of allMonitors) {
          if (isFrozen) {
            frozen.push(name);
          }
        }

        let msg = "At least one completion condition failed to complete" +
              " within a reasonable amount of time. Causing a crash to" +
              " ensure that we do not leave the user with an unresponsive" +
              " process draining resources." +
              " Conditions: " + frozen.join(", ") +
              " Phase: " + topic;
        err(msg);
        if (gCrashReporter) {
          let data = {
            phase: topic,
            conditions: frozen
          };
          gCrashReporter.annotateCrashReport("AsyncShutdownTimeout",
            JSON.stringify(data));
        } else {
          warn("No crash reporter available");
        }

        let error = new Error();
        gDebug.abort(error.fileName, error.lineNumber + 1);
      },
      function onSatisfied() {
        // The promise has been rejected, which means that we have satisfied
        // all completion conditions.
      });

    promise = promise.then(function() {
      satisfied = true;
      timeToCrash.reject();
    }/* No error is possible here*/);

    // Now, spin the event loop
    let thread = Services.tm.mainThread;
    while(!satisfied) {
      thread.processNextEvent(true);
    }
  }
};


// List of well-known runstates
// Ideally, runstates should be registered from the component that decides
// when they start/stop. For compatibility with existing startup/shutdown
// mechanisms, we register a few runstates here.

this.AsyncShutdown.profileBeforeChange = getPhase("profile-before-change");
this.AsyncShutdown.webWorkersShutdown = getPhase("web-workers-shutdown");
Object.freeze(this.AsyncShutdown);
