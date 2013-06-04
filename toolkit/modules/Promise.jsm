/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Promise"
];

/**
 * This module implements the "promise" construct, according to the
 * "Promises/A+" proposal as known in April 2013, documented here:
 *
 * <http://promises-aplus.github.com/promises-spec/>
 *
 * A promise is an object representing a value that may not be available yet.
 * Internally, a promise can be in one of three states:
 *
 * - Pending, when the final value is not available yet.  This is the only state
 *   that may transition to one of the other two states.
 *
 * - Resolved, when and if the final value becomes available.  A resolution
 *   value becomes permanently associated with the promise.  This may be any
 *   value, including "undefined".
 *
 * - Rejected, if an error prevented the final value from being determined.  A
 *   rejection reason becomes permanently associated with the promise.  This may
 *   be any value, including "undefined", though it is generally an Error
 *   object, like in exception handling.
 *
 * A reference to an existing promise may be received by different means, for
 * example as the return value of a call into an asynchronous API.  In this
 * case, the state of the promise can be observed but not directly controlled.
 *
 * To observe the state of a promise, its "then" method must be used.  This
 * method registers callback functions that are called as soon as the promise is
 * either resolved or rejected.  The method returns a new promise, that in turn
 * is resolved or rejected depending on the state of the original promise and on
 * the behavior of the callbacks.  For example, unhandled exceptions in the
 * callbacks cause the new promise to be rejected, even if the original promise
 * is resolved.  See the documentation of the "then" method for details.
 *
 * Promises may also be created using the "Promise.defer" function, the main
 * entry point of this module.  The function, along with the new promise,
 * returns separate methods to change its state to be resolved or rejected.
 * See the documentation of the "Deferred" prototype for details.
 *
 * -----------------------------------------------------------------------------
 *
 * Cu.import("resource://gre/modules/Promise.jsm");
 *
 * // This function creates and returns a new promise.
 * function promiseValueAfterTimeout(aValue, aTimeout)
 * {
 *   let deferred = Promise.defer();
 *
 *   try {
 *     // An asynchronous operation will trigger the resolution of the promise.
 *     // In this example, we don't have a callback that triggers a rejection.
 *     do_timeout(aTimeout, function () {
 *       deferred.resolve(aValue);
 *     });
 *   } catch (ex) {
 *     // Generally, functions returning promises propagate exceptions through
 *     // the returned promise, though they may also choose to fail early.
 *     deferred.reject(ex);
 *   }
 *
 *   // We don't return the deferred to the caller, but only the contained
 *   // promise, so that the caller cannot accidentally change its state.
 *   return deferred.promise;
 * }
 *
 * // This code uses the promise returned be the function above.
 * let promise = promiseValueAfterTimeout("Value", 1000);
 *
 * let newPromise = promise.then(function onResolve(aValue) {
 *   do_print("Resolved with this value: " + aValue);
 * }, function onReject(aReason) {
 *   do_print("Rejected with this reason: " + aReason);
 * });
 *
 * // Unexpected errors should always be reported at the end of a promise chain.
 * newPromise.then(null, Components.utils.reportError);
 *
 * -----------------------------------------------------------------------------
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");

const STATUS_PENDING = 0;
const STATUS_RESOLVED = 1;
const STATUS_REJECTED = 2;

// These "private names" allow some properties of the Promise object to be
// accessed only by this module, while still being visible on the object
// manually when using a debugger.  They don't strictly guarantee that the
// properties are inaccessible by other code, but provide enough protection to
// avoid using them by mistake.
const salt = Math.floor(Math.random() * 100);
const Name = (n) => "{private:" + n + ":" + salt + "}";
const N_STATUS = Name("status");
const N_VALUE = Name("value");
const N_HANDLERS = Name("handlers");

////////////////////////////////////////////////////////////////////////////////
//// Promise

/**
 * This object provides the public module functions.
 */
this.Promise = Object.freeze({
  /**
   * Creates a new pending promise and provides methods to resolve or reject it.
   *
   * @return A new object, containing the new promise in the "promise" property,
   *         and the methods to change its state in the "resolve" and "reject"
   *         properties.  See the Deferred documentation for details.
   */
  defer: function ()
  {
    return new Deferred();
  },

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
  resolve: function (aValue)
  {
    let promise = new PromiseImpl();
    PromiseWalker.completePromise(promise, STATUS_RESOLVED, aValue);
    return promise;
  },

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
  reject: function (aReason)
  {
    let promise = new PromiseImpl();
    PromiseWalker.completePromise(promise, STATUS_REJECTED, aReason);
    return promise;
  },
});

