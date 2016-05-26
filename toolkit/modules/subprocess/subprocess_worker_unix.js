/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported Process */
/* globals BaseProcess, BasePipe */

importScripts("resource://gre/modules/subprocess/subprocess_shared.js",
              "resource://gre/modules/subprocess/subprocess_shared_unix.js",
              "resource://gre/modules/subprocess/subprocess_worker_common.js");

const POLL_INTERVAL = 50;
const POLL_TIMEOUT = 0;

let io;

let nextPipeId = 0;

class Pipe extends BasePipe {
  constructor(process, fd) {
    super();

    this.process = process;
    this.fd = fd;
    this.id = nextPipeId++;
  }

  get pollEvents() {
    throw new Error("Not implemented");
  }

  /**
   * Closes the file descriptor.
   *
   * @param {boolean} [force=false]
   *        If true, the file descriptor is closed immediately. If false, the
   *        file descriptor is closed after all current pending IO operations
   *        have completed.
   *
   * @returns {Promise<void>}
   *          Resolves when the file descriptor has been closed.
   */
  close(force = false) {
    if (!force && this.pending.length) {
      this.closing = true;
      return this.closedPromise;
    }

    for (let {reject} of this.pending) {
      let error = new Error("File closed");
      error.errorCode = SubprocessConstants.ERROR_END_OF_FILE;
      reject(error);
    }
    this.pending.length = 0;

    if (!this.closed) {
      this.fd.dispose();

      this.closed = true;
      this.resolveClosed();

      io.pipes.delete(this.id);
      io.updatePollFds();
    }
    return this.closedPromise;
  }

  /**
   * Called when an error occurred while polling our file descriptor.
   */
  onError() {
    this.close(true);
    this.process.wait();
  }
}

class InputPipe extends Pipe {
  /**
   * A bit mask of poll() events which we currently wish to be notified of on
   * this file descriptor.
   */
  get pollEvents() {
    if (this.pending.length) {
      return LIBC.POLLIN;
    }
    return 0;
  }

  /**
   * Asynchronously reads at most `length` bytes of binary data from the file
   * descriptor into an ArrayBuffer of the same size. Returns a promise which
   * resolves when the operation is complete.
   *
   * @param {integer} length
   *        The number of bytes to read.
   *
   * @returns {Promise<ArrayBuffer>}
   */
  read(length) {
    if (this.closing || this.closed) {
      throw new Error("Attempt to read from closed pipe");
    }

    return new Promise((resolve, reject) => {
      this.pending.push({resolve, reject, length});
      io.updatePollFds();
    });
  }

  /**
   * Synchronously reads at most `count` bytes of binary data into an
   * ArrayBuffer, and returns that buffer. If no data can be read without
   * blocking, returns null instead.
   *
   * @param {integer} count
   *        The number of bytes to read.
   *
   * @returns {ArrayBuffer|null}
   */
  readBuffer(count) {
    let buffer = new ArrayBuffer(count);

    let read = +libc.read(this.fd, buffer, buffer.byteLength);
    if (read < 0 && ctypes.errno != LIBC.EAGAIN) {
      this.onError();
    }

    if (read <= 0) {
      return null;
    }

    if (read < buffer.byteLength) {
      return ArrayBuffer.transfer(buffer, read);
    }

    return buffer;
  }


  /**
   * Called when one of the IO operations matching the `pollEvents` mask may be
   * performed without blocking.
   */
  onReady() {
    let reads = this.pending;
    while (reads.length) {
      let {resolve, length} = reads[0];

      let buffer = this.readBuffer(length);
      if (buffer) {
        this.shiftPending();
        resolve(buffer);
      } else {
        break;
      }
    }

    if (reads.length == 0) {
      io.updatePollFds();
    }
  }
}

class OutputPipe extends Pipe {
  /**
   * A bit mask of poll() events which we currently wish to be notified of on
   * this file discriptor.
   */
  get pollEvents() {
    if (this.pending.length) {
      return LIBC.POLLOUT;
    }
    return 0;
  }

  /**
   * Asynchronously writes the given buffer to our file descriptor, and returns
   * a promise which resolves when the operation is complete.
   *
   * @param {ArrayBuffer} buffer
   *        The buffer to write.
   *
   * @returns {Promise<integer>}
   *          Resolves to the number of bytes written when the operation is
   *          complete.
   */
  write(buffer) {
    if (this.closing || this.closed) {
      throw new Error("Attempt to write to closed pipe");
    }

    return new Promise((resolve, reject) => {
      this.pending.push({resolve, reject, buffer, length: buffer.byteLength});
      io.updatePollFds();
    });
  }

