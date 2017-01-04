/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This implementation file is imported by the Promise.jsm module, and as a
 * special case by the debugger server.  To support chrome debugging, the
 * debugger server needs to have all its code in one global, so it must use
 * loadSubScript directly.
 *
 * In the general case, this script should be used by importing Promise.jsm:
 *
 * Components.utils.import("resource://gre/modules/Promise.jsm");
 *
 * More documentation can be found in the Promise.jsm module.
 */

// Globals

// Obtain an instance of Cu. How this instance is obtained depends on how this
// file is loaded.
//
// This file can be loaded in three different ways:
// 1. As a CommonJS module, by Loader.jsm, on the main thread.
// 2. As a CommonJS module, by worker-loader.js, on a worker thread.
// 3. As a subscript, by Promise.jsm, on the main thread.
//
// If require is defined, the file is loaded as a CommonJS module. Components
// will not be defined in that case, but we can obtain an instance of Cu from
// the chrome module. Otherwise, this file is loaded as a subscript, and we can
// obtain an instance of Cu from Components directly.
//
// If the file is loaded as a CommonJS module on a worker thread, the instance
// of Cu obtained from the chrome module will be null. The reason for this is
// that Components is not defined in worker threads, so no instance of Cu can
// be obtained.

var Cu = this.require ? require("chrome").Cu : Components.utils;
var Cc = this.require ? require("chrome").Cc : Components.classes;
var Ci = this.require ? require("chrome").Ci : Components.interfaces;
// If we can access Components, then we use it to capture an async
// parent stack trace; see scheduleWalkerLoop.  However, as it might
// not be available (see above), users of this must check it first.
var Components_ = this.require ? require("chrome").components : Components;

// If Cu is defined, use it to lazily define the FinalizationWitnessService.
if (Cu) {
  Cu.import("resource://gre/modules/Services.jsm");
  Cu.import("resource://gre/modules/XPCOMUtils.jsm");

  XPCOMUtils.defineLazyServiceGetter(this, "FinalizationWitnessService",
                                     "@mozilla.org/toolkit/finalizationwitness;1",
                                     "nsIFinalizationWitnessService");

  // For now, we're worried about add-ons using Promises with CPOWs, so we'll
  // permit them in this scope, but this support will go away soon.
  Cu.permitCPOWsInScope(this);
}

const STATUS_PENDING = 0;
const STATUS_RESOLVED = 1;
const STATUS_REJECTED = 2;

// This N_INTERNALS name allow internal properties of the Promise to be
// accessed only by this module, while still being visible on the object
// manually when using a debugger.  This doesn't strictly guarantee that the
// properties are inaccessible by other code, but provide enough protection to
// avoid using them by mistake.
const salt = Math.floor(Math.random() * 100);
const N_INTERNALS = "{private:internals:" + salt + "}";

// We use DOM Promise for scheduling the walker loop.
const DOMPromise = Cu ? Promise : null;

// Warn-upon-finalization mechanism
//
// One of the difficult problems with promises is locating uncaught
// rejections. We adopt the following strategy: if a promise is rejected
// at the time of its garbage-collection *and* if the promise is at the
// end of a promise chain (i.e. |thatPromise.then| has never been
// called), then we print a warning.
//
//  let deferred = Promise.defer();
//  let p = deferred.promise.then();
//  deferred.reject(new Error("I am un uncaught error"));
//  deferred = null;
//  p = null;
//
// In this snippet, since |deferred.promise| is not the last in the
// chain, no error will be reported for that promise. However, since
// |p| is the last promise in the chain, the error will be reported
// for |p|.
//
// Note that this may, in some cases, cause an error to be reported more
// than once. For instance, consider:
//
//   let deferred = Promise.defer();
//   let p1 = deferred.promise.then();
//   let p2 = deferred.promise.then();
//   deferred.reject(new Error("I am an uncaught error"));
//   p1 = p2 = deferred = null;
//
// In this snippet, the error is reported both by p1 and by p2.
//

