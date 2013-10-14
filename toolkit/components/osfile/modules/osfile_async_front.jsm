/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Asynchronous front-end for OS.File.
 *
 * This front-end is meant to be imported from the main thread. In turn,
 * it spawns one worker (perhaps more in the future) and delegates all
 * disk I/O to this worker.
 *
 * Documentation note: most of the functions and methods in this module
 * return promises. For clarity, we denote as follows a promise that may resolve
 * with type |A| and some value |value| or reject with type |B| and some
 * reason |reason|
 * @resolves {A} value
 * @rejects {B} reason
 */

"use strict";

this.EXPORTED_SYMBOLS = ["OS"];

const Cu = Components.utils;
const Ci = Components.interfaces;

let SharedAll = {};
Cu.import("resource://gre/modules/osfile/osfile_shared_allthreads.jsm", SharedAll);
Cu.import("resource://gre/modules/Deprecated.jsm", this);

// Boilerplate, to simplify the transition to require()
let OS = SharedAll.OS;

let LOG = OS.Shared.LOG.bind(OS.Shared, "Controller");

let isTypedArray = OS.Shared.isTypedArray;

// The constructor for file errors.
let OSError;
if (OS.Constants.Win) {
  Cu.import("resource://gre/modules/osfile/osfile_win_allthreads.jsm", this);
  OSError = OS.Shared.Win.Error;
} else if (OS.Constants.libc) {
  Cu.import("resource://gre/modules/osfile/osfile_unix_allthreads.jsm", this);
  OSError = OS.Shared.Unix.Error;
} else {
  throw new Error("I am neither under Windows nor under a Posix system");
}
let Type = OS.Shared.Type;
let Path = {};
Cu.import("resource://gre/modules/osfile/ospath.jsm", Path);

// The library of promises.
Cu.import("resource://gre/modules/Promise.jsm", this);

// The implementation of communications
Cu.import("resource://gre/modules/osfile/_PromiseWorker.jsm", this);

Cu.import("resource://gre/modules/Services.jsm", this);

Cu.import("resource://gre/modules/AsyncShutdown.jsm", this);

LOG("Checking profileDir", OS.Constants.Path);

// If profileDir is not available, osfile.jsm has been imported before the
// profile is setup. In this case, make this a lazy getter.
if (!("profileDir" in OS.Constants.Path)) {
  Object.defineProperty(OS.Constants.Path, "profileDir", {
    get: function() {
      let path = undefined;
      try {
        path = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
        delete OS.Constants.Path.profileDir;
        OS.Constants.Path.profileDir = path;
      } catch (ex) {
        // Ignore errors: profileDir is still not available
      }
      return path;
    }
  });
}

LOG("Checking localProfileDir");

if (!("localProfileDir" in OS.Constants.Path)) {
  Object.defineProperty(OS.Constants.Path, "localProfileDir", {
    get: function() {
      let path = undefined;
      try {
        path = Services.dirsvc.get("ProfLD", Ci.nsIFile).path;
        delete OS.Constants.Path.localProfileDir;
        OS.Constants.Path.localProfileDir = path;
      } catch (ex) {
        // Ignore errors: localProfileDir is still not available
      }
      return path;
    }
  });
}

/**
 * Return a shallow clone of the enumerable properties of an object.
 */
let clone = SharedAll.clone;

let worker = new PromiseWorker(
  "resource://gre/modules/osfile/osfile_async_worker.js", LOG);
