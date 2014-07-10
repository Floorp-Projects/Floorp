/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Managing safe shutdown of asynchronous services.
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
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
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
function fatalerr(msg, error = null) {
  return log(msg, "FATAL ERROR: ", error);
}

// Utility function designed to get the current state of execution
// of a blocker.
// We are a little paranoid here to ensure that in case of evaluation
// error we do not block the AsyncShutdown.
function safeGetState(fetchState) {
  if (!fetchState) {
    return "(none)";
  }
  let data, string;
  try {
    // Evaluate fetchState(), normalize the result into something that we can
    // safely stringify or upload.
    string = JSON.stringify(fetchState());
    data = JSON.parse(string);
    // Simplify the rest of the code by ensuring that we can simply
    // concatenate the result to a message.
    if (data && typeof data == "object") {
      data.toString = function() {
        return string;
      };
    }
    return data;
  } catch (ex) {
    if (string) {
      return string;
    }
    try {
      return "Error getting state: " + ex + " at " + ex.stack;
    } catch (ex2) {
      return "Error getting state but could not display error";
    }
  }
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
     * @param {function*} fetchState Optionally, a function returning
     * information about the current state of the blocker as an
     * object. Used for providing more details when logging errors or
     * crashing.
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
     */
    addBlocker: function(name, condition, fetchState = null) {
      spinner.addBlocker(name, condition, fetchState);
    },
    /**
     * Remove the blocker for a condition.
     *
     * If several blockers have been registered for the same
     * condition, remove all these blockers. If no blocker has been
     * registered for this condition, this is a noop.
     *
     * @return {boolean} true if a blocker has been removed, false
     * otherwise. Note that a result of false may mean either that
     * the blocker has never been installed or that the phase has
     * completed and the blocker has already been resolved.
     */
    removeBlocker: function(condition) {
      return spinner.removeBlocker(condition);
    },
    /**
     * Trigger the phase without having to broadcast a
     * notification. For testing purposes only.
     */
    get _trigger() {
      let accepted = false;
      try {
        accepted = Services.prefs.getBoolPref("toolkit.asyncshutdown.testing");
      } catch (ex) {
        // Ignore errors
      }
      if (accepted) {
        return () => spinner.observe();
      }
      return undefined;
    }
  });
  gPhases.set(topic, phase);
  return phase;
};

/**
 * Utility class used to spin the event loop until all blockers for a
 * Phase are satisfied.
 *
 * @param {string} topic The xpcom notification for that phase.
 */
function Spinner(topic) {
  this._barrier = new Barrier(topic);
  this._topic = topic;
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
  addBlocker: function(name, condition, fetchState) {
    this._barrier.client.addBlocker(name, condition, fetchState);
  },
  /**
   * Remove the blocker for a condition.
   *
   * If several blockers have been registered for the same
   * condition, remove all these blockers. If no blocker has been
   * registered for this condition, this is a noop.
   *
   * @return {boolean} true if a blocker has been removed, false
   * otherwise. Note that a result of false may mean either that
   * the blocker has never been installed or that the phase has
   * completed and the blocker has already been resolved.
   */
  removeBlocker: function(condition) {
    return this._barrier.client.removeBlocker(condition);
  },

  // nsIObserver.observe
  observe: function() {
    let topic = this._topic;
    let barrier = this._barrier;
    Services.obs.removeObserver(this, topic);

    let satisfied = false; // |true| once we have satisfied all conditions
    let promise = this._barrier.wait({
      warnAfterMS: DELAY_WARNING_MS,
      crashAfterMS: DELAY_CRASH_MS
    });

    // Now, spin the event loop
    promise.then(() => satisfied = true); // This promise cannot reject
    let thread = Services.tm.mainThread;
    while (!satisfied) {
      try {
        thread.processNextEvent(true);
      } catch (ex) {
        // An uncaught error should not stop us, but it should still
        // be reported and cause tests to fail.
        Promise.reject(ex);
      }
    }
  }
};

