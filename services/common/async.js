/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["Async"];

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

// Constants for makeSyncCallback, waitForSyncCallback.
const CB_READY = {};
const CB_COMPLETE = {};
const CB_FAIL = {};

const REASON_ERROR = Ci.mozIStorageStatementCallback.REASON_ERROR;

Cu.import("resource://gre/modules/Services.jsm");

/*
 * Helpers for various async operations.
 */
this.Async = {

  /**
   * Execute an arbitrary number of asynchronous functions one after the
   * other, passing the callback arguments on to the next one.  All functions
   * must take a callback function as their last argument.  The 'this' object
   * will be whatever chain()'s is.
   *
   * @usage this._chain = Async.chain;
   *        this._chain(this.foo, this.bar, this.baz)(args, for, foo)
   *
   * This is equivalent to:
   *
   *   let self = this;
   *   self.foo(args, for, foo, function (bars, args) {
   *     self.bar(bars, args, function (baz, params) {
   *       self.baz(baz, params);
   *     });
   *   });
   */
  chain: function chain() {
    let funcs = Array.slice(arguments);
    let thisObj = this;
    return function callback() {
      if (funcs.length) {
        let args = Array.slice(arguments).concat(callback);
        let f = funcs.shift();
        f.apply(thisObj, args);
      }
    };
  },

  /**
   * Helpers for making asynchronous calls within a synchronous API possible.
   *
   * If you value your sanity, do not look closely at the following functions.
   */

  /**
   * Create a sync callback that remembers state, in particular whether it has
   * been called.
   * The returned callback can be called directly passing an optional arg which
   * will be returned by waitForSyncCallback().  The callback also has a
   * .throw() method, which takes an error object and will cause
   * waitForSyncCallback to fail with the error object thrown as an exception
   * (but note that the .throw method *does not* itself throw - it just causes
   * the wait function to throw).
   */
  makeSyncCallback: function makeSyncCallback() {
    // The main callback remembers the value it was passed, and that it got data.
    let onComplete = function onComplete(data) {
      onComplete.state = CB_COMPLETE;
      onComplete.value = data;
    };

    // Initialize private callback data in preparation for being called.
    onComplete.state = CB_READY;
    onComplete.value = null;

    // Allow an alternate callback to trigger an exception to be thrown.
    onComplete.throw = function onComplete_throw(data) {
      onComplete.state = CB_FAIL;
      onComplete.value = data;
    };

    return onComplete;
  },

  /**
   * Wait for a sync callback to finish.
   */
  waitForSyncCallback: function waitForSyncCallback(callback) {
    // Grab the current thread so we can make it give up priority.
    let thread = Cc["@mozilla.org/thread-manager;1"].getService().currentThread;

    // Keep waiting until our callback is triggered (unless the app is quitting).
    while (Async.checkAppReady() && callback.state == CB_READY) {
      thread.processNextEvent(true);
    }

    // Reset the state of the callback to prepare for another call.
    let state = callback.state;
    callback.state = CB_READY;

    // Throw the value the callback decided to fail with.
    if (state == CB_FAIL) {
      throw callback.value;
    }

    // Return the value passed to the callback.
    return callback.value;
  },

  /**
   * Check if the app is still ready (not quitting).
   */
  checkAppReady: function checkAppReady() {
    // Watch for app-quit notification to stop any sync calls
    Services.obs.addObserver(function onQuitApplication() {
      Services.obs.removeObserver(onQuitApplication, "quit-application");
      Async.checkAppReady = function() {
        let exception = Components.Exception("App. Quitting", Cr.NS_ERROR_ABORT);
        exception.appIsShuttingDown = true;
        throw exception;
      };
    }, "quit-application", false);
    // In the common case, checkAppReady just returns true
    return (Async.checkAppReady = function() { return true; })();
  },

  /**
   * Check if the passed exception is one raised by checkAppReady. Typically
   * this will be used in exception handlers to allow such exceptions to
   * make their way to the top frame and allow the app to actually terminate.
   */
  isShutdownException(exception) {
    return exception && exception.appIsShuttingDown === true;
  },

  /**
   * Return the two things you need to make an asynchronous call synchronous
   * by spinning the event loop.
   */
  makeSpinningCallback: function makeSpinningCallback() {
    let cb = Async.makeSyncCallback();
    function callback(error, ret) {
      if (error)
        cb.throw(error);
      else
        cb(ret);
    }
    callback.wait = () => Async.waitForSyncCallback(cb);
    return callback;
  },

  // Prototype for mozIStorageCallback, used in querySpinningly.
  // This allows us to define the handle* functions just once rather
  // than on every querySpinningly invocation.
  _storageCallbackPrototype: {
    results: null,

    // These are set by queryAsync.
    names: null,
    syncCb: null,

    handleResult: function handleResult(results) {
      if (!this.names) {
        return;
      }
      if (!this.results) {
        this.results = [];
      }
      let row;
      while ((row = results.getNextRow()) != null) {
        let item = {};
        for (let name of this.names) {
          item[name] = row.getResultByName(name);
        }
        this.results.push(item);
      }
    },
    handleError: function handleError(error) {
      this.syncCb.throw(error);
    },
    handleCompletion: function handleCompletion(reason) {

      // If we got an error, handleError will also have been called, so don't
      // call the callback! We never cancel statements, so we don't need to
      // address that quandary.
      if (reason == REASON_ERROR)
        return;

      // If we were called with column names but didn't find any results,
      // the calling code probably still expects an array as a return value.
      if (this.names && !this.results) {
        this.results = [];
      }
      this.syncCb(this.results);
    }
  },

  querySpinningly: function querySpinningly(query, names) {
    // 'Synchronously' asyncExecute, fetching all results by name.
    let storageCallback = Object.create(Async._storageCallbackPrototype);
    storageCallback.names = names;
    storageCallback.syncCb = Async.makeSyncCallback();
    query.executeAsync(storageCallback);
    return Async.waitForSyncCallback(storageCallback.syncCb);
  },

  promiseSpinningly(promise) {
    let cb = Async.makeSpinningCallback();
    promise.then(result =>  {
      cb(null, result);
    }, err => {
      cb(err || new Error("Promise rejected without explicit error"));
    });
    return cb.wait();
  },
};
