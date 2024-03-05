/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A wrapper around ChromeWorker with extended capabilities designed
 * to simplify main thread-to-worker thread asynchronous function calls.
 *
 * This wrapper:
 * - groups requests and responses as a method `post` that returns a `Promise`;
 * - ensures that exceptions thrown on the worker thread are correctly deserialized;
 * - provides some utilities for benchmarking various operations.
 *
 * Generally, you should use PromiseWorker.sys.mjs along with its worker-side
 * counterpart PromiseWorker.js.
 */

/**
 * Constructors for decoding standard exceptions received from the
 * worker.
 */
const EXCEPTION_CONSTRUCTORS = {
  EvalError(error) {
    let result = new EvalError(error.message, error.fileName, error.lineNumber);
    result.stack = error.stack;
    return result;
  },
  InternalError(error) {
    let result = new InternalError(
      error.message,
      error.fileName,
      error.lineNumber
    );
    result.stack = error.stack;
    return result;
  },
  RangeError(error) {
    let result = new RangeError(
      error.message,
      error.fileName,
      error.lineNumber
    );
    result.stack = error.stack;
    return result;
  },
  ReferenceError(error) {
    let result = new ReferenceError(
      error.message,
      error.fileName,
      error.lineNumber
    );
    result.stack = error.stack;
    return result;
  },
  SyntaxError(error) {
    let result = new SyntaxError(
      error.message,
      error.fileName,
      error.lineNumber
    );
    result.stack = error.stack;
    return result;
  },
  TypeError(error) {
    let result = new TypeError(error.message, error.fileName, error.lineNumber);
    result.stack = error.stack;
    return result;
  },
  URIError(error) {
    let result = new URIError(error.message, error.fileName, error.lineNumber);
    result.stack = error.stack;
    return result;
  },
  DOMException(error) {
    let result = new DOMException(error.message, error.name);
    return result;
  },
};

/**
 * An object responsible for dispatching messages to a chrome worker
 * and routing the responses.
 *
 * Instances of this constructor who need logging may provide a method
 * `log: function(...args) { ... }` in charge of printing out (or
 * discarding) logs.
 *
 * Instances of this constructor may add exception handlers to
 * `this.ExceptionHandlers`, if they need to handle custom exceptions.
 *
 * @param {string} url The url containing the source code for this worker,
 * as in constructor ChromeWorker.
 *
 * @param {WorkerOptions} options The option parameter for ChromeWorker.
 *
 * @param {Record<String, function>} functions Functions that the worker can call.
 *
 * Functions can be synchronous functions or promises and return a value
 * that is sent back to the worker. The function can also send back a
 * `BasePromiseWorker.Meta` object containing the data and an array of transferrable
 * objects to transfer data to the worker with zero memory copy via `postMessage`.
 *
 * Example of sunch a function:
 *
 *   async function threadFunction(message) {
 *     return new BasePromiseWorker.Meta(
 *         ["data1", "data2", someBuffer],
 *         {transfers: [someBuffer]}
 *     );
 *   }
 *
 * @constructor
 */
export var BasePromiseWorker = function (url, options = {}, functions = {}) {
  if (typeof url != "string") {
    throw new TypeError("Expecting a string");
  }
  this._url = url;
  this._options = options;
  this._functions = functions;

  /**
   * A set of methods, with the following
   *
   * ConstructorName: function({message, fileName, lineNumber}) {
   *   // Construct a new instance of ConstructorName based on
   *   // `message`, `fileName`, `lineNumber`
   * }
   *
   * By default, this covers EvalError, InternalError, RangeError,
   * ReferenceError, SyntaxError, TypeError, URIError.
   */
  this.ExceptionHandlers = Object.create(EXCEPTION_CONSTRUCTORS);

  /**
   * The map of deferred, waiting for the completion of their
   * respective job by the worker.
   *
   * Each item in the map may contain an additional field |closure|,
   * used to store strong references to value that must not be
   * garbage-collected before the reply has been received (e.g.
   * arrays).
   *
   * @type {Map<string, {deferred, closure, id}>}
   */
  this._deferredJobs = new Map();

  /**
   * The number of the current message.
   *
   * Used for debugging purposes.
   */
  this._deferredJobId = 0;

  /**
   * The instant at which the worker was launched.
   */
  this.launchTimeStamp = null;

  /**
   * Timestamps provided by the worker for statistics purposes.
   */
  this.workerTimeStamps = null;
};

