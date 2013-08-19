/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "Task"
];

/**
 * This module implements a subset of "Task.js" <http://taskjs.org/>.
 *
 * Paraphrasing from the Task.js site, tasks make sequential, asynchronous
 * operations simple, using the power of JavaScript's "yield" operator.
 *
 * Tasks are built upon generator functions and promises, documented here:
 *
 * <https://developer.mozilla.org/en/JavaScript/Guide/Iterators_and_Generators>
 * <http://wiki.commonjs.org/wiki/Promises/A>
 *
 * The "Task.spawn" function takes a generator function and starts running it as
 * a task.  Every time the task yields a promise, it waits until the promise is
 * fulfilled.  "Task.spawn" returns a promise that is resolved when the task
 * completes successfully, or is rejected if an exception occurs.
 *
 * -----------------------------------------------------------------------------
 *
 * Cu.import("resource://gre/modules/Task.jsm");
 *
 * Task.spawn(function () {
 *
 *   // This is our task.  It is a generator function because it contains the
 *   // "yield" operator at least once.  Let's create a promise object, wait on
 *   // it and capture its resolution value.
 *   let myPromise = getPromiseResolvedOnTimeoutWithValue(1000, "Value");
 *   let result = yield myPromise;
 *
 *   // This part is executed only after the promise above is fulfilled (after
 *   // one second, in this imaginary example).  We can easily loop while
 *   // calling asynchronous functions, and wait multiple times.
 *   for (let i = 0; i < 3; i++) {
 *     result += yield getPromiseResolvedOnTimeoutWithValue(50, "!");
 *   }
 *
 *   // Optionally, a value can be returned using this special exception
 *   // (because "return" cannot communicate a result in generator functions).
 *   throw new Task.Result("Resolution result for the task: " + result);
 *
 * }).then(function (result) {
 *
 *   // result == "Resolution result for the task: Value!!!"
 *
 *   // The result is undefined if no special Task.Result exception was thrown.
 *
 * }, function (exception) {
 *
 *   // Failure!  We can inspect or report the exception.
 *
 * });
 *
 * -----------------------------------------------------------------------------
 *
 * This module implements only the "Task.js" interfaces described above, with no
 * additional features to control the task externally, or do custom scheduling.
 * It also provides the following extensions that simplify task usage in the
 * most common cases:
 *
 * - The "Task.spawn" function also accepts an iterator returned by a generator
 *   function, in addition to a generator function.  This way, you can call into
 *   the generator function with the parameters you want, and with "this" bound
 *   to the correct value.  Also, "this" is never bound to the task object when
 *   "Task.spawn" calls the generator function.
 *
 * - In addition to a promise object, a task can yield the iterator returned by
 *   a generator function.  The iterator is turned into a task automatically.
 *   This reduces the syntax overhead of calling "Task.spawn" explicitly when
 *   you want to recurse into other task functions.
 *
 * - The "Task.spawn" function also accepts a primitive value, or a function
 *   returning a primitive value, and treats the value as the result of the
 *   task.  This makes it possible to call an externally provided function and
 *   spawn a task from it, regardless of whether it is an asynchronous generator
 *   or a synchronous function.  This comes in handy when iterating over
 *   function lists where some items have been converted to tasks and some not.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js");

////////////////////////////////////////////////////////////////////////////////
//// Task

/**
 * This object provides the public module functions.
 */
this.Task = {
  /**
   * Creates and starts a new task.
   *
   * @param aTask
   *        - If you specify a generator function, it is called with no
   *          arguments to retrieve the associated iterator.  The generator
   *          function is a task, that is can yield promise objects to wait
   *          upon.
   *        - If you specify the iterator returned by a generator function you
   *          called, the generator function is also executed as a task.  This
   *          allows you to call the function with arguments.
   *        - If you specify a function that is not a generator, it is called
   *          with no arguments, and its return value is used to resolve the
   *          returned promise.
   *        - If you specify anything else, you get a promise that is already
   *          resolved with the specified value.
   *
   * @return A promise object where you can register completion callbacks to be
   *         called when the task terminates.
   */
  spawn: function Task_spawn(aTask) {
    if (aTask && typeof(aTask) == "function") {
      try {
        // Let's call into the function ourselves.
        aTask = aTask();
      } catch (ex if ex instanceof Task.Result) {
        return Promise.resolve(ex.value);
      } catch (ex) {
        return Promise.reject(ex);
      }
    }

    if (aTask && typeof(aTask.send) == "function") {
      // This is an iterator resulting from calling a generator function.
      return new TaskImpl(aTask).deferred.promise;
    }

    // Just propagate the given value to the caller as a resolved promise.
    return Promise.resolve(aTask);
  },

  /**
   * Constructs a special exception that, when thrown inside a generator
   * function, allows the associated task to be resolved with a specific value.
   *
   * Example: throw new Task.Result("Value");
   */
  Result: function Task_Result(aValue) {
    this.value = aValue;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// TaskImpl

/**
 * Executes the specified iterator as a task, and gives access to the promise
 * that is fulfilled when the task terminates.
 */
function TaskImpl(iterator) {
  this.deferred = Promise.defer();
  this._iterator = iterator;
  this._run(true);
}

TaskImpl.prototype = {
  /**
   * Includes the promise object where task completion callbacks are registered,
   * and methods to resolve or reject the promise at task completion.
   */
  deferred: null,

  /**
   * The iterator returned by the generator function associated with this task.
   */
  _iterator: null,

  /**
   * Main execution routine, that calls into the generator function.
   *
   * @param aSendResolved
   *        If true, indicates that we should continue into the generator
   *        function regularly (if we were waiting on a promise, it was
   *        resolved). If true, indicates that we should cause an exception to
   *        be thrown into the generator function (if we were waiting on a
   *        promise, it was rejected).
   * @param aSendValue
   *        Resolution result or rejection exception, if any.
   */
  _run: function TaskImpl_run(aSendResolved, aSendValue) {
    try {
      let yielded = aSendResolved ? this._iterator.send(aSendValue)
                                  : this._iterator.throw(aSendValue);

      // If our task yielded an iterator resulting from calling another
      // generator function, automatically spawn a task from it, effectively
      // turning it into a promise that is fulfilled on task completion.
      if (yielded && typeof(yielded.send) == "function") {
        yielded = Task.spawn(yielded);
      }

      if (yielded && typeof(yielded.then) == "function") {
        // We have a promise object now. When fulfilled, call again into this
        // function to continue the task, with either a resolution or rejection
        // condition.
        yielded.then(this._run.bind(this, true),
                     this._run.bind(this, false));
      } else {
        // If our task yielded a value that is not a promise, just continue and
        // pass it directly as the result of the yield statement.
        this._run(true, yielded);
      }

    } catch (ex if ex instanceof Task.Result) {
      // The generator function threw the special exception that allows it to
      // return a specific value on resolution.
      this.deferred.resolve(ex.value);
    } catch (ex if ex instanceof StopIteration) {
      // The generator function terminated with no specific result.
      this.deferred.resolve();
    } catch (ex) {
      // The generator function failed with an uncaught exception.
      this.deferred.reject(ex);
    }
  }
};
