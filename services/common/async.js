/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Async"];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

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

  // Returns a method that yields every X calls.
  // Common case is calling the returned method every iteration in a loop.
  jankYielder(yieldEvery = 50) {
    let iterations = 0;
    return async () => {
      Async.checkAppReady(); // Let it throw!
      if (++iterations % yieldEvery === 0) {
        await Async.promiseYield();
      }
    };
  },

  /**
   * Turn a synchronous iterator/iterable into an async iterator that calls a
   * Async.jankYielder at each step.
   *
   * @param iterable {Iterable}
   *        Iterable or iterator that should be wrapped. (Anything usable in
   *        for..of should work)
   *
   * @param [maybeYield = 50] {number|() => Promise<void>}
   *        Either an existing jankYielder to use, or a number to provide as the
   *        argument to Async.jankYielder.
   */
  async* yieldingIterator(iterable, maybeYield = 50) {
    if (typeof maybeYield == "number") {
      maybeYield = Async.jankYielder(maybeYield);
    }
    for (let item of iterable) {
      await maybeYield();
      yield item;
    }
  },

  asyncQueueCaller(log) {
    return new AsyncQueueCaller(log);
  },

  asyncObserver(log, obj) {
    return new AsyncObserver(log, obj);
  }
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
        await func();
      } catch (e) {
        this._log.error(e);
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