let Scheduler = {
  /**
   * |true| once we have sent at least one message to the worker.
   */
  launched: false,

  /**
   * |true| once shutdown has begun i.e. we should reject any
   * message
   */
  shutdown: false,

  /**
   * The latest promise returned.
   */
  latestPromise: Promise.resolve("OS.File scheduler hasn't been launched yet"),

  post: function post(...args) {
    if (!this.launched && OS.Shared.DEBUG) {
      // If we have delayed sending SET_DEBUG, do it now.
      worker.post("SET_DEBUG", [true]);
    }
    this.launched = true;
    if (this.shutdown) {
      LOG("OS.File is not available anymore. The following request has been rejected.", args);
      return Promise.reject(new Error("OS.File has been shut down."));
    }

    // By convention, the last argument of any message may be an |options| object.
    let methodArgs = args[1];
    let options = methodArgs ? methodArgs[methodArgs.length - 1] : null;
    let promise = worker.post.apply(worker, args);
    return this.latestPromise = promise.then(
      function onSuccess(data) {
        // Check for duration and return result.
        if (!options) {
          return data.ok;
        }
        // Check for options.outExecutionDuration.
        if (typeof options !== "object" ||
          !("outExecutionDuration" in options)) {
          return data.ok;
        }
        // If data.durationMs is not present, return data.ok (there was an
        // exception applying the method).
        if (!("durationMs" in data)) {
          return data.ok;
        }
        // Bug 874425 demonstrates that two successive calls to Date.now()
        // can actually produce an interval with negative duration.
        // We assume that this is due to an operation that is so short
        // that Date.now() is not monotonic, so we round this up to 0.
        let durationMs = Math.max(0, data.durationMs);
        // Accumulate (or initialize) outExecutionDuration
        if (typeof options.outExecutionDuration == "number") {
          options.outExecutionDuration += durationMs;
        } else {
          options.outExecutionDuration = durationMs;
        }
        return data.ok;
      },
      function onError(error) {
        // Decode any serialized error
        if (error instanceof PromiseWorker.WorkerError) {
          throw OS.File.Error.fromMsg(error.data);
        }
        // Extract something meaningful from WorkerErrorEvent
        if (typeof error == "object" && error && error.constructor.name == "WorkerErrorEvent") {
          let message = error.message;
          if (message == "uncaught exception: [object StopIteration]") {
            throw StopIteration;
          }
          throw new Error(message, error.filename, error.lineno);
        }
        throw error;
      }
    );
  }
};

const PREF_OSFILE_LOG = "toolkit.osfile.log";
const PREF_OSFILE_LOG_REDIRECT = "toolkit.osfile.log.redirect";

/**
 * Safely read a PREF_OSFILE_LOG preference.
 * Returns a value read or, in case of an error, oldPref or false.
 *
 * @param bool oldPref
 *        An optional value that the DEBUG flag was set to previously.
 */
let readDebugPref = function readDebugPref(prefName, oldPref = false) {
  let pref = oldPref;
  try {
    pref = Services.prefs.getBoolPref(prefName);
  } catch (x) {
    // In case of an error when reading a pref keep it as is.
  }
  // If neither pref nor oldPref were set, default it to false.
  return pref;
};

/**
 * Listen to PREF_OSFILE_LOG changes and update gShouldLog flag
 * appropriately.
 */
Services.prefs.addObserver(PREF_OSFILE_LOG,
  function prefObserver(aSubject, aTopic, aData) {
    OS.Shared.DEBUG = readDebugPref(PREF_OSFILE_LOG, OS.Shared.DEBUG);
    if (Scheduler.launched) {
      // Don't start the worker just to set this preference.
      Scheduler.post("SET_DEBUG", [OS.Shared.DEBUG]);
    }
  }, false);
OS.Shared.DEBUG = readDebugPref(PREF_OSFILE_LOG, false);

Services.prefs.addObserver(PREF_OSFILE_LOG_REDIRECT,
  function prefObserver(aSubject, aTopic, aData) {
    OS.Shared.TEST = readDebugPref(PREF_OSFILE_LOG_REDIRECT, OS.Shared.TEST);
  }, false);
OS.Shared.TEST = readDebugPref(PREF_OSFILE_LOG_REDIRECT, false);

// Update worker's DEBUG flag if it's true.
// Don't start the worker just for this, though.
if (OS.Shared.DEBUG && Scheduler.launched) {
  Scheduler.post("SET_DEBUG", [true]);
}

// Observer topics used for monitoring shutdown
const WEB_WORKERS_SHUTDOWN_TOPIC = "web-workers-shutdown";