////////////////////////////////////////////////////////////////////////////////
//// PromiseWalker

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
  completePromise: function (aPromise, aStatus, aValue)
  {
    // Do nothing if the promise is already resolved or rejected.
    if (aPromise[N_STATUS] != STATUS_PENDING) {
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
    aPromise[N_STATUS] = aStatus;
    aPromise[N_VALUE] = aValue;
    if (aPromise[N_HANDLERS].length > 0) {
      this.schedulePromise(aPromise);
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
  schedulePromise: function (aPromise)
  {
    // Migrate the handlers from the provided promise to the global list.
    for (let handler of aPromise[N_HANDLERS]) {
      this.handlers.push(handler);
    }
    aPromise[N_HANDLERS].length = 0;

    // Schedule the walker loop on the next tick of the event loop.
    if (!this.walkerLoopScheduled) {
      this.walkerLoopScheduled = true;
      Services.tm.currentThread.dispatch(this.walkerLoop,
                                         Ci.nsIThread.DISPATCH_NORMAL);
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
  walkerLoop: function ()
  {
    // Allow rescheduling the walker loop immediately.  This makes this walker
    // resilient to the case where one handler does not return, but starts a
    // nested event loop.  In that case, the newly scheduled walker will take
    // over.  In the common case, the newly scheduled walker will be invoked
    // after this one has returned, with no actual handler to process.  This
    // small overhead is required to make nested event loops work correctly, but
    // occurs at most once per resolution chain, thus having only a minor
    // impact on overall performance.
    this.walkerLoopScheduled = false;

    // Process all the known handlers eagerly.
    while (this.handlers.length > 0) {
      this.handlers.shift().process();
    }
  },
};

// Bind the function to the singleton once.
PromiseWalker.walkerLoop = PromiseWalker.walkerLoop.bind(PromiseWalker);

////////////////////////////////////////////////////////////////////////////////
//// Deferred

/**
 * Returned by "Promise.defer" to provide a new promise along with methods to
 * change its state.
 */
function Deferred()
{
  this.promise = new PromiseImpl();
  this.resolve = this.resolve.bind(this);
  this.reject = this.reject.bind(this);

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
  resolve: function (aValue) {
    PromiseWalker.completePromise(this.promise, STATUS_RESOLVED, aValue);
  },

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
  reject: function (aReason) {
    PromiseWalker.completePromise(this.promise, STATUS_REJECTED, aReason);
  },
};

////////////////////////////////////////////////////////////////////////////////
//// PromiseImpl

/**
 * The promise object implementation.  This includes the public "then" method,
 * as well as private state properties.
 */
function PromiseImpl()
{
  /*
   * Internal status of the promise.  This can be equal to STATUS_PENDING,
   * STATUS_RESOLVED, or STATUS_REJECTED.
   */
  Object.defineProperty(this, N_STATUS, { value: STATUS_PENDING,
                                          writable: true });

  /*
   * When the N_STATUS property is STATUS_RESOLVED, this contains the final
   * resolution value, that cannot be a promise, because resolving with a
   * promise will cause its state to be eventually propagated instead.  When the
   * N_STATUS property is STATUS_REJECTED, this contains the final rejection
   * reason, that could be a promise, even if this is uncommon.
   */
  Object.defineProperty(this, N_VALUE, { writable: true });

  /*
   * Array of Handler objects registered by the "then" method, and not processed
   * yet.  Handlers are removed when the promise is resolved or rejected.
   */
  Object.defineProperty(this, N_HANDLERS, { value: [] });

  Object.seal(this);
}

PromiseImpl.prototype = {
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
  then: function (aOnResolve, aOnReject)
  {
    let handler = new Handler(this, aOnResolve, aOnReject);
    this[N_HANDLERS].push(handler);

    // Ensure the handler is scheduled for processing if this promise is already
    // resolved or rejected.
    if (this[N_STATUS] != STATUS_PENDING) {
      PromiseWalker.schedulePromise(this);
    }

    return handler.nextPromise;
  },
};

////////////////////////////////////////////////////////////////////////////////
//// Handler

/**
 * Handler registered on a promise by the "then" function.
 */
function Handler(aThisPromise, aOnResolve, aOnReject)
{
  this.thisPromise = aThisPromise;
  this.onResolve = aOnResolve;
  this.onReject = aOnReject;
  this.nextPromise = new PromiseImpl();
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
  process: function()
  {
    // The state of this promise is propagated unless a handler is defined.
    let nextStatus = this.thisPromise[N_STATUS];
    let nextValue = this.thisPromise[N_VALUE];

    try {
      // If a handler is defined for either resolution or rejection, invoke it
      // to determine the state of the next promise, that will be resolved with
      // the returned value, that can also be another promise.
      if (nextStatus == STATUS_RESOLVED) {
        if (typeof(this.onResolve) == "function") {
          nextValue = this.onResolve(nextValue);
        }
      } else if (typeof(this.onReject) == "function") {
        nextValue = this.onReject(nextValue);
        nextStatus = STATUS_RESOLVED;
      }
    } catch (ex) {
      // If an exception occurred in the handler, reject the next promise.
      nextStatus = STATUS_REJECTED;
      nextValue = ex;
    }

    // Propagate the newly determined state to the next promise.
    PromiseWalker.completePromise(this.nextPromise, nextStatus, nextValue);
  },
};
