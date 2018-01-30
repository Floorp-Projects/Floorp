/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* eslint-disable mozilla/balanced-listeners */

/* exported BaseProcess, PromiseWorker */

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.importGlobalProperties(["TextDecoder"]);

XPCOMUtils.defineLazyModuleGetter(this, "AsyncShutdown",
                                  "resource://gre/modules/AsyncShutdown.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
                                  "resource://gre/modules/Timer.jsm");

Services.scriptloader.loadSubScript("resource://gre/modules/subprocess/subprocess_shared.js", this);

/* global SubprocessConstants */
var EXPORTED_SYMBOLS = ["BaseProcess", "PromiseWorker", "SubprocessConstants"];

const BUFFER_SIZE = 32768;

let nextResponseId = 0;

/**
 * Wraps a ChromeWorker so that messages sent to it return a promise which
 * resolves when the message has been received and the operation it triggers is
 * complete.
 */
class PromiseWorker extends ChromeWorker {
  constructor(url) {
    super(url);

    this.listeners = new Map();
    this.pendingResponses = new Map();

    this.addListener("close", this.onClose.bind(this));
    this.addListener("failure", this.onFailure.bind(this));
    this.addListener("success", this.onSuccess.bind(this));
    this.addListener("debug", this.onDebug.bind(this));

    this.addEventListener("message", this.onmessage);

    this.shutdown = this.shutdown.bind(this);
    AsyncShutdown.webWorkersShutdown.addBlocker(
      "Subprocess.jsm: Shut down IO worker",
      this.shutdown);
  }

  onClose() {
    AsyncShutdown.webWorkersShutdown.removeBlocker(this.shutdown);
  }

  shutdown() {
    return this.call("shutdown", []);
  }

  /**
   * Adds a listener for the given message from the worker. Any message received
   * from the worker with a `data.msg` property matching the given `msg`
   * parameter are passed to the given listener.
   *
   * @param {string} msg
   *        The message to listen for.
   * @param {function(Event)} listener
   *        The listener to call when matching messages are received.
   */
  addListener(msg, listener) {
    if (!this.listeners.has(msg)) {
      this.listeners.set(msg, new Set());
    }
    this.listeners.get(msg).add(listener);
  }

  /**
   * Removes the given message listener.
   *
   * @param {string} msg
   *        The message to stop listening for.
   * @param {function(Event)} listener
   *        The listener to remove.
   */
  removeListener(msg, listener) {
    let listeners = this.listeners.get(msg);
    if (listeners) {
      listeners.delete(listener);

      if (!listeners.size) {
        this.listeners.delete(msg);
      }
    }
  }