// Preference used to configure test shutdown observer.
const PREF_OSFILE_TEST_SHUTDOWN_OBSERVER =
  "toolkit.osfile.test.shutdown.observer";

/**
 * A condition function meant to be used during phase
 * webWorkersShutdown, to warn about unclosed files and directories
 * and reconfigure the Scheduler to reject further requests.
 *
 * @param {bool=} shutdown If true or unspecified, reconfigure
 * the scheduler to reject further requests. Can be set to |false|
 * for testing purposes.
 * @return {promise} A promise satisfied once all pending messages
 * (including the shutdown warning message) have been answered.
 */
function warnAboutUnclosedFiles(shutdown = true) {
  if (!Scheduler.launched) {
    // Don't launch the scheduler on our behalf. If no message has been
    // sent to the worker, we can't have any leaking file/directory
    // descriptor.
    return null;
  }
  // Send a "System_shutdown" message to the worker.
  let promise = Scheduler.post("System_shutdown");

  // Configure the worker to reject any further message.
  if (shutdown) {
    Scheduler.shutdown = true;
  }

  return promise.then(function onSuccess(opened) {
    let msg = "";
    if (opened.openedFiles.length > 0) {
      msg += "The following files are still open:\n" +
        opened.openedFiles.join("\n");
    }
    if (msg) {
      msg += "\n";
    }
    if (opened.openedDirectoryIterators.length > 0) {
      msg += "The following directory iterators are still open:\n" +
        opened.openedDirectoryIterators.join("\n");
    }
    // Only log if file descriptors leaks detected.
    if (msg) {
      LOG("WARNING: File descriptors leaks detected.\n" + msg);
    }
  });
};

AsyncShutdown.webWorkersShutdown.addBlocker(
  "OS.File: flush pending requests, warn about unclosed files, shut down service.",
  () => warnAboutUnclosedFiles(true)
);


// Attaching an observer for PREF_OSFILE_TEST_SHUTDOWN_OBSERVER to enable or
// disable the test shutdown event observer.
// Note: By default the PREF_OSFILE_TEST_SHUTDOWN_OBSERVER is unset.
// Note: This is meant to be used for testing purposes only.
Services.prefs.addObserver(PREF_OSFILE_TEST_SHUTDOWN_OBSERVER,
  function prefObserver() {
    // The temporary phase topic used to trigger the unclosed
    // phase warning.
    let TOPIC = null;
    try {
      TOPIC = Services.prefs.getCharPref(
        PREF_OSFILE_TEST_SHUTDOWN_OBSERVER);
    } catch (x) {
    }
    if (TOPIC) {
      // Generate a phase, add a blocker.
      // Note that this can work only if AsyncShutdown itself has been
      // configured for testing by the testsuite.
      let phase = AsyncShutdown._getPhase(TOPIC);
      phase.addBlocker(
        "(for testing purposes) OS.File: warn about unclosed files",
        () => warnAboutUnclosedFiles(false)
      );
    }
  }, false);

/**
 * Representation of a file, with asynchronous methods.
 *
 * @param {*} fdmsg The _message_ representing the platform-specific file
 * handle.
 *
 * @constructor
 */
let File = function File(fdmsg) {
  // FIXME: At the moment, |File| does not close on finalize
  // (see bug 777715)
  this._fdmsg = fdmsg;
  this._closeResult = null;
  this._closed = null;
};