var PendingErrors = {
  // An internal counter, used to generate unique id.
  _counter: 0,
  // Functions registered to be notified when a pending error
  // is reported as uncaught.
  _observers: new Set(),
  _map: new Map(),

  /**
   * Initialize PendingErrors
   */
  init() {
    Services.obs.addObserver(function observe(aSubject, aTopic, aValue) {
      PendingErrors.report(aValue);
    }, "promise-finalization-witness", false);
  },

  /**
   * Register an error as tracked.
   *
   * @return The unique identifier of the error.
   */
  register(error) {
    let id = "pending-error-" + (this._counter++);
    //
    // At this stage, ideally, we would like to store the error itself
    // and delay any treatment until we are certain that we will need
    // to report that error. However, in the (unlikely but possible)
    // case the error holds a reference to the promise itself, doing so
    // would prevent the promise from being garbabe-collected, which
    // would both cause a memory leak and ensure that we cannot report
    // the uncaught error.
    //
    // To avoid this situation, we rather extract relevant data from
    // the error and further normalize it to strings.
    //
    let value = {
      date: new Date(),
      message: "" + error,
      fileName: null,
      stack: null,
      lineNumber: null
    };
    try { // Defend against non-enumerable values
      if (error && error instanceof Ci.nsIException) {
        // nsIException does things a little differently.
        try {
          // For starters |.toString()| does not only contain the message, but
          // also the top stack frame, and we don't really want that.
          value.message = error.message;
        } catch (ex) {
          // Ignore field
        }
        try {
          // All lowercase filename. ;)
          value.fileName = error.filename;
        } catch (ex) {
          // Ignore field
        }
        try {
          value.lineNumber = error.lineNumber;
        } catch (ex) {
          // Ignore field
        }
      } else if (typeof error == "object" && error) {
        for (let k of ["fileName", "stack", "lineNumber"]) {
          try { // Defend against fallible getters and string conversions
            let v = error[k];
            value[k] = v ? ("" + v) : null;
          } catch (ex) {
            // Ignore field
          }
        }
      }

      if (!value.stack) {
        // |error| is not an Error (or Error-alike). Try to figure out the stack.
        let stack = null;
        if (error && error.location &&
            error.location instanceof Ci.nsIStackFrame) {
          // nsIException has full stack frames in the |.location| member.
          stack = error.location;
        } else {
          // Components.stack to the rescue!
          stack = Components_.stack;
          // Remove those top frames that refer to Promise.jsm.
          while (stack) {
            if (!stack.filename.endsWith("/Promise.jsm")) {
              break;
            }
            stack = stack.caller;
          }
        }
        if (stack) {
          let frames = [];
          while (stack) {
            frames.push(stack);
            stack = stack.caller;
          }
          value.stack = frames.join("\n");
        }
      }
    } catch (ex) {
      // Ignore value
    }
    this._map.set(id, value);
    return id;
  },

  /**
   * Notify all observers that a pending error is now uncaught.
   *
   * @param id The identifier of the pending error, as returned by
   * |register|.
   */
  report(id) {
    let value = this._map.get(id);
    if (!value) {
      return; // The error has already been reported
    }
    this._map.delete(id);
    for (let obs of this._observers.values()) {
      obs(value);
    }
  },

  /**
   * Mark all pending errors are uncaught, notify the observers.
   */
  flush() {
    // Since we are going to modify the map while walking it,
    // let's copying the keys first.
    for (let key of Array.from(this._map.keys())) {
      this.report(key);
    }
  },

  /**
   * Stop tracking an error, as this error has been caught,
   * eventually.
   */
  unregister(id) {
    this._map.delete(id);
  },

  /**
   * Add an observer notified when an error is reported as uncaught.
   *
   * @param {function} observer A function notified when an error is
   * reported as uncaught. Its arguments are
   *   {message, date, fileName, stack, lineNumber}
   * All arguments are optional.
   */
  addObserver(observer) {
    this._observers.add(observer);
  },

  /**
   * Remove an observer added with addObserver
   */
  removeObserver(observer) {
    this._observers.delete(observer);
  },

  /**
   * Remove all the observers added with addObserver
   */
  removeAllObservers() {
    this._observers.clear();
  }
};

// Initialize the warn-upon-finalization mechanism if and only if Cu is defined.
// Otherwise, FinalizationWitnessService won't be defined (see above).
if (Cu) {
  PendingErrors.init();
}

