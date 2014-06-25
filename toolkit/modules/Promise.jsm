/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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

// These constants must be defined on the "this" object for them to be visible
// by subscripts in B2G, since "this" does not match the global scope.
this.Cc = Components.classes;
this.Ci = Components.interfaces;
this.Cu = Components.utils;
this.Cr = Components.results;

this.Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(this.Ci.mozIJSSubScriptLoader)
    .loadSubScript("resource://gre/modules/Promise-backend.js", this);