File.prototype = {
  /**
   * Close a file asynchronously.
   *
   * This method is idempotent.
   *
   * @return {promise}
   * @resolves {null}
   * @rejects {OS.File.Error}
   */
  close: function close() {
    if (this._fdmsg != null) {
      let msg = this._fdmsg;
      this._fdmsg = null;
      return this._closeResult =
        Scheduler.post("File_prototype_close", [msg], this);
    }
    return this._closeResult;
  },

  /**
   * Fetch information about the file.
   *
   * @return {promise}
   * @resolves {OS.File.Info} The latest information about the file.
   * @rejects {OS.File.Error}
   */
  stat: function stat() {
    return Scheduler.post("File_prototype_stat", [this._fdmsg], this).then(
      File.Info.fromMsg
    );
  },

  /**
   * Read a number of bytes from the file and into a buffer.
   *
   * @param {Typed array | C pointer} buffer This buffer will be
   * modified by another thread. Using this buffer before the |read|
   * operation has completed is a BAD IDEA.
   * @param {JSON} options
   *
   * @return {promise}
   * @resolves {number} The number of bytes effectively read.
   * @rejects {OS.File.Error}
   */
  readTo: function readTo(buffer, options = {}) {
    // If |buffer| is a typed array and there is no |bytes| options, we
    // need to extract the |byteLength| now, as it will be lost by
    // communication
    if (isTypedArray(buffer) && !("bytes" in options)) {
      // Preserve reference to option |outExecutionDuration|, if it is passed.
      options = clone(options, ["outExecutionDuration"]);
      options.bytes = buffer.byteLength;
    }
    // Note: Type.void_t.out_ptr.toMsg ensures that
    // - the buffer is effectively shared (not neutered) between both
    //   threads;
    // - we take care of any |byteOffset|.
    return Scheduler.post("File_prototype_readTo",
      [this._fdmsg,
       Type.void_t.out_ptr.toMsg(buffer),
       options],
       buffer/*Ensure that |buffer| is not gc-ed*/);
  },
  /**
   * Write bytes from a buffer to this file.
   *
   * Note that, by default, this function may perform several I/O
   * operations to ensure that the buffer is fully written.
   *
   * @param {Typed array | C pointer} buffer The buffer in which the
   * the bytes are stored. The buffer must be large enough to
   * accomodate |bytes| bytes. Using the buffer before the operation
   * is complete is a BAD IDEA.
   * @param {*=} options Optionally, an object that may contain the
   * following fields:
   * - {number} bytes The number of |bytes| to write from the buffer. If
   * unspecified, this is |buffer.byteLength|. Note that |bytes| is required
   * if |buffer| is a C pointer.
   *
   * @return {number} The number of bytes actually written.
   */
  write: function write(buffer, options = {}) {
    // If |buffer| is a typed array and there is no |bytes| options,
    // we need to extract the |byteLength| now, as it will be lost
    // by communication
    if (isTypedArray(buffer)) {
      // Preserve reference to option |outExecutionDuration|, if it is passed.
      options = clone(options, ["outExecutionDuration"]);
      options.bytes = buffer.byteLength;
    }
    // Note: Type.void_t.out_ptr.toMsg ensures that
    // - the buffer is effectively shared (not neutered) between both
    //   threads;
    // - we take care of any |byteOffset|.
    return Scheduler.post("File_prototype_write",
      [this._fdmsg,
       Type.void_t.in_ptr.toMsg(buffer),
       options],
       buffer/*Ensure that |buffer| is not gc-ed*/);
  },

  /**
   * Read bytes from this file to a new buffer.
   *
   * @param {number=} bytes If unspecified, read all the remaining bytes from
   * this file. If specified, read |bytes| bytes, or less if the file does not
   * contain that many bytes.
   * @param {JSON} options
   * @return {promise}
   * @resolves {Uint8Array} An array containing the bytes read.
   */
  read: function read(nbytes, options = {}) {
    let promise = Scheduler.post("File_prototype_read",
      [this._fdmsg,
       nbytes, options]);
    return promise.then(
      function onSuccess(data) {
        return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
      });
  },

  /**
   * Return the current position in the file, as bytes.
   *
   * @return {promise}
   * @resolves {number} The current position in the file,
   * as a number of bytes since the start of the file.
   */
  getPosition: function getPosition() {
    return Scheduler.post("File_prototype_getPosition",
      [this._fdmsg]);
  },

  /**
   * Set the current position in the file, as bytes.
   *
   * @param {number} pos A number of bytes.
   * @param {number} whence The reference position in the file,
   * which may be either POS_START (from the start of the file),
   * POS_END (from the end of the file) or POS_CUR (from the
   * current position in the file).
   *
   * @return {promise}
   */
  setPosition: function setPosition(pos, whence) {
    return Scheduler.post("File_prototype_setPosition",
      [this._fdmsg, pos, whence]);
  }
};