// Default mechanism for displaying errors
PendingErrors.addObserver(function(details) {
  const generalDescription = "A promise chain failed to handle a rejection." +
    " Did you forget to '.catch', or did you forget to 'return'?\nSee" +
    " https://developer.mozilla.org/Mozilla/JavaScript_code_modules/Promise.jsm/Promise\n\n";

  let error = Cc['@mozilla.org/scripterror;1'].createInstance(Ci.nsIScriptError);
  if (!error || !Services.console) {
    // Too late during shutdown to use the nsIConsole
    dump("*************************\n");
    dump(generalDescription);
    dump("On: " + details.date + "\n");
    dump("Full message: " + details.message + "\n");
    dump("Full stack: " + (details.stack || "not available") + "\n");
    dump("*************************\n");
    return;
  }
  let message = details.message;
  if (details.stack) {
    message += "\nFull Stack: " + details.stack;
  }
  error.init(
             /* message*/ generalDescription +
             "Date: " + details.date + "\nFull Message: " + message,
             /* sourceName*/ details.fileName,
             /* sourceLine*/ details.lineNumber ? ("" + details.lineNumber) : 0,
             /* lineNumber*/ details.lineNumber || 0,
             /* columnNumber*/ 0,
             /* flags*/ Ci.nsIScriptError.errorFlag,
             /* category*/ "chrome javascript");
  Services.console.logMessage(error);
});


// Additional warnings for developers
//
// The following error types are considered programmer errors, which should be
// reported (possibly redundantly) so as to let programmers fix their code.
const ERRORS_TO_REPORT = ["EvalError", "RangeError", "ReferenceError", "TypeError"];

// Promise

/**
 * The Promise constructor. Creates a new promise given an executor callback.
 * The executor callback is called with the resolve and reject handlers.
 *
 * @param aExecutor
 *        The callback that will be called with resolve and reject.
 */
this.Promise = function Promise(aExecutor) {
  if (typeof(aExecutor) != "function") {
    throw new TypeError("Promise constructor must be called with an executor.");
  }

  /*
   * Object holding all of our internal values we associate with the promise.
   */
  Object.defineProperty(this, N_INTERNALS, { value: {
    /*
     * Internal status of the promise.  This can be equal to STATUS_PENDING,
     * STATUS_RESOLVED, or STATUS_REJECTED.
     */
    status: STATUS_PENDING,

    /*
     * When the status property is STATUS_RESOLVED, this contains the final
     * resolution value, that cannot be a promise, because resolving with a
     * promise will cause its state to be eventually propagated instead.  When the
     * status property is STATUS_REJECTED, this contains the final rejection
     * reason, that could be a promise, even if this is uncommon.
     */
    value: undefined,

    /*
     * Array of Handler objects registered by the "then" method, and not processed
     * yet.  Handlers are removed when the promise is resolved or rejected.
     */
    handlers: [],

    /**
     * When the status property is STATUS_REJECTED and until there is
     * a rejection callback, this contains an array
     * - {string} id An id for use with |PendingErrors|;
     * - {FinalizationWitness} witness A witness broadcasting |id| on
     *   notification "promise-finalization-witness".
     */
    witness: undefined
  }});

  Object.seal(this);

  let resolve = PromiseWalker.completePromise
                             .bind(PromiseWalker, this, STATUS_RESOLVED);
  let reject = PromiseWalker.completePromise
                            .bind(PromiseWalker, this, STATUS_REJECTED);

  try {
    aExecutor.call(undefined, resolve, reject);
  } catch (ex) {
    reject(ex);
  }
}

/**
 * Calls one of the provided functions as soon as this promise is either
 * resolved or rejected.  A new promise is returned, whose state evolves
 * depending on this promise and the provided callback functions.
 *
 * The appropriate callback is always invoked after this method returns, even
 * if this promise is already resolved or rejected.  You can also call the
 * "then" method multiple times on the same promise, and the callbacks will be
 * invoked in the same order as they were registered.
 *
 * @param aOnResolve
 *        If the promise is resolved, this function is invoked with the
 *        resolution value of the promise as its only argument, and the
 *        outcome of the function determines the state of the new promise
 *        returned by the "then" method.  In case this parameter is not a
 *        function (usually "null"), the new promise returned by the "then"
 *        method is resolved with the same value as the original promise.
 *
 * @param aOnReject
 *        If the promise is rejected, this function is invoked with the
 *        rejection reason of the promise as its only argument, and the
 *        outcome of the function determines the state of the new promise
 *        returned by the "then" method.  In case this parameter is not a
 *        function (usually left "undefined"), the new promise returned by the
 *        "then" method is rejected with the same reason as the original
 *        promise.
 *
 * @return A new promise that is initially pending, then assumes a state that
 *         depends on the outcome of the invoked callback function:
 *          - If the callback returns a value that is not a promise, including
 *            "undefined", the new promise is resolved with this resolution
 *            value, even if the original promise was rejected.
 *          - If the callback throws an exception, the new promise is rejected
 *            with the exception as the rejection reason, even if the original
 *            promise was resolved.
 *          - If the callback returns a promise, the new promise will
 *            eventually assume the same state as the returned promise.
 *
 * @note If the aOnResolve callback throws an exception, the aOnReject
 *       callback is not invoked.  You can register a rejection callback on
 *       the returned promise instead, to process any exception occurred in
 *       either of the callbacks registered on this promise.
 */
