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

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
});
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gDebug",
  "@mozilla.org/xpcom/debug;1",
  "nsIDebug2"
);

// `true` if this is a content process, `false` otherwise.
// It would be nicer to go through `Services.appinfo`, but some tests need to be
// able to replace that field with a custom implementation before it is first
// called.
const isContent =
  // eslint-disable-next-line mozilla/use-services
  Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).processType ==
  Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;

// Display timeout warnings after 10 seconds
const DELAY_WARNING_MS = 10 * 1000;

// Crash the process if shutdown is really too long
// (allowing for sleep).
const PREF_DELAY_CRASH_MS = "toolkit.asyncshutdown.crash_timeout";
var DELAY_CRASH_MS = Services.prefs.getIntPref(PREF_DELAY_CRASH_MS, 60 * 1000); // One minute
Services.prefs.addObserver(PREF_DELAY_CRASH_MS, function () {
  DELAY_CRASH_MS = Services.prefs.getIntPref(PREF_DELAY_CRASH_MS);
});

/**
 * Any addBlocker calls that failed. We add this into barrier wait
 * crash annotations to help with debugging. When we fail to add
 * shutdown blockers that can break shutdown. We track these globally
 * rather than per barrier because one failure mode is when the
 * barrier has already finished by the time addBlocker is invoked -
 * but the failure to add the blocker may result in later barriers
 * waiting indefinitely, so the debug information is still useful
 * for those later barriers. See bug 1801674 for more context.
 */
let gBrokenAddBlockers = [];

/**
 * A set of Promise that supports waiting.
 *
 * Promise items may be added or removed during the wait. The wait will
 * resolve once all Promise items have been resolved or removed.
 */
function PromiseSet() {
  /**
   * key: the Promise passed pass the client of the `PromiseSet`.
   * value: an indirection on top of `key`, as an object with
   *   the following fields:
   *   - indirection: a Promise resolved if `key` is resolved or
   *     if `resolve` is called
   *   - resolve: a function used to resolve the indirection.
   */
  this._indirections = new Map();
  // Once all the tracked promises have been resolved we are done. Once Wait()
  // resolves, it should not be possible anymore to add further promises.
  // This covers for a possibly rare case, where something may try to add a
  // blocker after wait() is done, that would never be awaited for.
  this._done = false;
}
PromiseSet.prototype = {
  /**
   * Wait until all Promise have been resolved or removed.
   *
   * Note that calling `wait()` causes Promise to be removed from the
   * Set once they are resolved.
   * @param {function} onDoneCb invoked synchronously once all the entries
   * have been handled and no new entries will be accepted.
   * @return {Promise} Resolved once all Promise have been resolved or removed,
   * or rejected after at least one Promise has rejected.
   */
  wait(onDoneCb) {
    // Pick an arbitrary element in the map, if any exists.
    let entry = this._indirections.entries().next();
    if (entry.done) {
      // No indirections left, we are done.
      this._done = true;
      onDoneCb();
      return Promise.resolve();
    }

    let [, indirection] = entry.value;
    let promise = indirection.promise;
    promise = promise.then(() =>
      // At this stage, the entry has been cleaned up.
      this.wait(onDoneCb)
    );
    return promise;
  },

  /**
   * Add a new Promise to the set.
   *
   * Calls to wait (including ongoing calls) will only return once
   * `key` has either resolved or been removed.
   */
  add(key) {
    if (this._done) {
      throw new Error("Wait is complete, cannot add further promises.");
    }
    this._ensurePromise(key);
    let indirection = lazy.PromiseUtils.defer();
    key
      .then(
        x => {
          // Clean up immediately.
          // This needs to be done before the call to `resolve`, otherwise
          // `wait()` may loop forever.
          this._indirections.delete(key);
          indirection.resolve(x);
        },
        err => {
          this._indirections.delete(key);
          indirection.reject(err);
        }
      )
      .finally(() => {
        this._indirections.delete(key);
        // Normally the promise is resolved or rejected, but if its global
        // goes away, only finally may be invoked. In all the other cases this
        // is a no-op since the promise has been fulfilled already.
        indirection.reject(
          new Error("Promise not fulfilled, did it lost its global?")
        );
      });
    this._indirections.set(key, indirection);
  },

  /**
   * Remove a Promise from the set.
   *
   * Calls to wait (including ongoing calls) will ignore this promise,
   * unless it is added again.
   */
  delete(key) {
    this._ensurePromise(key);
    let value = this._indirections.get(key);
    if (!value) {
      return false;
    }
    this._indirections.delete(key);
    value.resolve();
    return true;
  },

  _ensurePromise(key) {
    if (!key || typeof key != "object") {
      throw new Error("Expected an object");
    }
    if (!("then" in key) || typeof key.then != "function") {
      throw new Error("Expected a Promise");
    }
  },
};