/**
 * Open a file asynchronously.
 *
 * @return {promise}
 * @resolves {OS.File}
 * @rejects {OS.Error}
 */
File.open = function open(path, mode, options) {
  return Scheduler.post(
    "open", [Type.path.toMsg(path), mode, options],
    path
  ).then(
    function onSuccess(msg) {
      return new File(msg);
    }
  );
};

/**
 * Get the information on the file.
 *
 * @return {promise}
 * @resolves {OS.File.Info}
 * @rejects {OS.Error}
 */
File.stat = function stat(path) {
  return Scheduler.post(
    "stat", [Type.path.toMsg(path)],
    path).then(File.Info.fromMsg);
};

/**
 * Fetch the current directory
 *
 * @return {promise}
 * @resolves {string} The current directory, as a path usable with OS.Path
 * @rejects {OS.Error}
 */
File.getCurrentDirectory = function getCurrentDirectory() {
  return Scheduler.post(
    "getCurrentDirectory"
  ).then(Type.path.fromMsg);
};

/**
 * Change the current directory
 *
 * @param {string} path The OS-specific path to the current directory.
 * You should use the methods of OS.Path and the constants of OS.Constants.Path
 * to build OS-specific paths in a portable manner.
 *
 * @return {promise}
 * @resolves {null}
 * @rejects {OS.Error}
 */
File.setCurrentDirectory = function setCurrentDirectory(path) {
  return Scheduler.post(
    "setCurrentDirectory", [Type.path.toMsg(path)], path
  );
};

/**
 * Copy a file to a destination.
 *
 * @param {string} sourcePath The platform-specific path at which
 * the file may currently be found.
 * @param {string} destPath The platform-specific path at which the
 * file should be copied.
 * @param {*=} options An object which may contain the following fields:
 *
 * @option {bool} noOverwrite - If true, this function will fail if
 * a file already exists at |destPath|. Otherwise, if this file exists,
 * it will be erased silently.
 *
 * @rejects {OS.File.Error} In case of any error.
 *
 * General note: The behavior of this function is defined only when
 * it is called on a single file. If it is called on a directory, the
 * behavior is undefined and may not be the same across all platforms.
 *
 * General note: The behavior of this function with respect to metadata
 * is unspecified. Metadata may or may not be copied with the file. The
 * behavior may not be the same across all platforms.
*/
File.copy = function copy(sourcePath, destPath, options) {
  return Scheduler.post("copy", [Type.path.toMsg(sourcePath),
    Type.path.toMsg(destPath), options], [sourcePath, destPath]);
};

/**
 * Move a file to a destination.
 *
 * @param {string} sourcePath The platform-specific path at which
 * the file may currently be found.
 * @param {string} destPath The platform-specific path at which the
 * file should be moved.
 * @param {*=} options An object which may contain the following fields:
 *
 * @option {bool} noOverwrite - If set, this function will fail if
 * a file already exists at |destPath|. Otherwise, if this file exists,
 * it will be erased silently.
 *
 * @returns {Promise}
 * @rejects {OS.File.Error} In case of any error.
 *
 * General note: The behavior of this function is defined only when
 * it is called on a single file. If it is called on a directory, the
 * behavior is undefined and may not be the same across all platforms.
 *
 * General note: The behavior of this function with respect to metadata
 * is unspecified. Metadata may or may not be moved with the file. The
 * behavior may not be the same across all platforms.
 */
File.move = function move(sourcePath, destPath, options) {
  return Scheduler.post("move", [Type.path.toMsg(sourcePath),
    Type.path.toMsg(destPath), options], [sourcePath, destPath]);
};

/**
 * Remove an empty directory.
 *
 * @param {string} path The name of the directory to remove.
 * @param {*=} options Additional options.
 *   - {bool} ignoreAbsent If |true|, do not fail if the
 *     directory does not exist yet.
 */
File.removeEmptyDir = function removeEmptyDir(path, options) {
  return Scheduler.post("removeEmptyDir",
    [Type.path.toMsg(path), options], path);
};