Promise.prototype.then = function(aOnResolve, aOnReject) {
  let handler = new Handler(this, aOnResolve, aOnReject);
  this[N_INTERNALS].handlers.push(handler);

  // Ensure the handler is scheduled for processing if this promise is already
  // resolved or rejected.
  if (this[N_INTERNALS].status != STATUS_PENDING) {

    // This promise is not the last in the chain anymore. Remove any watchdog.
    if (this[N_INTERNALS].witness != null) {
      let [id, witness] = this[N_INTERNALS].witness;
      this[N_INTERNALS].witness = null;
      witness.forget();
      PendingErrors.unregister(id);
    }

    PromiseWalker.schedulePromise(this);
  }

  return handler.nextPromise;
};

/**
 * Invokes `promise.then` with undefined for the resolve handler and a given
 * reject handler.
 *
 * @param aOnReject
 *        The rejection handler.
 *
 * @return A new pending promise returned.
 *
 * @see Promise.prototype.then
 */
Promise.prototype.catch = function(aOnReject) {
  return this.then(undefined, aOnReject);
};

/**
 * Creates a new pending promise and provides methods to resolve or reject it.
 *
 * @return A new object, containing the new promise in the "promise" property,
 *         and the methods to change its state in the "resolve" and "reject"
 *         properties.  See the Deferred documentation for details.
 */
Promise.defer = function() {
  return new Deferred();
};

/**
 * Creates a new promise resolved with the specified value, or propagates the
 * state of an existing promise.
 *
 * @param aValue
 *        If this value is not a promise, including "undefined", it becomes
 *        the resolution value of the returned promise.  If this value is a
 *        promise, then the returned promise will eventually assume the same
 *        state as the provided promise.
 *
 * @return A promise that can be pending, resolved, or rejected.
 */
Promise.resolve = function(aValue) {
  if (aValue && typeof(aValue) == "function" && aValue.isAsyncFunction) {
    throw new TypeError(
      "Cannot resolve a promise with an async function. " +
      "You should either invoke the async function first " +
      "or use 'Task.spawn' instead of 'Task.async' to start " +
      "the Task and return its promise.");
  }

  if (aValue instanceof Promise) {
    return aValue;
  }

  return new Promise((aResolve) => aResolve(aValue));
};

/**
 * Creates a new promise rejected with the specified reason.
 *
 * @param aReason
 *        The rejection reason for the returned promise.  Although the reason
 *        can be "undefined", it is generally an Error object, like in
 *        exception handling.
 *
 * @return A rejected promise.
 *
 * @note The aReason argument should not be a promise.  Using a rejected
 *       promise for the value of aReason would make the rejection reason
 *       equal to the rejected promise itself, and not its rejection reason.
 */
Promise.reject = function(aReason) {
  return new Promise((_, aReject) => aReject(aReason));
};

/**
 * Returns a promise that is resolved or rejected when all values are
 * resolved or any is rejected.
 *
 * @param aValues
 *        Iterable of promises that may be pending, resolved, or rejected. When
 *        all are resolved or any is rejected, the returned promise will be
 *        resolved or rejected as well.
 *
 * @return A new promise that is fulfilled when all values are resolved or
 *         that is rejected when any of the values are rejected. Its
 *         resolution value will be an array of all resolved values in the
 *         given order, or undefined if aValues is an empty array. The reject
 *         reason will be forwarded from the first promise in the list of
 *         given promises to be rejected.
 */