/**
 * A mechanism used to register blockers that prevent some action from
 * happening.
 *
 * An instance of |Barrier| provides a capability |client| that
 * clients can use to register blockers. The barrier is resolved once
 * all registered blockers have been resolved. The owner of the
 * |Barrier| may wait for the resolution of the barrier and obtain
 * information on which blockers have not been resolved yet.
 *
 * @param {string} name The name of the blocker. Used mainly for error-
 *     reporting.
 */
function Barrier(name) {
  /**
   * The set of conditions registered by clients, as a map.
   *
   * Key: condition (function)
   * Value: Array of {name: string, fetchState: function}
   */
  this._conditions = new Map();

  /**
   * Indirections, used to let clients cancel a blocker when they
   * call removeBlocker().
   *
   * Key: condition (function)
   * Value: Deferred.
   */
  this._indirections = null;

  /**
   * The name of the barrier.
   */
  this._name = name;

  /**
   * A cache for the promise returned by wait().
   */
  this._promise = null;

  /**
   * An array of objects used to monitor the state of each blocker.
   */
  this._monitors = null;

  /**
   * The capability of adding blockers. This object may safely be returned
   * or passed to clients.
   */
  this.client = {
    /**
     * Register a blocker for the completion of this barrier.
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
     * @param {function*} fetchState Optionally, a function returning
     * information about the current state of the blocker as an
     * object. Used for providing more details when logging errors or
     * crashing.
     */
    addBlocker: function(name, condition, fetchState) {
      if (typeof name != "string") {
        throw new TypeError("Expected a human-readable name as first argument");
      }
      if (fetchState && typeof fetchState != "function") {
        throw new TypeError("Expected nothing or a function as third argument");
      }
      if (!this._conditions) {
	throw new Error("Phase " + this._name +
			" has already begun, it is too late to register" +
			" completion condition '" + name + "'.");
      }

      // Determine the filename and line number of the caller.
      let leaf = Components.stack;
      let frame;
      for (frame = leaf; frame != null && frame.filename == leaf.filename; frame = frame.caller) {
        // Climb up the stack
      }
      let filename = frame ? frame.filename : "?";
      let lineNumber = frame ? frame.lineNumber : -1;

      // Now build the rest of the stack as a string, using Task.jsm's rewriting
      // to ensure that we do not lose information at each call to `Task.spawn`.
      let frames = [];
      while (frame != null) {
        frames.push(frame.filename + ":" + frame.name + ":" + frame.lineNumber);
        frame = frame.caller;
      }
      let stack = Task.Debugging.generateReadableStack(frames.join("\n")).split("\n");

      let set = this._conditions.get(condition);
      if (!set) {
        set = [];
        this._conditions.set(condition, set);
      }
      set.push({name: name,
                fetchState: fetchState,
                filename: filename,
                lineNumber: lineNumber,
                stack: stack});
    }.bind(this),

    /**
     * Remove the blocker for a condition.
     *
     * If several blockers have been registered for the same
     * condition, remove all these blockers. If no blocker has been
     * registered for this condition, this is a noop.
     *
     * @return {boolean} true if at least one blocker has been
     * removed, false otherwise.
     */
    removeBlocker: function(condition) {
      if (this._conditions) {
        // wait() hasn't been called yet.
        return this._conditions.delete(condition);
      }

      if (this._indirections) {
        // wait() is in progress
        let deferred = this._indirections.get(condition);
        if (deferred) {
          // Unlock the blocker
          deferred.resolve();
        }
        return this._indirections.delete(condition);
      }

      // wait() is complete.
      return false;
    }.bind(this),
  };
}
Barrier.prototype = Object.freeze({
  /**
   * The current state of the barrier, as a JSON-serializable object
   * designed for error-reporting.
   */
  get state() {
    if (this._conditions) {
      return "Not started";
    }
    if (!this._monitors) {
      return "Complete";
    }
    let frozen = [];
    for (let {name, isComplete, fetchState, stack, filename, lineNumber} of this._monitors) {
      if (!isComplete) {
        frozen.push({name: name,
                     state: safeGetState(fetchState),
                     filename: filename,
                     lineNumber: lineNumber,
                     stack: stack});
      }
    }
    return frozen;
  },

  /**
   * Wait until all currently registered blockers are complete.
   *
   * Once this method has been called, any attempt to register a new blocker
   * for this barrier will cause an error.
   *
   * Successive calls to this method always return the same value.
   *
   * @param {object=}  options Optionally, an object  that may contain
   * the following fields:
   *  {number} warnAfterMS If provided and > 0, print a warning if the barrier
   *   has not been resolved after the given number of milliseconds.
   *  {number} crashAfterMS If provided and > 0, crash the process if the barrier
   *   has not been resolved after the give number of milliseconds (rounded up
   *   to the next second). To avoid crashing simply because the computer is busy
   *   or going to sleep, we actually wait for ceil(crashAfterMS/1000) successive
   *   periods of at least one second. Upon crashing, if a crash reporter is present,
   *   prepare a crash report with the state of this barrier.
   *
   *
   * @return {Promise} A promise satisfied once all blockers are complete.
   */
  wait: function(options = {}) {
    // This method only implements caching on top of _wait()
    if (this._promise) {
      return this._promise;
    }
    return this._promise = this._wait(options);
  },
  _wait: function(options) {
    let topic = this._name;
    let conditions = this._conditions;
    this._conditions = null; // Too late to register
    if (conditions.size == 0) {
      return Promise.resolve();
    }

    this._indirections = new Map();
    // The promises for which we are waiting.
    let allPromises = [];

    // Information to determine and report to the user which conditions
    // are not satisfied yet.
    this._monitors = [];

    for (let _condition of conditions.keys()) {
      for (let current of conditions.get(_condition)) {
        let condition = _condition; // Avoid capturing the wrong variable
        let {name, fetchState, stack, filename, lineNumber} = current;

        // An indirection on top of condition, used to let clients
        // cancel a blocker through removeBlocker.
        let indirection = Promise.defer();
        this._indirections.set(condition, indirection);

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

          let monitor = {
            isComplete: false,
            name: name,
            fetchState: fetchState,
            stack: stack,
            filename: filename,
            lineNumber: lineNumber
          };

	  condition = condition.then(null, function onError(error) {
            let msg = "A completion condition encountered an error" +
              " while we were spinning the event loop." +
	      " Condition: " + name +
              " Phase: " + topic +
              " State: " + safeGetState(fetchState);
	    warn(msg, error);

            // The error should remain uncaught, to ensure that it
            // still causes tests to fail.
            Promise.reject(error);
	  });
          condition.then(() => indirection.resolve());

          indirection.promise.then(() => monitor.isComplete = true);
          this._monitors.push(monitor);
          allPromises.push(indirection.promise);

        } catch (error) {
            let msg = "A completion condition encountered an error" +
                  " while we were initializing the phase." +
                  " Condition: " + name +
                  " Phase: " + topic +
                  " State: " + safeGetState(fetchState);
            warn(msg, error);
        }

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

    promise = promise.then(() => {
      this._monitors = null;
      this._indirections = null;
    }); // Memory cleanup


    // Now handle warnings and crashes

    let warnAfterMS = DELAY_WARNING_MS;
    if (options && "warnAfterMS" in options) {
      if (typeof options.warnAfterMS == "number"
         || options.warnAfterMS == null) {
        // Change the delay or deactivate warnAfterMS
        warnAfterMS = options.warnAfterMS;
      } else {
        throw new TypeError("Wrong option value for warnAfterMS");
      }
    }

    if (warnAfterMS && warnAfterMS > 0) {
      // If the promise takes too long to be resolved/rejected,
      // we need to notify the user.
      let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      timer.initWithCallback(function() {
        let msg = "At least one completion condition is taking too long to complete." +
	  " Conditions: " + JSON.stringify(this.state) +
	  " Barrier: " + topic;
        warn(msg);
      }.bind(this), warnAfterMS, Ci.nsITimer.TYPE_ONE_SHOT);

      promise = promise.then(function onSuccess() {
        timer.cancel();
        // As a side-effect, this prevents |timer| from
        // being garbage-collected too early.
      });
    }

    let crashAfterMS = DELAY_CRASH_MS;
    if (options && "crashAfterMS" in options) {
      if (typeof options.crashAfterMS == "number"
         || options.crashAfterMS == null) {
        // Change the delay or deactivate crashAfterMS
        crashAfterMS = options.crashAfterMS;
      } else {
        throw new TypeError("Wrong option value for crashAfterMS");
      }
    }

    if (crashAfterMS  > 0) {
      let timeToCrash = null;

      // If after |crashAfterMS| milliseconds (adjusted to take into
      // account sleep and otherwise busy computer) we have not finished
      // this shutdown phase, we assume that the shutdown is somehow
      // frozen, presumably deadlocked. At this stage, the only thing we
      // can do to avoid leaving the user's computer in an unstable (and
      // battery-sucking) situation is report the issue and crash.
      timeToCrash = looseTimer(crashAfterMS);
      timeToCrash.promise.then(
        function onTimeout() {
	  // Report the problem as best as we can, then crash.
	  let state = this.state;

          // If you change the following message, please make sure
          // that any information on the topic and state appears
          // within the first 200 characters of the message. This
          // helps automatically sort oranges.
          let msg = "AsyncShutdown timeout in " + topic +
            " Conditions: " + JSON.stringify(state) +
            " At least one completion condition failed to complete" +
	    " within a reasonable amount of time. Causing a crash to" +
	    " ensure that we do not leave the user with an unresponsive" +
	    " process draining resources.";
	  fatalerr(msg);
	  if (gCrashReporter && gCrashReporter.enabled) {
            let data = {
              phase: topic,
              conditions: state
	    };
            gCrashReporter.annotateCrashReport("AsyncShutdownTimeout",
              JSON.stringify(data));
	  } else {
            warn("No crash reporter available");
	  }

          // To help sorting out bugs, we want to make sure that the
          // call to nsIDebug.abort points to a guilty client, rather
          // than to AsyncShutdown itself. We search through all the
          // clients until we find one that is guilty and use its
          // filename/lineNumber, which have been determined during
          // the call to `addBlocker`.
          let filename = "?";
          let lineNumber = -1;
          for (let monitor of this._monitors) {
            if (monitor.isComplete) {
              continue;
            }
            filename = monitor.filename;
            lineNumber = monitor.lineNumber;
          }
	  gDebug.abort(filename, lineNumber);
        }.bind(this),
	  function onSatisfied() {
            // The promise has been rejected, which means that we have satisfied
            // all completion conditions.
          });

      promise = promise.then(function() {
        timeToCrash.reject();
      }/* No error is possible here*/);
    }

    return promise;
  },
});



// List of well-known phases
// Ideally, phases should be registered from the component that decides
// when they start/stop. For compatibility with existing startup/shutdown
// mechanisms, we register a few phases here.

this.AsyncShutdown.profileChangeTeardown = getPhase("profile-change-teardown");
this.AsyncShutdown.profileBeforeChange = getPhase("profile-before-change");
this.AsyncShutdown.sendTelemetry = getPhase("profile-before-change2");
this.AsyncShutdown.webWorkersShutdown = getPhase("web-workers-shutdown");

this.AsyncShutdown.Barrier = Barrier;

Object.freeze(this.AsyncShutdown);
