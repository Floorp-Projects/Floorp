/**
 * Thin wrapper around a ChromeWorker that wraps postMessage/onmessage/onerror
 * as promises.
 *
 * Not for public use yet.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["PromiseWorker"];

// The library of promises.
Components.utils.import("resource://gre/modules/Promise.jsm", this);

/**
 * An implementation of queues (FIFO).
 *
 * The current implementation uses one array, runs in O(n ^ 2), and is optimized
 * for the case in which queues are generally short.
 */
let Queue = function Queue() {
  this._array = [];
};
Queue.prototype = {
  pop: function pop() {
    return this._array.shift();
  },
  push: function push(x) {
    return this._array.push(x);
  },
  isEmpty: function isEmpty() {
    return this._array.length == 0;
  }
};

/**
 * An object responsible for dispatching messages to
 * a chrome worker and routing the responses.
 *
 * @param {string} url The url containing the source code for this worker,
 * as in constructor ChromeWorker.
 * @param {Function} log A logging function.
 *
 * @constructor
 */
function PromiseWorker(url, log) {
  if (typeof url != "string") {
    throw new TypeError("Expecting a string");
  }
  if (typeof log !== "function") {
    throw new TypeError("log is expected to be a function");
  }
  this._log = log;
  this._url = url;

  /**
   * The queue of deferred, waiting for the completion of their
   * respective job by the worker.
   *
   * Each item in the list may contain an additional field |closure|,
   * used to store strong references to value that must not be
   * garbage-collected before the reply has been received (e.g.
   * arrays).
   *
   * @type {Queue<{deferred:deferred, closure:*=}>}
   */
  this._queue = new Queue();

  /**
   * The number of the current message.
   *
   * Used for debugging purposes.
   */
  this._id = 0;

  /**
   * The instant at which the worker was launched.
   */
  this.launchTimeStamp = null;

  /**
   * Timestamps provided by the worker for statistics purposes.
   */
  this.workerTimeStamps = null;
}
PromiseWorker.prototype = {
  /**
   * Instantiate the worker lazily.
   */
  get _worker() {
    delete this._worker;
    let worker = new ChromeWorker(this._url);
    let self = this;
    Object.defineProperty(this, "_worker", {value:
      worker
    });

    // We assume that we call to _worker for the purpose of calling
    // postMessage().
    this.launchTimeStamp = Date.now();

    /**
     * Receive errors that are not instances of OS.File.Error, propagate
     * them to the listeners.
     *
     * The worker knows how to serialize errors that are instances
     * of |OS.File.Error|. These are treated by |worker.onmessage|.
     * However, for other errors, we rely on DOM's mechanism for
     * serializing errors, which transmits these errors through
     * |worker.onerror|.
     *
     * @param {Error} error Some JS error.
     */
    worker.onerror = function onerror(error) {
      self._log("Received uncaught error from worker", error.message, error.filename, error.lineno);
      error.preventDefault();
      let {deferred} = self._queue.pop();
      deferred.reject(error);
    };

    /**
     * Receive messages from the worker, propagate them to the listeners.
     *
     * Messages must have one of the following shapes:
     * - {ok: some_value} in case of success
     * - {fail: some_error} in case of error, where
     *    some_error is an instance of |PromiseWorker.WorkerError|
     *
     * Messages may also contain a field |id| to help
     * with debugging.
     *
     * Messages may also optionally contain a field |durationMs|, holding
     * the duration of the function call in milliseconds.
     *
     * @param {*} msg The message received from the worker.
     */
    worker.onmessage = function onmessage(msg) {
      self._log("Received message from worker", msg.data);
      let handler = self._queue.pop();
      let deferred = handler.deferred;
      let data = msg.data;
      if (data.id != handler.id) {
        throw new Error("Internal error: expecting msg " + handler.id + ", " +
                        " got " + data.id + ": " + JSON.stringify(msg.data));
      }
      if ("timeStamps" in data) {
        self.workerTimeStamps = data.timeStamps;
      }
      if ("ok" in data) {
        // Pass the data to the listeners.
        deferred.resolve(data);
      } else if ("StopIteration" in data) {
        // We have received a StopIteration error
        deferred.reject(StopIteration);
      } if ("fail" in data) {
        // We have received an error that was serialized by the
        // worker.
        deferred.reject(new PromiseWorker.WorkerError(data.fail));
      }
    };
    return worker;
  },

  /**
   * Post a message to a worker.
   *
   * @param {string} fun The name of the function to call.
   * @param {Array} array The contents of the message.
   * @param {*=} closure An object holding references that should not be
   * garbage-collected before the message treatment is complete.
   *
   * @return {promise}
   */
  post: function post(fun, array, closure) {
    let deferred = Promise.defer();
    let id = ++this._id;
    let message = {fun: fun, args: array, id: id};
    this._log("Posting message", message);
    try {
      this._worker.postMessage(message);
    } catch (ex if typeof ex == "number") {
      this._log("Could not post message", message, "due to xpcom error", ex);
      // handle raw xpcom errors (see eg bug 961317)
      return Promise.reject(new Components.Exception("Error in postMessage", ex));
    } catch (ex) {
      this._log("Could not post message", message, "due to error", ex);
      return Promise.reject(ex);
    }

    this._queue.push({deferred:deferred, closure: closure, id: id});
    this._log("Message posted");
    return deferred.promise;
  }
};

/**
 * An error that has been serialized by the worker.
 *
 * @constructor
 */
PromiseWorker.WorkerError = function WorkerError(data) {
  this.data = data;
};

this.PromiseWorker = PromiseWorker;