Promise.all = function(aValues) {
  if (aValues == null || typeof(aValues[Symbol.iterator]) != "function") {
    throw new Error("Promise.all() expects an iterable.");
  }

  return new Promise((resolve, reject) => {
    let values = Array.isArray(aValues) ? aValues : [...aValues];
    let countdown = values.length;
    let resolutionValues = new Array(countdown);

    if (!countdown) {
      resolve(resolutionValues);
      return;
    }

    function checkForCompletion(aValue, aIndex) {
      resolutionValues[aIndex] = aValue;
      if (--countdown === 0) {
        resolve(resolutionValues);
      }
    }

    for (let i = 0; i < values.length; i++) {
      let index = i;
      let value = values[i];
      let resolver = val => checkForCompletion(val, index);

      if (value && typeof(value.then) == "function") {
        value.then(resolver, reject);
      } else {
        // Given value is not a promise, forward it as a resolution value.
        resolver(value);
      }
    }
  });
};

/**
 * Returns a promise that is resolved or rejected when the first value is
 * resolved or rejected, taking on the value or reason of that promise.
 *
 * @param aValues
 *        Iterable of values or promises that may be pending, resolved, or
 *        rejected. When any is resolved or rejected, the returned promise will
 *        be resolved or rejected as to the given value or reason.
 *
 * @return A new promise that is fulfilled when any values are resolved or
 *         rejected. Its resolution value will be forwarded from the resolution
 *         value or rejection reason.
 */
Promise.race = function(aValues) {
  if (aValues == null || typeof(aValues[Symbol.iterator]) != "function") {
    throw new Error("Promise.race() expects an iterable.");
  }

  return new Promise((resolve, reject) => {
    for (let value of aValues) {
      Promise.resolve(value).then(resolve, reject);
    }
  });
};

Promise.Debugging = {
  /**
   * Add an observer notified when an error is reported as uncaught.
   *
   * @param {function} observer A function notified when an error is
   * reported as uncaught. Its arguments are
   *   {message, date, fileName, stack, lineNumber}
   * All arguments are optional.
   */
  addUncaughtErrorObserver(observer) {
    PendingErrors.addObserver(observer);
  },

  /**
   * Remove an observer added with addUncaughtErrorObserver
   *
   * @param {function} An observer registered with
   * addUncaughtErrorObserver.
   */
  removeUncaughtErrorObserver(observer) {
    PendingErrors.removeObserver(observer);
  },

  /**
   * Remove all the observers added with addUncaughtErrorObserver
   */
  clearUncaughtErrorObservers() {
    PendingErrors.removeAllObservers();
  },

  /**
   * Force all pending errors to be reported immediately as uncaught.
   * Note that this may cause some false positives.
   */
  flushUncaughtErrors() {
    PendingErrors.flush();
  },
};
Object.freeze(Promise.Debugging);

Object.freeze(Promise);

// If module is defined, this file is loaded as a CommonJS module. Make sure
// Promise is exported in that case.
if (this.module) {
  module.exports = Promise;
}

// PromiseWalker

/**
 * This singleton object invokes the handlers registered on resolved and
 * rejected promises, ensuring that processing is not recursive and is done in
 * the same order as registration occurred on each promise.
 *
 * There is no guarantee on the order of execution of handlers registered on
 * different promises.
 */