/**
 * Display a warning.
 *
 * As this code is generally used during shutdown, there are chances
 * that the UX will not be available to display warnings on the
 * console. We therefore use dump() rather than Cu.reportError().
 */
function log(msg, prefix = "", error = null) {
  try {
    dump(prefix + msg + "\n");
    if (error) {
      dump(prefix + error + "\n");
      if (typeof error == "object" && "stack" in error) {
        dump(prefix + error.stack + "\n");
      }
    }
  } catch (ex) {
    dump("INTERNAL ERROR in AsyncShutdown: cannot log message.\n");
  }
}
const PREF_DEBUG_LOG = "toolkit.asyncshutdown.log";
var DEBUG_LOG = Services.prefs.getBoolPref(PREF_DEBUG_LOG, false);
Services.prefs.addObserver(PREF_DEBUG_LOG, function () {
  DEBUG_LOG = Services.prefs.getBoolPref(PREF_DEBUG_LOG);
});

function debug(msg, error = null) {
  if (DEBUG_LOG) {
    log(msg, "DEBUG: ", error);
  }
}
function warn(msg, error = null) {
  log(msg, "WARNING: ", error);
}
function fatalerr(msg, error = null) {
  log(msg, "FATAL ERROR: ", error);
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
    let state = fetchState();
    if (!state) {
      return "(none)";
    }
    string = JSON.stringify(state);
    data = JSON.parse(string);
    // Simplify the rest of the code by ensuring that we can simply
    // concatenate the result to a message.
    if (data && typeof data == "object") {
      data.toString = function () {
        return string;
      };
    }
    return data;
  } catch (ex) {
    // Make sure that this causes test failures
    Promise.reject(ex);

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
  let deferred = lazy.PromiseUtils.defer();
  timer.initWithCallback(
    function () {
      if (beats <= 0) {
        deferred.resolve();
      }
      --beats;
    },
    DELAY_BEAT,
    Ci.nsITimer.TYPE_REPEATING_PRECISE_CAN_SKIP
  );
  // Ensure that the timer is both canceled once we are done with it
  // and not garbage-collected until then.
  deferred.promise.then(
    () => timer.cancel(),
    () => timer.cancel()
  );
  return deferred;
}

/**
 * Given an nsIStackFrame object, find the caller filename, line number,
 * and stack if necessary, and return them as an object.
 *
 * @param {nsIStackFrame} topFrame Top frame of the call stack.
 * @param {string} filename Pre-supplied filename or null if unknown.
 * @param {number} lineNumber Pre-supplied line number or null if unknown.
 * @param {string} stack Pre-supplied stack or null if unknown.
 *
 * @return object
 */
function getOrigin(topFrame, filename = null, lineNumber = null, stack = null) {
  try {
    // Determine the filename and line number of the caller.
    let frame = topFrame;

    for (; frame && frame.filename == topFrame.filename; frame = frame.caller) {
      // Climb up the stack
    }

    if (filename == null) {
      filename = frame ? frame.filename : "?";
    }
    if (lineNumber == null) {
      lineNumber = frame ? frame.lineNumber : 0;
    }
    if (stack == null) {
      // Now build the rest of the stack as a string, using Task.jsm's rewriting
      // to ensure that we do not lose information at each call to `Task.spawn`.
      stack = [];
      while (frame != null) {
        stack.push(frame.filename + ":" + frame.name + ":" + frame.lineNumber);
        frame = frame.caller;
      }
    }

    return {
      filename,
      lineNumber,
      stack,
    };
  } catch (ex) {
    return {
      filename: "<internal error: could not get origin>",
      lineNumber: -1,
      stack: "<internal error: could not get origin>",
    };
  }
}