  /**
   * Attempts to synchronously write the given buffer to our file descriptor.
   * Writes only as many bytes as can be written without blocking, and returns
   * the number of byes successfully written.
   *
   * Closes the file descriptor if an IO error occurs.
   *
   * @param {ArrayBuffer} buffer
   *        The buffer to write.
   *
   * @returns {integer}
   *          The number of bytes successfully written.
   */
  writeBuffer(buffer) {
    let bytesWritten = libc.write(this.fd, buffer, buffer.byteLength);

    if (bytesWritten < 0 && ctypes.errno != LIBC.EAGAIN) {
      this.onError();
    }

    return bytesWritten;
  }

  /**
   * Called when one of the IO operations matching the `pollEvents` mask may be
   * performed without blocking.
   */
  onReady() {
    let writes = this.pending;
    while (writes.length) {
      let {buffer, resolve, length} = writes[0];

      let written = this.writeBuffer(buffer);

      if (written == buffer.byteLength) {
        resolve(length);
        this.shiftPending();
      } else if (written > 0) {
        writes[0].buffer = buffer.slice(written);
      } else {
        break;
      }
    }

    if (writes.length == 0) {
      io.updatePollFds();
    }
  }
}

class Process extends BaseProcess {
  /**
   * Each Process object opens an additional pipe from the target object, which
   * will be automatically closed when the process exits, but otherwise
   * carries no data.
   *
   * This property contains a bit mask of poll() events which we wish to be
   * notified of on this descriptor. We're not expecting any input from this
   * pipe, but we need to poll for input until the process exits in order to be
   * notified when the pipe closes.
   */
  get pollEvents() {
    if (this.exitCode === null) {
      return LIBC.POLLIN;
    }
    return 0;
  }

  /**
   * Kills the process with the given signal.
   *
   * @param {integer} signal
   */
  kill(signal) {
    libc.kill(this.pid, signal);
    this.wait();
  }

  /**
   * Initializes the IO pipes for use as standard input, output, and error
   * descriptors in the spawned process.
   *
   * @returns {unix.Fd[]}
   *          The array of file descriptors belonging to the spawned process.
   */
  initPipes(options) {
    let stderr = options.stderr;

    let our_pipes = [];
    let their_pipes = new Map();

    let pipe = input => {
      let fds = ctypes.int.array(2)();

      let res = libc.pipe(fds);
      if (res == -1) {
        throw new Error("Unable to create pipe");
      }

      fds = Array.from(fds, unix.Fd);

      if (input) {
        fds.reverse();
      }

      if (input) {
        our_pipes.push(new InputPipe(this, fds[1]));
      } else {
        our_pipes.push(new OutputPipe(this, fds[1]));
      }

      libc.fcntl(fds[0], LIBC.F_SETFD, LIBC.FD_CLOEXEC);
      libc.fcntl(fds[1], LIBC.F_SETFD, LIBC.FD_CLOEXEC);
      libc.fcntl(fds[1], LIBC.F_SETFL, LIBC.O_NONBLOCK);

      return fds[0];
    };

    their_pipes.set(0, pipe(false));
    their_pipes.set(1, pipe(true));

    if (stderr == "pipe") {
      their_pipes.set(2, pipe(true));
    } else if (stderr == "stdout") {
      their_pipes.set(2, their_pipes.get(1));
    }

    // Create an additional pipe that we can use to monitor for process exit.
    their_pipes.set(3, pipe(true));
    this.fd = our_pipes.pop().fd;

    this.pipes = our_pipes;

    return their_pipes;
  }