this.PromiseWalker = {
  /**
   * Singleton array of all the unprocessed handlers currently registered on
   * resolved or rejected promises.  Handlers are removed from the array as soon
   * as they are processed.
   */
  handlers: [],

  /**
   * Called when a promise needs to change state to be resolved or rejected.
   *
   * @param aPromise
   *        Promise that needs to change state.  If this is already resolved or
   *        rejected, this method has no effect.
   * @param aStatus
   *        New desired status, either STATUS_RESOLVED or STATUS_REJECTED.
   * @param aValue
   *        Associated resolution value or rejection reason.
   */
  completePromise(aPromise, aStatus, aValue) {
    // Do nothing if the promise is already resolved or rejected.
    if (aPromise[N_INTERNALS].status != STATUS_PENDING) {
      return;
    }

    // Resolving with another promise will cause this promise to eventually
    // assume the state of the provided promise.
    if (aStatus == STATUS_RESOLVED && aValue &&
        typeof(aValue.then) == "function") {
      aValue.then(this.completePromise.bind(this, aPromise, STATUS_RESOLVED),
                  this.completePromise.bind(this, aPromise, STATUS_REJECTED));
      return;
    }

    // Change the promise status and schedule our handlers for processing.
    aPromise[N_INTERNALS].status = aStatus;
    aPromise[N_INTERNALS].value = aValue;
    if (aPromise[N_INTERNALS].handlers.length > 0) {
      this.schedulePromise(aPromise);
    } else if (Cu && aStatus == STATUS_REJECTED) {
      // This is a rejection and the promise is the last in the chain.
      // For the time being we therefore have an uncaught error.
      let id = PendingErrors.register(aValue);
      let witness =
          FinalizationWitnessService.make("promise-finalization-witness", id);
      aPromise[N_INTERNALS].witness = [id, witness];
    }
  },

  /**
   * Sets up the PromiseWalker loop to start on the next tick of the event loop
   */
  scheduleWalkerLoop() {
    this.walkerLoopScheduled = true;

    // If this file is loaded on a worker thread, DOMPromise will not behave as
    // expected: because native promises are not aware of nested event loops
    // created by the debugger, their respective handlers will not be called
    // until after leaving the nested event loop. The debugger server relies
    // heavily on the use promises, so this could cause the debugger to hang.
    //
    // To work around this problem, any use of native promises in the debugger
    // server should be avoided when it is running on a worker thread. Because
    // it is still necessary to be able to schedule runnables on the event
    // queue, the worker loader defines the function setImmediate as a
    // per-module global for this purpose.
    //
    // If Cu is defined, this file is loaded on the main thread. Otherwise, it
    // is loaded on the worker thread.
    if (Cu) {
      let stack = Components_ ? Components_.stack : null;
      if (stack) {
        DOMPromise.resolve().then(() => {
          Cu.callFunctionWithAsyncStack(this.walkerLoop.bind(this), stack,
                                        "Promise")
        });
      } else {
        DOMPromise.resolve().then(() => this.walkerLoop());
      }
    } else {
      setImmediate(this.walkerLoop);
    }
  },

  /**
   * Schedules the resolution or rejection handlers registered on the provided
   * promise for processing.
   *
   * @param aPromise
   *        Resolved or rejected promise whose handlers should be processed.  It
   *        is expected that this promise has at least one handler to process.
   */
  schedulePromise(aPromise) {
    // Migrate the handlers from the provided promise to the global list.
    for (let handler of aPromise[N_INTERNALS].handlers) {
      this.handlers.push(handler);
    }
    aPromise[N_INTERNALS].handlers.length = 0;

    // Schedule the walker loop on the next tick of the event loop.
    if (!this.walkerLoopScheduled) {
      this.scheduleWalkerLoop();
    }
  },

  /**
   * Indicates whether the walker loop is currently scheduled for execution on
   * the next tick of the event loop.
   */
  walkerLoopScheduled: false,

  /**
   * Processes all the known handlers during this tick of the event loop.  This
   * eager processing is done to avoid unnecessarily exiting and re-entering the
   * JavaScript context for each handler on a resolved or rejected promise.
   *
   * This function is called with "this" bound to the PromiseWalker object.
   */
  walkerLoop() {
    // If there is more than one handler waiting, reschedule the walker loop
    // immediately.  Otherwise, use walkerLoopScheduled to tell schedulePromise()
    // to reschedule the loop if it adds more handlers to the queue.  This makes
    // this walker resilient to the case where one handler does not return, but
    // starts a nested event loop.  In that case, the newly scheduled walker will
    // take over.  In the common case, the newly scheduled walker will be invoked
    // after this one has returned, with no actual handler to process.  This
    // small overhead is required to make nested event loops work correctly, but
    // occurs at most once per resolution chain, thus having only a minor
    // impact on overall performance.
    if (this.handlers.length > 1) {
      this.scheduleWalkerLoop();
    } else {
      this.walkerLoopScheduled = false;
    }

    // Process all the known handlers eagerly.
    while (this.handlers.length > 0) {
      this.handlers.shift().process();
    }
  },
};

// Bind the function to the singleton once.
PromiseWalker.walkerLoop = PromiseWalker.walkerLoop.bind(PromiseWalker);

// Deferred

/**
 * Returned by "Promise.defer" to provide a new promise along with methods to
 * change its state.
 */
function Deferred() {
  this.promise = new Promise((aResolve, aReject) => {
    this.resolve = aResolve;
    this.reject = aReject;
  });
  Object.freeze(this);
}

