/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported BasePipe, BaseProcess, debug */
/* globals Process, io */

function debug(message) {
  self.postMessage({msg: "debug", message});
}

class BasePipe {
  constructor() {
    this.closing = false;
    this.closed = false;

    this.closedPromise = new Promise(resolve => {
      this.resolveClosed = resolve;
    });

    this.pending = [];
  }

  shiftPending() {
    let result = this.pending.shift();

    if (this.closing && this.pending.length == 0) {
      this.close();
    }

    return result;
  }
}

let nextProcessId = 0;

class BaseProcess {
  constructor(options) {
    this.id = nextProcessId++;

    this.exitCode = null;

    this.exitPromise = new Promise(resolve => {
      this.resolveExit = resolve;
    });
    this.exitPromise.then(() => {
      // The input file descriptors will be closed after poll
      // reports that their input buffers are empty. If we close
      // them now, we may lose output.
      this.pipes[0].close(true);
    });

    this.pid = null;
    this.pipes = [];

    this.stringArrays = [];

    this.spawn(options);
  }

  /**
   * Creates a null-terminated array of pointers to null-terminated C-strings,
   * and returns it.
   *
   * @param {string[]} strings
   *        The strings to convert into a C string array.
   *
   * @returns {ctypes.char.ptr.array}
   */
  stringArray(strings) {
    let result = ctypes.char.ptr.array(strings.length + 1)();

    let cstrings = strings.map(str => ctypes.char.array()(str));
    for (let [i, cstring] of cstrings.entries()) {
      result[i] = cstring;
    }

    // Char arrays used in char arg and environment vectors must be
    // explicitly kept alive in a JS object, or they will be reaped
    // by the GC if it runs before our process is started.
    this.stringArrays.push(cstrings);

    return result;
  }
}

let requests = {
  init(details) {
    io.init(details);

    return {data: {}};
  },

  shutdown() {
    io.shutdown();

    return {data: {}};
  },

  close(pipeId, force = false) {
    let pipe = io.getPipe(pipeId);

    return pipe.close(force).then(() => ({data: {}}));
  },

  spawn(options) {
    let process = new Process(options);
    let processId = process.id;

    io.addProcess(process);

    let fds = process.pipes.map(pipe => pipe.id);

    return {data: {processId, fds, pid: process.pid}};
  },

  kill(processId, force = false) {
    let process = io.getProcess(processId);

    process.kill(force ? 9 : 15);

    return {data: {}};
  },

  wait(processId) {
    let process = io.getProcess(processId);

    process.wait();

    return process.exitPromise.then(exitCode => {
      io.cleanupProcess(process);
      return {data: {exitCode}};
    });
  },

  read(pipeId, count) {
    let pipe = io.getPipe(pipeId);

    return pipe.read(count).then(buffer => {
      return {data: {buffer}};
    });
  },

  write(pipeId, buffer) {
    let pipe = io.getPipe(pipeId);

    return pipe.write(buffer).then(bytesWritten => {
      return {data: {bytesWritten}};
    });
  },

  getOpenFiles() {
    return {data: new Set(io.pipes.keys())};
  },

  getProcesses() {
    let data = new Map(Array.from(io.processes.values(),
                                  proc => [proc.id, proc.pid]));
    return {data};
  },

  waitForNoProcesses() {
    return Promise.all(Array.from(io.processes.values(), proc => proc.exitPromise));
  },
};

onmessage = event => {
  io.messageCount--;

  let {msg, msgId, args} = event.data;

  new Promise(resolve => {
    resolve(requests[msg](...args));
  }).then(result => {
    let response = {
      msg: "success",
      msgId,
      data: result.data,
    };

    self.postMessage(response, result.transfer || []);
  }).catch(error => {
    if (error instanceof Error) {
      error = {
        message: error.message,
        fileName: error.fileName,
        lineNumber: error.lineNumber,
        column: error.column,
        stack: error.stack,
        errorCode: error.errorCode,
      };
    }

    self.postMessage({
      msg: "failure",
      msgId,
      error,
    });
  }).catch(error => {
    console.error(error);

    self.postMessage({
      msg: "failure",
      msgId,
      error: {},
    });
  });
};

onclose = event => {
  io.shutdown();

  self.postMessage({msg: "close"});
};
