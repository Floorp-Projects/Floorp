/*
 * Copyright 2009-2011 Mozilla Foundation and contributors
 * Licensed under the New BSD license. See LICENSE.txt or:
 * http://opensource.org/licenses/BSD-3-Clause
 */


var EXPORTED_SYMBOLS = [ "Promise" ];

/**
 * Create an unfulfilled promise
 *
 * @param {*=} aTrace A debugging value
 *
 * @constructor
 */
function Promise(aTrace) {
  this._status = Promise.PENDING;
  this._value = undefined;
  this._onSuccessHandlers = [];
  this._onErrorHandlers = [];
  this._trace = aTrace;

  // Debugging help
  if (Promise.Debug._debug) {
    this._id = Promise.Debug._nextId++;
    Promise.Debug._outstanding[this._id] = this;
  }
}

/**
 * Debugging options and tools.
 */
Promise.Debug = {
  /**
   * Set current debugging mode.
   *
   * @param {boolean} value If |true|, maintain _nextId, _outstanding, _recent.
   * Otherwise, cleanup debugging data.
   */
  setDebug: function(value) {
    Promise.Debug._debug = value;
    if (!value) {
      Promise.Debug._outstanding = [];
      Promise.Debug._recent = [];
    }
  },

  _debug: false,

  /**
   * We give promises and ID so we can track which are outstanding.
   */
  _nextId: 0,

  /**
   * Outstanding promises. Handy for debugging (only).
   */
  _outstanding: [],

  /**
   * Recently resolved promises. Also for debugging only.
   */
  _recent: []
};


/**
 * A promise can be in one of 2 states.
 * The ERROR and SUCCESS states are terminal, the PENDING state is the only
 * start state.
 */
Promise.ERROR = -1;
Promise.PENDING = 0;
Promise.SUCCESS = 1;

/**
 * Yeay for RTTI
 */
Promise.prototype.isPromise = true;

/**
 * Have we either been resolve()ed or reject()ed?
 */
Promise.prototype.isComplete = function() {
  return this._status != Promise.PENDING;
};

/**
 * Have we resolve()ed?
 */
Promise.prototype.isResolved = function() {
  return this._status == Promise.SUCCESS;
};

/**
 * Have we reject()ed?
 */
Promise.prototype.isRejected = function() {
  return this._status == Promise.ERROR;
};

/**
 * Take the specified action of fulfillment of a promise, and (optionally)
 * a different action on promise rejection
 */
Promise.prototype.then = function(onSuccess, onError) {
  if (typeof onSuccess === 'function') {
    if (this._status === Promise.SUCCESS) {
      onSuccess.call(null, this._value);
    }
    else if (this._status === Promise.PENDING) {
      this._onSuccessHandlers.push(onSuccess);
    }
  }

  if (typeof onError === 'function') {
    if (this._status === Promise.ERROR) {
      onError.call(null, this._value);
    }
    else if (this._status === Promise.PENDING) {
      this._onErrorHandlers.push(onError);
    }
  }

  return this;
};

/**
 * Like then() except that rather than returning <tt>this</tt> we return
 * a promise which resolves when the original promise resolves
 */
Promise.prototype.chainPromise = function(onSuccess) {
  var chain = new Promise();
  chain._chainedFrom = this;
  this.then(function(data) {
    try {
      chain.resolve(onSuccess(data));
    }
    catch (ex) {
      chain.reject(ex);
    }
  }, function(ex) {
    chain.reject(ex);
  });
  return chain;
};

/**
 * Supply the fulfillment of a promise
 */
Promise.prototype.resolve = function(data) {
  return this._complete(this._onSuccessHandlers,
                        Promise.SUCCESS, data, 'resolve');
};

/**
 * Renege on a promise
 */
Promise.prototype.reject = function(data) {
  return this._complete(this._onErrorHandlers, Promise.ERROR, data, 'reject');
};

/**
 * Internal method to be called on resolve() or reject()
 * @private
 */
Promise.prototype._complete = function(list, status, data, name) {
  // Complain if we've already been completed
  if (this._status != Promise.PENDING) {
    Promise._error("Promise complete.", "Attempted ", name, "() with ", data);
    Promise._error("Previous status: ", this._status, ", value =", this._value);
    throw new Error('Promise already complete');
  }

  if (list.length == 0 && status == Promise.ERROR) {
    var frame;
    var text;

    //Complain if a rejection is ignored
    //(this is the equivalent of an empty catch-all clause)
    Promise._error("Promise rejection ignored and silently dropped", data);
    if (data.stack) {// This looks like an exception. Try harder to display it
      if (data.fileName && data.lineNumber) {
        Promise._error("Error originating at", data.fileName,
                       ", line", data.lineNumber );
      }
      try {
        for (frame = data.stack; frame; frame = frame.caller) {
          text += frame + "\n";
        }
        Promise._error("Attempting to extract exception stack", text);
      } catch (x) {
        Promise._error("Could not extract exception stack.");
      }
    } else {
      Promise._error("Exception stack not available.");
    }
    if (Components && Components.stack) {
      try {
        text = "";
        for (frame = Components.stack; frame; frame = frame.caller) {
          text += frame + "\n";
        }
        Promise._error("Attempting to extract current stack", text);
      } catch (x) {
        Promise._error("Could not extract current stack.");
      }
    } else {
      Promise._error("Current stack not available.");
    }
  }


  this._status = status;
  this._value = data;

  // Call all the handlers, and then delete them
  list.forEach(function(handler) {
    handler.call(null, this._value);
  }, this);
  delete this._onSuccessHandlers;
  delete this._onErrorHandlers;

  // Remove the given {promise} from the _outstanding list, and add it to the
  // _recent list, pruning more than 20 recent promises from that list
  delete Promise.Debug._outstanding[this._id];
  // The original code includes this very useful debugging aid, however there
  // is concern that it will create a memory leak, so we leave it out here.
  /*
  Promise._recent.push(this);
  while (Promise._recent.length > 20) {
    Promise._recent.shift();
  }
  */

  return this;
};