Deferred.prototype = {
  /**
   * A newly created promise, initially in the pending state.
   */
  promise: null,

  /**
   * Resolves the associated promise with the specified value, or propagates the
   * state of an existing promise.  If the associated promise has already been
   * resolved or rejected, this method does nothing.
   *
   * This function is bound to its associated promise when "Promise.defer" is
   * called, and can be called with any value of "this".
   *
   * @param aValue
   *        If this value is not a promise, including "undefined", it becomes
   *        the resolution value of the associated promise.  If this value is a
   *        promise, then the associated promise will eventually assume the same
   *        state as the provided promise.
   *
   * @note Calling this method with a pending promise as the aValue argument,
   *       and then calling it again with another value before the promise is
   *       resolved or rejected, has unspecified behavior and should be avoided.
   */
  resolve: null,

  /**
   * Rejects the associated promise with the specified reason.  If the promise
   * has already been resolved or rejected, this method does nothing.
   *
   * This function is bound to its associated promise when "Promise.defer" is
   * called, and can be called with any value of "this".
   *
   * @param aReason
   *        The rejection reason for the associated promise.  Although the
   *        reason can be "undefined", it is generally an Error object, like in
   *        exception handling.
   *
   * @note The aReason argument should not generally be a promise.  In fact,
   *       using a rejected promise for the value of aReason would make the
   *       rejection reason equal to the rejected promise itself, not to the
   *       rejection reason of the rejected promise.
   */
  reject: null,
};

// Handler

/**
 * Handler registered on a promise by the "then" function.
 */
function Handler(aThisPromise, aOnResolve, aOnReject) {
  this.thisPromise = aThisPromise;
  this.onResolve = aOnResolve;
  this.onReject = aOnReject;
  this.nextPromise = new Promise(() => {});
}

Handler.prototype = {
  /**
   * Promise on which the "then" method was called.
   */
  thisPromise: null,

  /**
   * Unmodified resolution handler provided to the "then" method.
   */
  onResolve: null,

  /**
   * Unmodified rejection handler provided to the "then" method.
   */
  onReject: null,

  /**
   * New promise that will be returned by the "then" method.
   */
  nextPromise: null,

  /**
   * Called after thisPromise is resolved or rejected, invokes the appropriate
   * callback and propagates the result to nextPromise.
   */
  process() {
    // The state of this promise is propagated unless a handler is defined.
    let nextStatus = this.thisPromise[N_INTERNALS].status;
    let nextValue = this.thisPromise[N_INTERNALS].value;

    try {
      // If a handler is defined for either resolution or rejection, invoke it
      // to determine the state of the next promise, that will be resolved with
      // the returned value, that can also be another promise.
      if (nextStatus == STATUS_RESOLVED) {
        if (typeof(this.onResolve) == "function") {
          nextValue = this.onResolve.call(undefined, nextValue);
        }
      } else if (typeof(this.onReject) == "function") {
        nextValue = this.onReject.call(undefined, nextValue);
        nextStatus = STATUS_RESOLVED;
      }
    } catch (ex) {

      // An exception has occurred in the handler.

      if (ex && typeof ex == "object" && "name" in ex &&
          ERRORS_TO_REPORT.indexOf(ex.name) != -1) {

        // We suspect that the exception is a programmer error, so we now
        // display it using dump().  Note that we do not use Cu.reportError as
        // we assume that this is a programming error, so we do not want end
        // users to see it. Also, if the programmer handles errors correctly,
        // they will either treat the error or log them somewhere.

        dump("*************************\n");
        dump("A coding exception was thrown in a Promise " +
             ((nextStatus == STATUS_RESOLVED) ? "resolution" : "rejection") +
             " callback.\n");
        dump("See https://developer.mozilla.org/Mozilla/JavaScript_code_modules/Promise.jsm/Promise\n\n");
        dump("Full message: " + ex + "\n");
        dump("Full stack: " + (("stack" in ex) ? ex.stack : "not available") + "\n");
        dump("*************************\n");

      }

      // Additionally, reject the next promise.
      nextStatus = STATUS_REJECTED;
      nextValue = ex;
    }

    // Propagate the newly determined state to the next promise.
    PromiseWalker.completePromise(this.nextPromise, nextStatus, nextValue);
  },
};