  spawn(options) {
    let {command, arguments: args} = options;

    let argv = this.stringArray(args);
    let envp = this.stringArray(options.environment);

    let actions = unix.posix_spawn_file_actions_t();
    let actionsp = actions.address();

    let fds = this.initPipes(options);

    let cwd;
    try {
      if (options.workdir) {
        cwd = ctypes.char.array(LIBC.PATH_MAX)();
        libc.getcwd(cwd, cwd.length);

        if (libc.chdir(options.workdir) < 0) {
          throw new Error(`Unable to change working directory to ${options.workdir}`);
        }
      }

      libc.posix_spawn_file_actions_init(actionsp);
      for (let [i, fd] of fds.entries()) {
        libc.posix_spawn_file_actions_adddup2(actionsp, fd, i);
      }

      let pid = unix.pid_t();
      let rv = libc.posix_spawn(pid.address(), command, actionsp, null, argv, envp);

      if (rv != 0) {
        for (let pipe of this.pipes) {
          pipe.close();
        }
        throw new Error(`Failed to execute command "${command}"`);
      }

      this.pid = pid.value;
    } finally {
      libc.posix_spawn_file_actions_destroy(actionsp);

      this.stringArrays.length = 0;

      if (cwd) {
        libc.chdir(cwd);
      }
      for (let fd of new Set(fds.values())) {
        fd.dispose();
      }
    }
  }

  /**
   * Called when input is available on our sentinel file descriptor.
   *
   * @see pollEvents
   */
  onReady() {
    // We're not actually expecting any input on this pipe. If we get any, we
    // can't poll the pipe any further without reading it.
    if (this.wait() == undefined) {
      this.kill(9);
    }
  }

  /**
   * Called when an error occurred while polling our sentinel file descriptor.
   *
   * @see pollEvents
   */
  onError() {
    this.wait();
  }

  /**
   * Attempts to wait for the process's exit status, without blocking. If
   * successful, resolves the `exitPromise` to the process's exit value.
   *
   * @returns {integer|null}
   *          The process's exit status, if it has already exited.
   */
  wait() {
    if (this.exitCode !== null) {
      return this.exitCode;
    }

    let status = ctypes.int();

    let res = libc.waitpid(this.pid, status.address(), LIBC.WNOHANG);
    if (res == this.pid) {
      let sig = unix.WTERMSIG(status.value);
      if (sig) {
        this.exitCode = -sig;
      } else {
        this.exitCode = unix.WEXITSTATUS(status.value);
      }

      this.fd.dispose();
      this.resolveExit(this.exitCode);
      return this.exitCode;
    }
  }
}

io = {
  pollFds: null,
  pollHandlers: null,

  pipes: new Map(),

  processes: new Map(),

  interval: null,

  getPipe(pipeId) {
    let pipe = this.pipes.get(pipeId);

    if (!pipe) {
      let error = new Error("File closed");
      error.errorCode = SubprocessConstants.ERROR_END_OF_FILE;
      throw error;
    }
    return pipe;
  },

  getProcess(processId) {
    let process = this.processes.get(processId);

    if (!process) {
      throw new Error(`Invalid process ID: ${processId}`);
    }
    return process;
  },

  updatePollFds() {
    let handlers = [...this.pipes.values(),
                    ...this.processes.values()];

    handlers = handlers.filter(handler => handler.pollEvents);

    let pollfds = unix.pollfd.array(handlers.length)();

    for (let [i, handler] of handlers.entries()) {
      let pollfd = pollfds[i];

      pollfd.fd = handler.fd;
      pollfd.events = handler.pollEvents;
      pollfd.revents = 0;
    }

    this.pollFds = pollfds;
    this.pollHandlers = handlers;

    if (pollfds.length && !this.interval) {
      this.interval = setInterval(this.poll.bind(this), POLL_INTERVAL);
    } else if (!pollfds.length && this.interval) {
      clearInterval(this.interval);
      this.interval = null;
    }
  },

  poll() {
    let handlers = this.pollHandlers;
    let pollfds = this.pollFds;

    let count = libc.poll(pollfds, pollfds.length, POLL_TIMEOUT);

    for (let i = 0; count && i < pollfds.length; i++) {
      let pollfd = pollfds[i];
      if (pollfd.revents) {
        count--;

        let handler = handlers[i];
        try {
          if (pollfd.revents & handler.pollEvents) {
            handler.onReady();
          }
          if (pollfd.revents & (LIBC.POLLERR | LIBC.POLLHUP | LIBC.POLLNVAL)) {
            handler.onError();
          }
        } catch (e) {
          console.error(e);
          debug(`Worker error: ${e} :: ${e.stack}`);
          handler.onError();
        }

        pollfd.revents = 0;
      }
    }
  },

  addProcess(process) {
    this.processes.set(process.id, process);

    for (let pipe of process.pipes) {
      this.pipes.set(pipe.id, pipe);
    }
  },

  cleanupProcess(process) {
    this.processes.delete(process.id);
  },
};