/**
 * {string} topic -> phase
 */
var gPhases = new Map();

export var AsyncShutdown = {
  /**
   * Access function getPhase. For testing purposes only.
   */
  get _getPhase() {
    let accepted = Services.prefs.getBoolPref(
      "toolkit.asyncshutdown.testing",
      false
    );
    if (accepted) {
      return getPhase;
    }
    return undefined;
  },

  /**
   * This constant is used as the amount of milliseconds to allow shutdown to be
   * blocked until we crash the process forcibly and is read from the
   * 'toolkit.asyncshutdown.crash_timeout' pref.
   */
  get DELAY_CRASH_MS() {
    return DELAY_CRASH_MS;
  },
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
     * @param {object*} details Optionally, an object with details
     * that may be useful for error reporting, as a subset of of the following
     * fields:
     * - fetchState (strongly recommended) A function returning
     *    information about the current state of the blocker as an
     *    object. Used for providing more details when logging errors or
     *    crashing.
     * - stack. A string containing stack information. This module can
     *    generally infer stack information if it is not provided.
     * - lineNumber A number containing the line number for the caller.
     *    This module can generally infer this information if it is not
     *    provided.
     * - filename A string containing the filename for the caller. This
     *    module can generally infer  the information if it is not provided.
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
    addBlocker(name, condition, details = null) {
      spinner.addBlocker(name, condition, details);
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
    removeBlocker(condition) {
      return spinner.removeBlocker(condition);
    },

    get name() {
      return spinner.name;
    },

    get isClosed() {
      return spinner.isClosed;
    },

    /**
     * Trigger the phase without having to broadcast a
     * notification. For testing purposes only.
     */
    get _trigger() {
      let accepted = Services.prefs.getBoolPref(
        "toolkit.asyncshutdown.testing",
        false
      );
      if (accepted) {
        return () => spinner.observe();
      }
      return undefined;
    },
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
  this._barrier = new Barrier(topic);
  this._topic = topic;
  Services.obs.addObserver(this, topic);
}

Spinner.prototype = {
  /**
   * Register a new condition for this phase.
   *
   * See the documentation of `addBlocker` in property `client`
   * of instances of `Barrier`.
   */
  addBlocker(name, condition, details) {
    this._barrier.client.addBlocker(name, condition, details);
  },
  /**
   * Remove the blocker for a condition.
   *
   * See the documentation of `removeBlocker` in rpoperty `client`
   * of instances of `Barrier`
   *
   * @return {boolean} true if a blocker has been removed, false
   * otherwise. Note that a result of false may mean either that
   * the blocker has never been installed or that the phase has
   * completed and the blocker has already been resolved.
   */
  removeBlocker(condition) {
    return this._barrier.client.removeBlocker(condition);
  },

  get name() {
    return this._barrier.client.name;
  },

  get isClosed() {
    return this._barrier.client.isClosed;
  },

  // nsIObserver.observe
  observe() {
    let topic = this._topic;
    debug(`Starting phase ${topic}`);
    Services.obs.removeObserver(this, topic);

    // Setup the promise that will signal our phase's end.
    let isPhaseEnd = false;
    try {
      this._barrier
        .wait({
          warnAfterMS: DELAY_WARNING_MS,
          crashAfterMS: DELAY_CRASH_MS,
        })
        .finally(() => {
          isPhaseEnd = true;
        });
    } catch (ex) {
      debug("Error waiting for notification");
      throw ex;
    }

    // Now, spin the event loop. In case of a hang we will just crash without
    // ever leaving this loop.
    debug("Spinning the event loop");
    Services.tm.spinEventLoopUntil(
      `AsyncShutdown Spinner for ${topic}`,
      () => isPhaseEnd
    );

    debug(`Finished phase ${topic}`);
  },
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
  if (!name) {
    throw new TypeError("Instances of Barrier need a (non-empty) name");
  }

  /**
   * The set of all Promise for which we need to wait before the barrier
   * is lifted. Note that this set may be changed while we are waiting.
   *
   * Set to `null` once the wait is complete.
   */
  this._waitForMe = new PromiseSet();

  /**
   * A map from conditions, as passed by users during the call to `addBlocker`,
   * to `promise`, as present in `this._waitForMe`.
   *
   * Used to let users perform cleanup through `removeBlocker`.
   * Set to `null` once the wait is complete.
   *
   * Key: condition (any, as passed by user)
   * Value: promise used as a key in `this._waitForMe`. Note that there is
   * no guarantee that the key is still present in `this._waitForMe`.
   */
  this._conditionToPromise = new Map();

  /**
   * A map from Promise, as present in `this._waitForMe` or
   * `this._conditionToPromise`, to information on blockers.
   *
   * Key: Promise (as present in this._waitForMe or this._conditionToPromise).
   * Value:  {
   *  trigger: function,
   *  promise,
   *  name,
   *  fetchState: function,
   *  stack,
   *  filename,
   *  lineNumber
   * };
   */
  this._promiseToBlocker = new Map();

  /**
   * The name of the barrier.
   */
  if (typeof name != "string") {
    throw new TypeError("The name of the barrier must be a string");
  }
  this._name = name;

  /**
   * A cache for the promise returned by wait().
   */
  this._promise = null;

  /**
   * `true` once we have started waiting.
   */
  this._isStarted = false;

  /**
   * `true` once we're done and won't accept any new blockers.
   */
  this._isClosed = false;

  /**
   * The capability of adding blockers. This object may safely be returned
   * or passed to clients.
   */
  this.client = {
    /**
     * The name of the barrier owning this client.
     */
    get name() {
      return name;
    },

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
     * @param {object*} details Optionally, an object with details
     * that may be useful for error reporting, as a subset of of the following
     * fields:
     * - fetchState (strongly recommended) A function returning
     *    information about the current state of the blocker as an
     *    object. Used for providing more details when logging errors or
     *    crashing.
     * - stack. A string containing stack information. This module can
     *    generally infer stack information if it is not provided.
     * - lineNumber A number containing the line number for the caller.
     *    This module can generally infer this information if it is not
     *    provided.
     * - filename A string containing the filename for the caller. This
     *    module can generally infer  the information if it is not provided.
     */
    addBlocker: (name, condition, details) => {
      if (typeof name != "string") {
        gBrokenAddBlockers.push("No-name blocker");
        throw new TypeError("Expected a human-readable name as first argument");
      }
      if (details && typeof details == "function") {
        details = {
          fetchState: details,
        };
      } else if (!details) {
        details = {};
      }
      if (typeof details != "object") {
        gBrokenAddBlockers.push(`${name} - invalid details`);
        throw new TypeError(
          "Expected an object as third argument to `addBlocker`, got " + details
        );
      }
      if (!this._waitForMe) {
        gBrokenAddBlockers.push(`${name} - ${this._name} finished`);
        throw new Error(
          `Phase "${this._name}" is finished, it is too late to register completion condition "${name}"`
        );
      }
      debug(`Adding blocker ${name} for phase ${this._name}`);

      try {
        this.client._internalAddBlocker(name, condition, details);
      } catch (ex) {
        gBrokenAddBlockers.push(`${name} - ${ex.message}`);
        throw ex;
      }
    },

    _internalAddBlocker: (
      name,
      condition,
      { fetchState = null, filename = null, lineNumber = null, stack = null }
    ) => {
      if (fetchState != null && typeof fetchState != "function") {
        throw new TypeError("Expected a function for option `fetchState`");
      }

      // Split the condition between a trigger function and a promise.

      // The function to call to notify the blocker that we have started waiting.
      // This function returns a promise resolved/rejected once the
      // condition is complete, and never throws.
      let trigger;

      // A promise resolved once the condition is complete.
      let promise;
      if (typeof condition == "function") {
        promise = new Promise((resolve, reject) => {
          trigger = () => {
            try {
              resolve(condition());
            } catch (ex) {
              reject(ex);
            }
          };
        });
      } else {
        // If `condition` is not a function, `trigger` is not particularly
        // interesting, and `condition` needs to be normalized to a promise.
        trigger = () => {};
        promise = Promise.resolve(condition);
      }

      // Make sure that `promise` never rejects.
      promise = promise
        .catch(error => {
          let msg = `A blocker encountered an error while we were waiting.
          Blocker:  ${name}
          Phase: ${this._name}
          State: ${safeGetState(fetchState)}`;
          warn(msg, error);

          // The error should remain uncaught, to ensure that it
          // still causes tests to fail.
          Promise.reject(error);
        })
        // Added as a last line of defense, in case `warn`, `this._name` or
        // `safeGetState` somehow throws an error.
        .catch(() => {});

      let topFrame = null;
      if (filename == null || lineNumber == null || stack == null) {
        topFrame = Components.stack;
      }

      let blocker = {
        trigger,
        promise,
        name,
        fetchState,
        getOrigin: () => getOrigin(topFrame, filename, lineNumber, stack),
      };

      this._waitForMe.add(promise);
      this._promiseToBlocker.set(promise, blocker);
      this._conditionToPromise.set(condition, promise);

      // As conditions may hold lots of memory, we attempt to cleanup
      // as soon as we are done (which might be in the next tick, if
      // we have been passed a resolved promise).
      promise = promise.then(() => {
        debug(`Completed blocker ${name} for phase ${this._name}`);
        this._removeBlocker(condition);
      });

      if (this._isStarted) {
        // The wait has already started. The blocker should be
        // notified asap. We do it out of band as clients probably
        // expect `addBlocker` to return immediately.
        Promise.resolve().then(trigger);
      }
    },

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
    removeBlocker: condition => {
      return this._removeBlocker(condition);
    },
  };

  /**
   * Whether this client still accepts new blockers.
   */
  Object.defineProperty(this.client, "isClosed", {
    get: () => {
      return this._isClosed;
    },
  });
}
Barrier.prototype = Object.freeze({
  /**
   * The current state of the barrier, as a JSON-serializable object
   * designed for error-reporting.
   */
  get state() {
    if (!this._isStarted) {
      return "Not started";
    }
    if (!this._waitForMe) {
      return "Complete";
    }
    let frozen = [];
    for (let blocker of this._promiseToBlocker.values()) {
      let { name, fetchState } = blocker;
      let { stack, filename, lineNumber } = blocker.getOrigin();
      frozen.push({
        name,
        state: safeGetState(fetchState),
        filename,
        lineNumber,
        stack,
      });
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
  wait(options = {}) {
    // This method only implements caching on top of _wait()
    if (this._promise) {
      return this._promise;
    }
    return (this._promise = this._wait(options));
  },
  _wait(options) {
    // Sanity checks
    if (this._isStarted) {
      throw new TypeError("Internal error: already started " + this._name);
    }
    if (
      !this._waitForMe ||
      !this._conditionToPromise ||
      !this._promiseToBlocker
    ) {
      throw new TypeError("Internal error: already finished " + this._name);
    }

    let topic = this._name;

    // Notify blockers
    for (let blocker of this._promiseToBlocker.values()) {
      blocker.trigger(); // We have guarantees that this method will never throw
    }

    this._isStarted = true;

    // Now, wait
    let promise = this._waitForMe.wait(() => {
      this._isClosed = true;
    });

    promise = promise.catch(function onError(error) {
      // I don't think that this can happen.
      // However, let's be overcautious with async/shutdown error reporting.
      let msg =
        "An uncaught error appeared while completing the phase." +
        " Phase: " +
        topic;
      warn(msg, error);
    });

    promise = promise.then(() => {
      // Cleanup memory
      this._waitForMe = null;
      this._promiseToBlocker = null;
      this._conditionToPromise = null;
    });

    // Now handle warnings and crashes
    let warnAfterMS = DELAY_WARNING_MS;
    if (options && "warnAfterMS" in options) {
      if (
        typeof options.warnAfterMS == "number" ||
        options.warnAfterMS == null
      ) {
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
      timer.initWithCallback(
        () => {
          let msg =
            "At least one completion condition is taking too long to complete." +
            " Conditions: " +
            JSON.stringify(this.state) +
            " Barrier: " +
            topic;
          warn(msg);
        },
        warnAfterMS,
        Ci.nsITimer.TYPE_ONE_SHOT
      );

      promise = promise.then(function onSuccess() {
        timer.cancel();
        // As a side-effect, this prevents |timer| from
        // being garbage-collected too early.
      });
    }

    let crashAfterMS = DELAY_CRASH_MS;
    if (options && "crashAfterMS" in options) {
      if (
        typeof options.crashAfterMS == "number" ||
        options.crashAfterMS == null
      ) {
        // Change the delay or deactivate crashAfterMS
        crashAfterMS = options.crashAfterMS;
      } else {
        throw new TypeError("Wrong option value for crashAfterMS");
      }
    }

    if (crashAfterMS > 0) {
      let timeToCrash = null;

      // If after |crashAfterMS| milliseconds (adjusted to take into
      // account sleep and otherwise busy computer) we have not finished
      // this shutdown phase, we assume that the shutdown is somehow
      // frozen, presumably deadlocked. At this stage, the only thing we
      // can do to avoid leaving the user's computer in an unstable (and
      // battery-sucking) situation is report the issue and crash.
      timeToCrash = looseTimer(crashAfterMS);
      timeToCrash.promise.then(
        () => {
          // Report the problem as best as we can, then crash.
          let state = this.state;

          // If you change the following message, please make sure
          // that any information on the topic and state appears
          // within the first 200 characters of the message. This
          // helps automatically sort oranges.
          let msg =
            "AsyncShutdown timeout in " +
            topic +
            " Conditions: " +
            JSON.stringify(state) +
            " At least one completion condition failed to complete" +
            " within a reasonable amount of time. Causing a crash to" +
            " ensure that we do not leave the user with an unresponsive" +
            " process draining resources.";
          fatalerr(msg);
          if (gBrokenAddBlockers.length) {
            fatalerr(
              "Broken addBlocker calls: " + JSON.stringify(gBrokenAddBlockers)
            );
          }
          if (Services.appinfo.crashReporterEnabled) {
            Services.appinfo.annotateCrashReport(
              "AsyncShutdownTimeout",
              JSON.stringify(this._gatherCrashReportTimeoutData(topic, state))
            );
          } else {
            warn("No crash reporter available");
          }

          // To help sorting out bugs, we want to make sure that the
          // call to nsIDebug2.abort points to a guilty client, rather
          // than to AsyncShutdown itself. We pick a client that is
          // still blocking and use its filename/lineNumber,
          // which have been determined during the call to `addBlocker`.
          let filename = "?";
          let lineNumber = -1;
          for (let blocker of this._promiseToBlocker.values()) {
            ({ filename, lineNumber } = blocker.getOrigin());
            break;
          }
          lazy.gDebug.abort(filename, lineNumber);
        },
        function onSatisfied() {
          // The promise has been rejected, which means that we have satisfied
          // all completion conditions.
        }
      );

      promise = promise.then(
        function () {
          timeToCrash.reject();
        } /* No error is possible here*/
      );
    }

    return promise;
  },

  _gatherCrashReportTimeoutData(phase, conditions) {
    let data = { phase, conditions };
    if (gBrokenAddBlockers.length) {
      data.brokenAddBlockers = gBrokenAddBlockers;
    }
    return data;
  },

  _removeBlocker(condition) {
    if (
      !this._waitForMe ||
      !this._promiseToBlocker ||
      !this._conditionToPromise
    ) {
      // We have already cleaned up everything.
      return false;
    }

    let promise = this._conditionToPromise.get(condition);
    if (!promise) {
      // The blocker has already been removed
      return false;
    }
    this._conditionToPromise.delete(condition);
    this._promiseToBlocker.delete(promise);
    return this._waitForMe.delete(promise);
  },
});

// List of well-known phases
// Ideally, phases should be registered from the component that decides
// when they start/stop. For compatibility with existing startup/shutdown
// mechanisms, we register a few phases here.

// Parent process
if (!isContent) {
  AsyncShutdown.profileChangeTeardown = getPhase("profile-change-teardown");
  AsyncShutdown.profileBeforeChange = getPhase("profile-before-change");
  AsyncShutdown.sendTelemetry = getPhase("profile-before-change-telemetry");
}

// Notifications that fire in the parent and content process, but should
// only have phases in the parent process.
if (!isContent) {
  AsyncShutdown.quitApplicationGranted = getPhase("quit-application-granted");
}

// Don't add a barrier for content-child-shutdown because this
// makes it easier to cause shutdown hangs.

// All processes
AsyncShutdown.webWorkersShutdown = getPhase("web-workers-shutdown");
AsyncShutdown.xpcomWillShutdown = getPhase("xpcom-will-shutdown");

AsyncShutdown.Barrier = Barrier;

Object.freeze(AsyncShutdown);
