/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Helpers for async functions. Async functions are generator functions that are
 * run by Tasks. An async function returns a Promise for the resolution of the
 * function. When the function returns, the promise is resolved with the
 * returned value. If it throws the promise rejects with the thrown error.
 *
 * See Task documentation at https://developer.mozilla.org/en-US/docs/Mozilla/JavaScript_code_modules/Task.jsm.
 */

let {Cu} = require("chrome");
let {Task} = require("resource://gre/modules/Task.jsm");
let {Promise} = require("resource://gre/modules/Promise.jsm");

/**
 * Create an async function from a generator function.
 *
 * @param Function func
 *        The generator function that to wrap as an async function.
 * @return Function
 *         The async function.
 */
exports.async = function async(func) {
  return function(...args) {
    return Task.spawn(func.apply(this, args));
  };
};

/**
 * Create an async function that only executes once per instance of an object.
 * Once called on a given object, the same promise will be returned for any
 * future calls for that object.
 *
 * @param Function func
 *        The generator function that to wrap as an async function.
 * @return Function
 *         The async function.
 */
exports.asyncOnce = function asyncOnce(func) {
  const promises = new WeakMap();
  return function(...args) {
    let promise = promises.get(this);
    if (!promise) {
      promise = Task.spawn(func.apply(this, args));
      promises.set(this, promise);
    }
    return promise;
  };
};

/**
 * Adds an event listener to the given element, and then removes its event
 * listener once the event is called, returning the event object as a promise.
 * @param  nsIDOMElement element
 *         The DOM element to listen on
 * @param  String event
 *         The name of the event type to listen for
 * @param  Boolean useCapture
 *         Should we initiate the capture phase?
 * @return Promise
 *         The promise resolved with the event object when the event first
 *         happens
 */
exports.listenOnce = function listenOnce(element, event, useCapture) {
  return new Promise(function(resolve, reject) {
    var onEvent = function(ev) {
      element.removeEventListener(event, onEvent, useCapture);
      resolve(ev);
    }
    element.addEventListener(event, onEvent, useCapture);
  });
};

/**
 * Call a function that expects a callback as the last argument and returns a
 * promise for the result. This simplifies using callback APIs from tasks and
 * async functions.
 *
 * @param Any obj
 *        The |this| value to call the function on.
 * @param Function func
 *        The callback-expecting function to call.
 * @param Array args
 *        Additional arguments to pass to the method.
 * @return Promise
 *         The promise for the result. If the callback is called with only one
 *         argument, it is used as the resolution value. If there's multiple
 *         arguments, an array containing the arguments is the resolution value.
 *         If the method throws, the promise is rejected with the thrown value.
 */
function promisify(obj, func, args) {
  return new Promise(resolve => {
    args.push((...results) => {
      resolve(results.length > 1 ? results : results[0]);
    });
    func.apply(obj, args);
  });
}

/**
 * Call a method that expects a callback as the last argument and returns a
 * promise for the result.
 *
 * @see promisify
 */
exports.promiseInvoke = function promiseInvoke(obj, func, ...args) {
  return promisify(obj, func, args);
};

/**
 * Call a function that expects a callback as the last argument.
 *
 * @see promisify
 */
exports.promiseCall = function promiseCall(func, ...args) {
  return promisify(undefined, func, args);
};