/**
 * Remove an existing file.
 *
 * @param {string} path The name of the file.
 */
File.remove = function remove(path) {
  return Scheduler.post("remove",
    [Type.path.toMsg(path)]);
};



/**
 * Create a directory.
 *
 * @param {string} path The name of the directory.
 * @param {*=} options Additional options.
 * Implementations may interpret the following fields:
 *
 * - {C pointer} winSecurity If specified, security attributes
 * as per winapi function |CreateDirectory|. If unspecified,
 * use the default security descriptor, inherited from the
 * parent directory.
 * - {bool} ignoreExisting If |true|, do not fail if the
 * directory already exists.
 */
File.makeDir = function makeDir(path, options) {
  return Scheduler.post("makeDir",
    [Type.path.toMsg(path), options], path);
};

/**
 * Return the contents of a file
 *
 * @param {string} path The path to the file.
 * @param {number=} bytes Optionally, an upper bound to the number of bytes
 * to read.
 * @param {JSON} options Additional options.
 * - {boolean} sequential A flag that triggers a population of the page cache
 * with data from a file so that subsequent reads from that file would not
 * block on disk I/O. If |true| or unspecified, inform the system that the
 * contents of the file will be read in order. Otherwise, make no such
 * assumption. |true| by default.
 *
 * @resolves {Uint8Array} A buffer holding the bytes
 * read from the file.
 */
File.read = function read(path, bytes, options) {
  let promise = Scheduler.post("read",
    [Type.path.toMsg(path), bytes, options], path);
  return promise.then(
    function onSuccess(data) {
      return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
    });
};

/**
 * Find outs if a file exists.
 *
 * @param {string} path The path to the file.
 *
 * @return {bool} true if the file exists, false otherwise.
 */
File.exists = function exists(path) {
  return Scheduler.post("exists",
    [Type.path.toMsg(path)], path);
};

/**
 * Write a file, atomically.
 *
 * By opposition to a regular |write|, this operation ensures that,
 * until the contents are fully written, the destination file is
 * not modified.
 *
 * Limitation: In a few extreme cases (hardware failure during the
 * write, user unplugging disk during the write, etc.), data may be
 * corrupted. If your data is user-critical (e.g. preferences,
 * application data, etc.), you may wish to consider adding options
 * |tmpPath| and/or |flush| to reduce the likelihood of corruption, as
 * detailed below. Note that no combination of options can be
 * guaranteed to totally eliminate the risk of corruption.
 *
 * @param {string} path The path of the file to modify.
 * @param {Typed Array | C pointer} buffer A buffer containing the bytes to write.
 * @param {*=} options Optionally, an object determining the behavior
 * of this function. This object may contain the following fields:
 * - {number} bytes The number of bytes to write. If unspecified,
 * |buffer.byteLength|. Required if |buffer| is a C pointer.
 * - {string} tmpPath If |null| or unspecified, write all data directly
 * to |path|. If specified, write all data to a temporary file called
 * |tmpPath| and, once this write is complete, rename the file to
 * replace |path|. Performing this additional operation is a little
 * slower but also a little safer.
 * - {bool} noOverwrite - If set, this function will fail if a file already
 * exists at |path|.
 * - {bool} flush - If |false| or unspecified, return immediately once the
 * write is complete. If |true|, before writing, force the operating system
 * to write its internal disk buffers to the disk. This is considerably slower
 * (not just for the application but for the whole system) but also safer:
 * if the system shuts down improperly (typically due to a kernel freeze
 * or a power failure) or if the device is disconnected before the buffer
 * is flushed, the file has more chances of not being corrupted.
 *
 * @return {promise}
 * @resolves {number} The number of bytes actually written.
 */
