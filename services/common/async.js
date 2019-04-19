/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Async"];

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

/*
 * Helpers for various async operations.
 */
var Async = {

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
  chain: function chain(...funcs) {
    let thisObj = this;
    return function callback() {
      if (funcs.length) {
        let args = [...arguments, callback];
        let f = funcs.shift();
        f.apply(thisObj, args);
      }
    };
  },

  /**
   * Check if the app is still ready (not quitting). Returns true, or throws an
   * exception if not ready.
   */
  checkAppReady: function checkAppReady() {
    // Watch for app-quit notification to stop any sync calls
    Services.obs.addObserver(function onQuitApplication() {
      Services.obs.removeObserver(onQuitApplication, "quit-application");
      Async.checkAppReady = Async.promiseYield = function() {
        let exception = Components.Exception("App. Quitting", Cr.NS_ERROR_ABORT);
        exception.appIsShuttingDown = true;
        throw exception;
      };
    }, "quit-application");
    // In the common case, checkAppReady just returns true
    return (Async.checkAppReady = function() { return true; })();
  },

  /**
   * Check if the app is still ready (not quitting). Returns true if the app
   * is ready, or false if it is being shut down.
   */
  isAppReady() {
    try {
      return Async.checkAppReady();
    } catch (ex) {
      if (!Async.isShutdownException(ex)) {
        throw ex;
      }
    }
    return false;
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
   * A "tight loop" of promises can still lock up the browser for some time.
   * Periodically waiting for a promise returned by this function will solve
   * that.
   * You should probably not use this method directly and instead use jankYielder
   * below.
   * Some reference here:
   * - https://gist.github.com/jesstelford/bbb30b983bddaa6e5fef2eb867d37678
   * - https://bugzilla.mozilla.org/show_bug.cgi?id=1094248
   */
  promiseYield() {
    return new Promise(resolve => {
      Services.tm.currentThread.dispatch(resolve, Ci.nsIThread.DISPATCH_NORMAL);
    });
  },

  /**
   * Shared state for yielding every N calls.
   *
   * Can be passed to multiple Async.yieldingForEach to have them overall yield
   * every N iterations.
   */
  yieldState(yieldEvery = 50) {
    let iterations = 0;

    return {
      shouldYield() {
        ++iterations;
        return iterations % yieldEvery === 0;
      },
    };
  },

  /**
   * Apply the given function to each element of the iterable, yielding the
   * event loop every yieldEvery iterations.
   *
   * @param iterable {Iterable}
   *        The iterable or iterator to iterate through.
   *
   * @param fn {(*) -> void|boolean}
   *        The function to be called on each element of the iterable.
   *
   *        Returning true from the function will stop the iteration.
   *
   * @param [yieldEvery = 50] {number|object}
   *        The number of iterations to complete before yielding back to the event
   *        loop.
   *
   * @return {boolean}
   *         Whether or not the function returned early.
   */
  async yieldingForEach(iterable, fn, yieldEvery = 50) {
    const yieldState = typeof yieldEvery === "number" ? Async.yieldState(yieldEvery) : yieldEvery;
    let iteration = 0;

    for (const item of iterable) {
      let result = fn(item, iteration++);
      if (typeof result !== "undefined" && typeof result.then !== "undefined") {
        // If we await result when it is not a Promise, we create an
        // automatically resolved promise, which is exactly the case that we
        // are trying to avoid.
        result = await result;
      }

      if (result === true) {
        return true;
      }

      if (yieldState.shouldYield()) {
        await Async.promiseYield();
        Async.checkAppReady();
      }
    }

    return false;
  },

  asyncQueueCaller(log) {
    return new AsyncQueueCaller(log);
  },

  asyncObserver(log, obj) {
    return new AsyncObserver(log, obj);
  },
};

/**
 * Allows consumers to enqueue asynchronous callbacks to be called in order.
 * Typically this is used when providing a callback to a caller that doesn't
 * await on promises.
 */
class AsyncQueueCaller {
  constructor(log) {
    this._log = log;
    this._queue = Promise.resolve();
    this.QueryInterface = ChromeUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]);
  }

  /**
   * /!\ Never await on another function that calls enqueueCall /!\
   *     on the same queue or we will deadlock.
   */
  enqueueCall(func) {
    this._queue = (async () => {
      await this._queue;
      try {
        return await func();
      } catch (e) {
        this._log.error(e);
        return false;
      }
    })();
  }

  promiseCallsComplete() {
    return this._queue;
  }
}

/*
 * Subclass of AsyncQueueCaller that can be used with Services.obs directly.
 * When this observe() is called, it will enqueue a call to the consumers's
 * observe().
 */
class AsyncObserver extends AsyncQueueCaller {
  constructor(obj, log) {
    super(log);
    this.obj = obj;
  }

  observe(subject, topic, data) {
    this.enqueueCall(() => this.obj.observe(subject, topic, data));
  }

  promiseObserversComplete() {
    return this.promiseCallsComplete();
  }
}