BasePromiseWorker.prototype = {
  log() {
    // By Default, ignore all logs.
  },

  _generateDeferredJobId() {
    this._deferredJobId += 1;
    return "ThreadToWorker-" + this._deferredJobId;
  },

  /**
   * Instantiate the worker lazily.
   */
  get _worker() {
    if (this.__worker) {
      return this.__worker;
    }

    let worker = (this.__worker = new ChromeWorker(this._url, this._options));

    // We assume that we call to _worker for the purpose of calling
    // postMessage().
    this.launchTimeStamp = Date.now();

    /**
     * Receive errors that have been serialized by the built-in mechanism
     * of DOM/Chrome Workers.
     *
     * PromiseWorker.js  knows how  to  serialize a  number of  errors
     * without    losing   information.    These   are    treated   by
     * |worker.onmessage|.   However, for  other  errors,  we rely  on
     * DOM's mechanism  for serializing errors, which  transmits these
     * errors through |worker.onerror|.
     *
     * @param {Error} error Some JS error.
     */
    worker.onerror = error => {
      this.log(
        "Received uncaught error from worker",
        error.message,
        error.filename,
        error.lineno
      );

      error.preventDefault();

      if (this._deferredJobs.size > 0) {
        this._deferredJobs.forEach(job => {
          job.deferred.reject(error);
        });
        this._deferredJobs.clear();
      }
    };

    /**
     * Receive messages from the worker, propagate them to the listeners.
     *
     * Messages must have one of the following shapes:
     * - {ok: some_value} in case of success
     * - {fail: some_error} in case of error, where
     *    some_error is an instance of |PromiseWorker.WorkerError|
     *
     * Messages also contains the following fields:
     * - |id| an integer matching the deferred function to resolve (mandatory)
     * - |fun| a string matching a function to call (optional)
     * - |durationMs| holding the duration of the function call in milliseconds. (optional)
     *
     * @param {*} msg The message received from the worker.
     */
    worker.onmessage = msg => {
      let data = msg.data;
      let messageId = data.id;

      this.log(`Received message ${messageId} from worker`);

      if ("timeStamps" in data) {
        this.workerTimeStamps = data.timeStamps;
      }

      // If fun is provided by the worker, we look into the functions
      if ("fun" in data) {
        if (data.fun in this._functions) {
          Promise.resolve(this._functions[data.fun](...data.args)).then(
            ok => {
              if (ok instanceof BasePromiseWorker.Meta) {
                if ("transfers" in ok.meta) {
                  worker.postMessage(
                    { ok: ok.data, id: messageId },
                    ok.meta.transfers
                  );
                } else {
                  worker.postMessage({ ok: ok.data, id: messageId });
                }
              } else {
                worker.postMessage({ id: messageId, ok });
              }
            },
            fail => {
              worker.postMessage({ id: messageId, fail });
            }
          );
        } else {
          worker.postMessage({
            id: messageId,
            fail: `function ${data.fun} not found`,
          });
        }
        return;
      }

      // If the message id matches one of the promise that waits, we resolve/reject with the data
      if (this._deferredJobs.has(messageId)) {
        let handler = this._deferredJobs.get(messageId);
        let deferred = handler.deferred;

        if ("ok" in data) {
          // Pass the data to the listeners.
          deferred.resolve(data);
        } else if ("fail" in data) {
          // We have received an error that was serialized by the
          // worker.
          deferred.reject(new WorkerError(data.fail));
        }
        return;
      }

      // in any other case, this is an unexpected message from the worker.
      throw new Error(
        `Unexpected message id ${messageId}, data: ${JSON.stringify(data)} `
      );
    };
    return worker;
  },

  /**
   * Post a message to a worker.
   *
   * @param {string} fun The name of the function to call.
   * @param {Array} args The arguments to pass to `fun`. If any
   * of the arguments is a Promise, it is resolved before posting the
   * message. If any of the arguments needs to be transfered instead
   * of copied, this may be specified by making the argument an instance
   * of `BasePromiseWorker.Meta` or by using the `transfers` argument.
   * By convention, the last argument may be an object `options`
   * with some of the following fields:
   * - {number|null} outExecutionDuration A parameter to be filled with the
   *   duration of the off main thread execution for this call.
   * @param {*=} closure An object holding references that should not be
   * garbage-collected before the message treatment is complete.
   * @param {Array=} transfers An array of objects that should be transfered
   * to the worker instead of being copied. If any of the objects is a Promise,
   * it is resolved before posting the message.
   *
   * @return {promise}
   */
  post(fun, args, closure, transfers) {
    return async function postMessage() {
      // Normalize in case any of the arguments is a promise
      if (args) {
        args = await Promise.resolve(Promise.all(args));
      }
      if (transfers) {
        transfers = await Promise.resolve(Promise.all(transfers));
      } else {
        transfers = [];
      }

      if (args) {
        // Extract `Meta` data
        args = args.map(arg => {
          if (arg instanceof BasePromiseWorker.Meta) {
            if (arg.meta && "transfers" in arg.meta) {
              transfers.push(...arg.meta.transfers);
            }
            return arg.data;
          }
          return arg;
        });
      }

      let id = this._generateDeferredJobId();
      let message = { fun, args, id };
      this.log("Posting message", JSON.stringify(message));
      try {
        this._worker.postMessage(message, ...[transfers]);
      } catch (ex) {
        if (typeof ex == "number") {
          this.log("Could not post message", message, "due to xpcom error", ex);
          // handle raw xpcom errors (see eg bug 961317)
          throw new Components.Exception("Error in postMessage", ex);
        }

        this.log("Could not post message", message, "due to error", ex);
        throw ex;
      }

      let deferred = Promise.withResolvers();
      this._deferredJobs.set(id, { deferred, closure, id });

      let reply;
      try {
        this.log("Expecting reply");
        reply = await deferred.promise;
      } catch (error) {
        this.log("Got error", JSON.stringify(error));
        reply = error;

        if (error instanceof WorkerError) {
          // We know how to deserialize most well-known errors
          throw this.ExceptionHandlers[error.data.exn](error.data);
        }

        if (ErrorEvent.isInstance(error)) {
          // Other errors get propagated as instances of ErrorEvent
          this.log(
            "Error serialized by DOM",
            error.message,
            error.filename,
            error.lineno
          );
          throw new Error(error.message, error.filename, error.lineno);
        }

        // We don't know about this kind of error
        throw error;
      }

      // By convention, the last argument may be an object `options`.
      let options = null;
      if (args) {
        options = args[args.length - 1];
      }

      // Check for duration and return result.
      if (
        !options ||
        typeof options !== "object" ||
        !("outExecutionDuration" in options)
      ) {
        return reply.ok;
      }
      // If reply.durationMs is not present, just return the result,
      // without updating durations (there was an error in the method
      // dispatch).
      if (!("durationMs" in reply)) {
        return reply.ok;
      }
      // Bug 874425 demonstrates that two successive calls to Date.now()
      // can actually produce an interval with negative duration.
      // We assume that this is due to an operation that is so short
      // that Date.now() is not monotonic, so we round this up to 0.
      let durationMs = Math.max(0, reply.durationMs);
      // Accumulate (or initialize) outExecutionDuration
      if (typeof options.outExecutionDuration == "number") {
        options.outExecutionDuration += durationMs;
      } else {
        options.outExecutionDuration = durationMs;
      }
      return reply.ok;
    }.bind(this)();
  },

  /**
   * Terminate the worker, if it has been created at all, and set things up to
   * be instantiated lazily again on the next `post()`.
   * If there are pending Promises in the jobs, we'll reject them and clear it.
   */
  terminate() {
    if (!this.__worker) {
      return;
    }

    try {
      this.__worker.terminate();
      delete this.__worker;
    } catch (ex) {
      // Ignore exceptions, only log them.
      this.log("Error whilst terminating ChromeWorker: " + ex.message);
    }

    if (this._deferredJobs.size) {
      let error = new Error("Internal error: worker terminated");
      this._deferredJobs.forEach(job => {
        job.deferred.reject(error);
      });
      this._deferredJobs.clear();
    }
  },
};

/**
 * An error that has been serialized by the worker.
 *
 * @constructor
 */
function WorkerError(data) {
  this.data = data;
}

/**
 * A constructor used to send data to the worker thread while
 * with special treatment (e.g. transmitting data instead of
 * copying it).
 *
 * @param {object=} data The data to send to the caller thread.
 * @param {object=} meta Additional instructions, as an object
 * that may contain the following fields:
 * - {Array} transfers An array of objects that should be transferred
 *   instead of being copied.
 *
 * @constructor
 */
BasePromiseWorker.Meta = function (data, meta) {
  this.data = data;
  this.meta = meta;
};