File.writeAtomic = function writeAtomic(path, buffer, options = {}) {
  // Copy |options| to avoid modifying the original object but preserve the
  // reference to |outExecutionDuration| option if it is passed.
  options = clone(options, ["outExecutionDuration"]);
  // As options.tmpPath is a path, we need to encode it as |Type.path| message
  if ("tmpPath" in options) {
    options.tmpPath = Type.path.toMsg(options.tmpPath);
  };
  if (isTypedArray(buffer) && (!("bytes" in options))) {
    options.bytes = buffer.byteLength;
  };
  // Note: Type.void_t.out_ptr.toMsg ensures that
  // - the buffer is effectively shared (not neutered) between both
  //   threads;
  // - we take care of any |byteOffset|.
  return Scheduler.post("writeAtomic",
    [Type.path.toMsg(path),
     Type.void_t.in_ptr.toMsg(buffer),
     options], [options, buffer]);
};

File.removeDir = function(path, options = {}) {
  return Scheduler.post("removeDir",
    [Type.path.toMsg(path), options], path);
};

/**
 * Information on a file, as returned by OS.File.stat or
 * OS.File.prototype.stat
 *
 * @constructor
 */
File.Info = function Info(value) {
  // Note that we can't just do this[k] = value[k] because our
  // prototype defines getters for all of these fields.
  for (let k in value) {
    if (k != "creationDate") {
      Object.defineProperty(this, k, {value: value[k]});
    }
  }
  Object.defineProperty(this, "_deprecatedCreationDate", {value: value["creationDate"]});
};
if (OS.Constants.Win) {
  File.Info.prototype = Object.create(OS.Shared.Win.AbstractInfo.prototype);
} else if (OS.Constants.libc) {
  File.Info.prototype = Object.create(OS.Shared.Unix.AbstractInfo.prototype);
} else {
  throw new Error("I am neither under Windows nor under a Posix system");
}

// Deprecated
Object.defineProperty(File.Info.prototype, "creationDate", {
  get: function creationDate() {
    Deprecated.warning("Field 'creationDate' is deprecated.", "https://developer.mozilla.org/en-US/docs/JavaScript_OS.File/OS.File.Info#Cross-platform_Attributes");
    return this._deprecatedCreationDate;
  }
});

File.Info.fromMsg = function fromMsg(value) {
  return new File.Info(value);
};

/**
 * Get worker's current DEBUG flag.
 * Note: This is used for testing purposes.
 */
File.GET_DEBUG = function GET_DEBUG() {
  return Scheduler.post("GET_DEBUG");
};

/**
 * Iterate asynchronously through a directory
 *
 * @constructor
 */