  onmessage(event) {
    let {msg} = event.data;
    let listeners = this.listeners.get(msg) || new Set();

    for (let listener of listeners) {
      try {
        listener(event.data);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }

  /**
   * Called when a message sent to the worker has failed, and rejects its
   * corresponding promise.
   *
   * @private
   */
  onFailure({msgId, error}) {
    this.pendingResponses.get(msgId).reject(error);
    this.pendingResponses.delete(msgId);
  }

  /**
   * Called when a message sent to the worker has succeeded, and resolves its
   * corresponding promise.
   *
   * @private
   */
  onSuccess({msgId, data}) {
    this.pendingResponses.get(msgId).resolve(data);
    this.pendingResponses.delete(msgId);
  }

  onDebug({message}) {
    dump(`Worker debug: ${message}\n`);
  }

  /**
   * Calls the given method in the worker, and returns a promise which resolves
   * or rejects when the method has completed.
   *
   * @param {string} method
   *        The name of the method to call.
   * @param {Array} args
   *        The arguments to pass to the method.
   * @param {Array} [transferList]
   *        A list of objects to transfer to the worker, rather than cloning.
   * @returns {Promise}
   */
  call(method, args, transferList = []) {
    let msgId = nextResponseId++;

    return new Promise((resolve, reject) => {
      this.pendingResponses.set(msgId, {resolve, reject});

      let message = {
        msg: method,
        msgId,
        args,
      };

      this.postMessage(message, transferList);
    });
  }
}

/**
 * Represents an input or output pipe connected to a subprocess.
 *
 * @property {integer} fd
 *           The file descriptor number of the pipe on the child process's side.
 *           @readonly
 */
class Pipe {
  /**
   * @param {Process} process
   *        The child process that this pipe is connected to.
   * @param {integer} fd
   *        The file descriptor number of the pipe on the child process's side.
   * @param {integer} id
   *        The internal ID of the pipe, which ties it to the corresponding Pipe
   *        object on the Worker side.
   */
  constructor(process, fd, id) {
    this.id = id;
    this.fd = fd;
    this.processId = process.id;
    this.worker = process.worker;

    /**
     * @property {boolean} closed
     *           True if the file descriptor has been closed, and can no longer
     *           be read from or written to. Pending IO operations may still
     *           complete, but new operations may not be initiated.
     *           @readonly
     */
    this.closed = false;
  }

  /**
   * Closes the end of the pipe which belongs to this process.
   *
   * @param {boolean} force
   *        If true, the pipe is closed immediately, regardless of any pending
   *        IO operations. If false, the pipe is closed after any existing
   *        pending IO operations have completed.
   * @returns {Promise<object>}
   *          Resolves to an object with no properties once the pipe has been
   *          closed.
   */
  close(force = false) {
    this.closed = true;
    return this.worker.call("close", [this.id, force]);
  }
}

/**
 * Represents an output-only pipe, to which data may be written.
 */
class OutputPipe extends Pipe {
  constructor(...args) {
    super(...args);

    this.encoder = new TextEncoder();
  }

  /**
   * Writes the given data to the stream.
   *
   * When given an array buffer or typed array, ownership of the buffer is
   * transferred to the IO worker, and it may no longer be used from this
   * thread.
   *
   * @param {ArrayBuffer|TypedArray|string} buffer
   *        Data to write to the stream.
   * @returns {Promise<object>}
   *          Resolves to an object with a `bytesWritten` property, containing
   *          the number of bytes successfully written, once the operation has
   *          completed.
   *
   * @rejects {object}
   *          May be rejected with an Error object, or an object with similar
   *          properties. The object will include an `errorCode` property with
   *          one of the following values if it was rejected for the
   *          corresponding reason:
   *
   *            - Subprocess.ERROR_END_OF_FILE: The pipe was closed before
   *              all of the data in `buffer` could be written to it.
   */
  write(buffer) {
    if (typeof buffer === "string") {
      buffer = this.encoder.encode(buffer);
    }

    if (Cu.getClassName(buffer, true) !== "ArrayBuffer") {
      if (buffer.byteLength === buffer.buffer.byteLength) {
        buffer = buffer.buffer;
      } else {
        buffer = buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
      }
    }

    let args = [this.id, buffer];

    return this.worker.call("write", args, [buffer]);
  }
}

/**
 * Represents an input-only pipe, from which data may be read.
 */
class InputPipe extends Pipe {
  constructor(...args) {
    super(...args);

    this.buffers = [];

    /**
     * @property {integer} dataAvailable
     *           The number of readable bytes currently stored in the input
     *           buffer.
     *           @readonly
     */
    this.dataAvailable = 0;

    this.decoder = new TextDecoder();

    this.pendingReads = [];

    this._pendingBufferRead = null;

    this.fillBuffer();
  }

  /**
   * @property {integer} bufferSize
   *           The current size of the input buffer. This varies depending on
   *           the size of pending read operations.
   *           @readonly
   */
  get bufferSize() {
    if (this.pendingReads.length) {
      return Math.max(this.pendingReads[0].length, BUFFER_SIZE);
    }
    return BUFFER_SIZE;
  }

  /**
   * Attempts to fill the input buffer.
   *
   * @private
   */
  fillBuffer() {
    let dataWanted = this.bufferSize - this.dataAvailable;

    if (!this._pendingBufferRead && dataWanted > 0) {
      this._pendingBufferRead = this._read(dataWanted);

      this._pendingBufferRead.then((result) => {
        this._pendingBufferRead = null;

        if (result) {
          this.onInput(result.buffer);

          this.fillBuffer();
        }
      });
    }
  }

  _read(size) {
    let args = [this.id, size];

    return this.worker.call("read", args).catch(e => {
      this.closed = true;

      for (let {length, resolve, reject} of this.pendingReads.splice(0)) {
        if (length === null && e.errorCode === SubprocessConstants.ERROR_END_OF_FILE) {
          resolve(new ArrayBuffer(0));
        } else {
          reject(e);
        }
      }
    });
  }

  /**
   * Adds the given data to the end of the input buffer.
   *
   * @param {ArrayBuffer} buffer
   *        An input buffer to append to the current buffered input.
   * @private
   */
  onInput(buffer) {
    this.buffers.push(buffer);
    this.dataAvailable += buffer.byteLength;
    this.checkPendingReads();
  }

  /**
   * Checks the topmost pending read operations and fulfills as many as can be
   * filled from the current input buffer.
   *
   * @private
   */
  checkPendingReads() {
    this.fillBuffer();

    let reads = this.pendingReads;
    while (reads.length && this.dataAvailable &&
           reads[0].length <= this.dataAvailable) {
      let pending = this.pendingReads.shift();

      let length = pending.length || this.dataAvailable;

      let result;
      let byteLength = this.buffers[0].byteLength;
      if (byteLength == length) {
        result = this.buffers.shift();
      } else if (byteLength > length) {
        let buffer = this.buffers[0];

        this.buffers[0] = buffer.slice(length);
        result = ArrayBuffer.transfer(buffer, length);
      } else {
        result = ArrayBuffer.transfer(this.buffers.shift(), length);
        let u8result = new Uint8Array(result);

        while (byteLength < length) {
          let buffer = this.buffers[0];
          let u8buffer = new Uint8Array(buffer);

          let remaining = length - byteLength;

          if (buffer.byteLength <= remaining) {
            this.buffers.shift();

            u8result.set(u8buffer, byteLength);
          } else {
            this.buffers[0] = buffer.slice(remaining);

            u8result.set(u8buffer.subarray(0, remaining), byteLength);
          }

          byteLength += Math.min(buffer.byteLength, remaining);
        }
      }

      this.dataAvailable -= result.byteLength;
      pending.resolve(result);
    }
  }

  /**
   * Reads exactly `length` bytes of binary data from the input stream, or, if
   * length is not provided, reads the first chunk of data to become available.
   * In the latter case, returns an empty array buffer on end of file.
   *
   * The read operation will not complete until enough data is available to
   * fulfill the request. If the pipe closes without enough available data to
   * fulfill the read, the operation fails, and any remaining buffered data is
   * lost.
   *
   * @param {integer} [length]
   *        The number of bytes to read.
   * @returns {Promise<ArrayBuffer>}
   *
   * @rejects {object}
   *          May be rejected with an Error object, or an object with similar
   *          properties. The object will include an `errorCode` property with
   *          one of the following values if it was rejected for the
   *          corresponding reason:
   *
   *            - Subprocess.ERROR_END_OF_FILE: The pipe was closed before
   *              enough input could be read to satisfy the request.
   */
  read(length = null) {
    if (length !== null && !(Number.isInteger(length) && length >= 0)) {
      throw new RangeError("Length must be a non-negative integer");
    }

    if (length == 0) {
      return Promise.resolve(new ArrayBuffer(0));
    }

    return new Promise((resolve, reject) => {
      this.pendingReads.push({length, resolve, reject});
      this.checkPendingReads();
    });
  }

  /**
   * Reads exactly `length` bytes from the input stream, and parses them as
   * UTF-8 JSON data.
   *
   * @param {integer} length
   *        The number of bytes to read.
   * @returns {Promise<object>}
   *
   * @rejects {object}
   *          May be rejected with an Error object, or an object with similar
   *          properties. The object will include an `errorCode` property with
   *          one of the following values if it was rejected for the
   *          corresponding reason:
   *
   *            - Subprocess.ERROR_END_OF_FILE: The pipe was closed before
   *              enough input could be read to satisfy the request.
   *            - Subprocess.ERROR_INVALID_JSON: The data read from the pipe
   *              could not be parsed as a valid JSON string.
   */
  readJSON(length) {
    if (!Number.isInteger(length) || length <= 0) {
      throw new RangeError("Length must be a positive integer");
    }

    return this.readString(length).then(string => {
      try {
        return JSON.parse(string);
      } catch (e) {
        e.errorCode = SubprocessConstants.ERROR_INVALID_JSON;
        throw e;
      }
    });
  }

  /**
   * Reads a chunk of UTF-8 data from the input stream, and converts it to a
   * JavaScript string.
   *
   * If `length` is provided, reads exactly `length` bytes. Otherwise, reads the
   * first chunk of data to become available, and returns an empty string on end
   * of file. In the latter case, the chunk is decoded in streaming mode, and
   * any incomplete UTF-8 sequences at the end of a chunk are returned at the
   * start of a subsequent read operation.
   *
   * @param {integer} [length]
   *        The number of bytes to read.
   * @param {object} [options]
   *        An options object as expected by TextDecoder.decode.
   * @returns {Promise<string>}
   *
   * @rejects {object}
   *          May be rejected with an Error object, or an object with similar
   *          properties. The object will include an `errorCode` property with
   *          one of the following values if it was rejected for the
   *          corresponding reason:
   *
   *            - Subprocess.ERROR_END_OF_FILE: The pipe was closed before
   *              enough input could be read to satisfy the request.
   */
  readString(length = null, options = {stream: length === null}) {
    if (length !== null && !(Number.isInteger(length) && length >= 0)) {
      throw new RangeError("Length must be a non-negative integer");
    }

    return this.read(length).then(buffer => {
      return this.decoder.decode(buffer, options);
    });
  }

  /**
   * Reads 4 bytes from the input stream, and parses them as an unsigned
   * integer, in native byte order.
   *
   * @returns {Promise<integer>}
   *
   * @rejects {object}
   *          May be rejected with an Error object, or an object with similar
   *          properties. The object will include an `errorCode` property with
   *          one of the following values if it was rejected for the
   *          corresponding reason:
   *
   *            - Subprocess.ERROR_END_OF_FILE: The pipe was closed before
   *              enough input could be read to satisfy the request.
   */
  readUint32() {
    return this.read(4).then(buffer => {
      return new Uint32Array(buffer)[0];
    });
  }
}

/**
 * @class Process
 * @extends BaseProcess
 */

/**
 * Represents a currently-running process, and allows interaction with it.
 */
class BaseProcess {
  /**
   * @param {PromiseWorker} worker
   *        The worker instance which owns the process.
   * @param {integer} processId
   *        The internal ID of the Process object, which ties it to the
   *        corresponding process on the Worker side.
   * @param {integer[]} fds
   *        An array of internal Pipe IDs, one for each standard file descriptor
   *        in the child process.
   * @param {integer} pid
   *        The operating system process ID of the process.
   */
  constructor(worker, processId, fds, pid) {
    this.id = processId;
    this.worker = worker;

    /**
     * @property {integer} pid
     *           The process ID of the process, assigned by the operating system.
     *           @readonly
     */
    this.pid = pid;

    this.exitCode = null;

    this.exitPromise = new Promise(resolve => {
      this.worker.call("wait", [this.id]).then(({exitCode}) => {
        resolve(Object.freeze({exitCode}));
        this.exitCode = exitCode;
      });
    });

    if (fds[0] !== undefined) {
      /**
       * @property {OutputPipe} stdin
       *           A Pipe object which allows writing to the process's standard
       *           input.
       *           @readonly
       */
      this.stdin = new OutputPipe(this, 0, fds[0]);
    }
    if (fds[1] !== undefined) {
      /**
       * @property {InputPipe} stdout
       *           A Pipe object which allows reading from the process's standard
       *           output.
       *           @readonly
       */
      this.stdout = new InputPipe(this, 1, fds[1]);
    }
    if (fds[2] !== undefined) {
      /**
       * @property {InputPipe} [stderr]
       *           An optional Pipe object which allows reading from the
       *           process's standard error output.
       *           @readonly
       */
      this.stderr = new InputPipe(this, 2, fds[2]);
    }
  }

  /**
   * Spawns a process, and resolves to a BaseProcess instance on success.
   *
   * @param {object} options
   *        An options object as passed to `Subprocess.call`.
   *
   * @returns {Promise<BaseProcess>}
   */
  static create(options) {
    let worker = this.getWorker();

    return worker.call("spawn", [options]).then(({processId, fds, pid}) => {
      return new this(worker, processId, fds, pid);
    });
  }

  static get WORKER_URL() {
    throw new Error("Not implemented");
  }

  static get WorkerClass() {
    return PromiseWorker;
  }

  /**
   * Gets the current subprocess worker, or spawns a new one if it does not
   * currently exist.
   *
   * @returns {PromiseWorker}
   */
  static getWorker() {
    if (!this._worker) {
      this._worker = new this.WorkerClass(this.WORKER_URL);
    }
    return this._worker;
  }

  /**
   * Kills the process.
   *
   * @param {integer} [timeout=300]
   *        A timeout, in milliseconds, after which the process will be forcibly
   *        killed. On platforms which support it, the process will be sent
   *        a `SIGTERM` signal immediately, so that it has a chance to terminate
   *        gracefully, and a `SIGKILL` signal if it hasn't exited within
   *        `timeout` milliseconds. On other platforms (namely Windows), the
   *        process will be forcibly terminated immediately.
   *
   * @returns {Promise<object>}
   *          Resolves to an object with an `exitCode` property when the process
   *          has exited.
   */
  kill(timeout = 300) {
    // If the process has already exited, don't bother sending a signal.
    if (this.exitCode != null) {
      return this.wait();
    }

    let force = timeout <= 0;
    this.worker.call("kill", [this.id, force]);

    if (!force) {
      setTimeout(() => {
        if (this.exitCode == null) {
          this.worker.call("kill", [this.id, true]);
        }
      }, timeout);
    }

    return this.wait();
  }

  /**
   * Returns a promise which resolves to the process's exit code, once it has
   * exited.
   *
   * @returns {Promise<object>}
   * Resolves to an object with an `exitCode` property, containing the
   * process's exit code, once the process has exited.
   *
   * On Unix-like systems, a negative exit code indicates that the
   * process was killed by a signal whose signal number is the absolute
   * value of the error code. On Windows, an exit code of -9 indicates
   * that the process was killed via the {@linkcode BaseProcess#kill kill()}
   * method.
   */
  wait() {
    return this.exitPromise;
  }
}