/**
 * Log an error on the most appropriate channel.
 *
 * If the console is available, this method uses |console.warn|. Otherwise,
 * this method falls back to |dump|.
 *
 * @param {...*} items Items to log.
 */
Promise._error = null;
if (typeof console != "undefined" && console.warn) {
  Promise._error = function() {
    var args = Array.prototype.slice.call(arguments);
    args.unshift("Promise");
    console.warn.call(console, args);
  };
} else {
  Promise._error = function() {
    var i;
    var len = arguments.length;
    dump("Promise: ");
    for (i = 0; i < len; ++i) {
      dump(arguments[i]+" ");
    }
    dump("\n");
  };
}

/**
 * Takes an array of promises and returns a promise that that is fulfilled once
 * all the promises in the array are fulfilled
 * @param promiseList The array of promises
 * @return the promise that is fulfilled when all the array is fulfilled
 */
Promise.group = function(promiseList) {
  if (!Array.isArray(promiseList)) {
    promiseList = Array.prototype.slice.call(arguments);
  }

  // If the original array has nothing in it, return now to avoid waiting
  if (promiseList.length === 0) {
    return new Promise().resolve([]);
  }

  var groupPromise = new Promise();
  var results = [];
  var fulfilled = 0;

  var onSuccessFactory = function(index) {
    return function(data) {
      results[index] = data;
      fulfilled++;
      // If the group has already failed, silently drop extra results
      if (groupPromise._status !== Promise.ERROR) {
        if (fulfilled === promiseList.length) {
          groupPromise.resolve(results);
        }
      }
    };
  };

  promiseList.forEach(function(promise, index) {
    var onSuccess = onSuccessFactory(index);
    var onError = groupPromise.reject.bind(groupPromise);
    promise.then(onSuccess, onError);
  });

  return groupPromise;
};

/**
 * Trap errors.
 *
 * This function serves as an asynchronous counterpart to |catch|.
 *
 * Example:
 *  myPromise.chainPromise(a) //May reject
 *           .chainPromise(b) //May reject
 *           .chainPromise(c) //May reject
 *           .trap(d)       //Catch any rejection from a, b or c
 *           .chainPromise(e) //If either a, b and c or
 *                            //d has resolved, execute
 *
 * Scenario 1:
 *   If a, b, c resolve, e is executed as if d had not been added.
 *
 * Scenario 2:
 *   If a, b or c rejects, d is executed. If d resolves, we proceed
 *   with e as if nothing had happened. Otherwise, we proceed with
 *   the rejection of d.
 *
 * @param {Function} aTrap Called if |this| promise is rejected,
 *   with one argument: the rejection.
 * @return {Promise} A new promise. This promise resolves if all
 *   previous promises have resolved or if |aTrap| succeeds.
 */
Promise.prototype.trap = function(aTrap) {
  var promise = new Promise();
  var resolve = Promise.prototype.resolve.bind(promise);
  var reject = function(aRejection) {
    try {
      //Attempt to handle issue
      var result = aTrap.call(aTrap, aRejection);
      promise.resolve(result);
    } catch (x) {
      promise.reject(x);
    }
  };
  this.then(resolve, reject);
  return promise;
};

/**
 * Execute regardless of errors.
 *
 * This function serves as an asynchronous counterpart to |finally|.
 *
 * Example:
 *  myPromise.chainPromise(a) //May reject
 *           .chainPromise(b) //May reject
 *           .chainPromise(c) //May reject
 *           .always(d)       //Executed regardless
 *           .chainPromise(e)
 *
 * Whether |a|, |b| or |c| resolve or reject, |d| is executed.
 *
 * @param {Function} aTrap Called regardless of whether |this|
 *   succeeds or fails.
 * @return {Promise} A new promise. This promise holds the same
 *   resolution/rejection as |this|.
 */
Promise.prototype.always = function(aTrap) {
  var promise = new Promise();
  var resolve = function(result) {
    try {
      aTrap.call(aTrap);
      promise.resolve(result);
    } catch (x) {
      promise.reject(x);
    }
  };
  var reject = function(result) {
    try {
      aTrap.call(aTrap);
      promise.reject(result);
    } catch (x) {
      promise.reject(result);
    }
  };
  this.then(resolve, reject);
  return promise;
};


Promise.prototype.toString = function() {
  var status;
  switch (this._status) {
  case Promise.PENDING:
    status = "pending";
    break;
  case Promise.SUCCESS:
    status = "resolved";
    break;
  case Promise.ERROR:
    status = "rejected";
    break;
  default:
    status = "invalid status: "+this._status;
  }
  return "[Promise " + this._id + " (" + status + ")]";
};