let DirectoryIterator = function DirectoryIterator(path, options) {
  /**
   * Open the iterator on the worker thread
   *
   * @type {Promise}
   * @resolves {*} A message accepted by the methods of DirectoryIterator
   * in the worker thread
   * @rejects {StopIteration} If all entries have already been visited
   * or the iterator has been closed.
   */
  this._itmsg = Scheduler.post(
    "new_DirectoryIterator", [Type.path.toMsg(path), options],
    path
  );
  this._isClosed = false;
};
DirectoryIterator.prototype = {
  iterator: function () this,
  __iterator__: function () this,

  /**
   * Determine whether the directory exists.
   *
   * @resolves {boolean}
   */
  exists: function exists() {
    return this._itmsg.then(
      function onSuccess(iterator) {
        return Scheduler.post("DirectoryIterator_prototype_exists", [iterator]);
      }
    );
  },
  /**
   * Get the next entry in the directory.
   *
   * @return {Promise}
   * @resolves {OS.File.Entry}
   * @rejects {StopIteration} If all entries have already been visited.
   */
  next: function next() {
    let self = this;
    let promise = this._itmsg;

    // Get the iterator, call _next
    promise = promise.then(
      function withIterator(iterator) {
        return self._next(iterator);
      });

    return promise;
  },
  /**
   * Get several entries at once.
   *
   * @param {number=} length If specified, the number of entries
   * to return. If unspecified, return all remaining entries.
   * @return {Promise}
   * @resolves {Array} An array containing the |length| next entries.
   */
  nextBatch: function nextBatch(size) {
    if (this._isClosed) {
      return Promise.resolve([]);
    }
    let promise = this._itmsg;
    promise = promise.then(
      function withIterator(iterator) {
        return Scheduler.post("DirectoryIterator_prototype_nextBatch", [iterator, size]);
      });
    promise = promise.then(
      function withEntries(array) {
        return array.map(DirectoryIterator.Entry.fromMsg);
      });
    return promise;
  },
  /**
   * Apply a function to all elements of the directory sequentially.
   *
   * @param {Function} cb This function will be applied to all entries
   * of the directory. It receives as arguments
   *  - the OS.File.Entry corresponding to the entry;
   *  - the index of the entry in the enumeration;
   *  - the iterator itself - return |iterator.close()| to stop the loop.
   *
   * If the callback returns a promise, iteration waits until the
   * promise is resolved before proceeding.
   *
   * @return {Promise} A promise resolved once the loop has reached
   * its end.
   */
  forEach: function forEach(cb, options) {
    if (this._isClosed) {
      return Promise.resolve();
    }

    let self = this;
    let position = 0;
    let iterator;

    // Grab iterator
    let promise = this._itmsg.then(
      function(aIterator) {
        iterator = aIterator;
      }
    );

    // Then iterate
    let loop = function loop() {
      if (self._isClosed) {
        return Promise.resolve();
      }
      return self._next(iterator).then(
        function onSuccess(value) {
          return Promise.resolve(cb(value, position++, self)).then(loop);
        },
        function onFailure(reason) {
          if (reason == StopIteration) {
            return;
          }
          throw reason;
        }
      );
    };

    return promise.then(loop);
  },
  /**
   * Auxiliary method: fetch the next item
   *
   * @rejects {StopIteration} If all entries have already been visited
   * or the iterator has been closed.
   */
  _next: function _next(iterator) {
    if (this._isClosed) {
      return this._itmsg;
    }
    let self = this;
    let promise = Scheduler.post("DirectoryIterator_prototype_next", [iterator]);
    promise = promise.then(
      DirectoryIterator.Entry.fromMsg,
      function onReject(reason) {
        if (reason == StopIteration) {
          self.close();
          throw StopIteration;
        }
        throw reason;
      });
    return promise;
  },
  /**
   * Close the iterator
   */
  close: function close() {
    if (this._isClosed) {
      return Promise.resolve();
    }
    this._isClosed = true;
    let self = this;
    return this._itmsg.then(
      function withIterator(iterator) {
        self._itmsg = Promise.reject(StopIteration);
        return Scheduler.post("DirectoryIterator_prototype_close", [iterator]);
      }
    );
  }
};

DirectoryIterator.Entry = function Entry(value) {
  return value;
};
if (OS.Constants.Win) {
  DirectoryIterator.Entry.prototype = Object.create(OS.Shared.Win.AbstractEntry.prototype);
} else if (OS.Constants.libc) {
  DirectoryIterator.Entry.prototype = Object.create(OS.Shared.Unix.AbstractEntry.prototype);
} else {
  throw new Error("I am neither under Windows nor under a Posix system");
}

DirectoryIterator.Entry.fromMsg = function fromMsg(value) {
  return new DirectoryIterator.Entry(value);
};

// Constants
Object.defineProperty(File, "POS_START", {value: OS.Shared.POS_START});
Object.defineProperty(File, "POS_CURRENT", {value: OS.Shared.POS_CURRENT});
Object.defineProperty(File, "POS_END", {value: OS.Shared.POS_END});

OS.File = File;
OS.File.Error = OSError;
OS.File.DirectoryIterator = DirectoryIterator;
OS.Path = Path;


// Auto-flush OS.File during profile-before-change. This ensures that any I/O
// that has been queued *before* profile-before-change is properly completed.
// To ensure that I/O queued *during* profile-before-change is completed,
// clients should register using AsyncShutdown.addBlocker.
AsyncShutdown.profileBeforeChange.addBlocker(
  "OS.File: flush I/O queued before profile-before-change",
  () =>
    // Wait until the latest currently enqueued promise is satisfied/rejected
    Scheduler.latestPromise.then(null,
      function onError() { /* ignore error */})
);
