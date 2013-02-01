"use strict";

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

this.EXPORTED_SYMBOLS = ["OS"];

Components.utils.import("resource://gre/modules/osfile/osfile_shared_allthreads.jsm", this);

let LOG = OS.Shared.LOG.bind(OS.Shared, "Controller");
let isTypedArray = OS.Shared.isTypedArray;

// A simple flag used to control debugging messages.
// FIXME: Once this library has been battle-tested, this flag will
// either be removed or replaced with a preference.
const DEBUG = false;

// The constructor for file errors.
let OSError;
if (OS.Constants.Win) {
  Components.utils.import("resource://gre/modules/osfile/osfile_win_allthreads.jsm", this);
  Components.utils.import("resource://gre/modules/osfile/ospath_win_back.jsm", this);
  OSError = OS.Shared.Win.Error;
} else if (OS.Constants.libc) {
  Components.utils.import("resource://gre/modules/osfile/osfile_unix_allthreads.jsm", this);
  Components.utils.import("resource://gre/modules/osfile/ospath_unix_back.jsm", this);
  OSError = OS.Shared.Unix.Error;
} else {
  throw new Error("I am neither under Windows nor under a Posix system");
}
let Type = OS.Shared.Type;

// The library of promises.
Components.utils.import("resource://gre/modules/commonjs/promise/core.js", this);

// The implementation of communications
Components.utils.import("resource://gre/modules/osfile/_PromiseWorker.jsm", this);

// If profileDir is not available, osfile.jsm has been imported before the
// profile is setup. In this case, we need to observe "profile-do-change"
// and set OS.Constants.Path.profileDir as soon as it becomes available.
if (!("profileDir" in OS.Constants.Path) || !("localProfileDir" in OS.Constants.Path)) {
  Components.utils.import("resource://gre/modules/Services.jsm", this);
  let observer = function observer() {
    Services.obs.removeObserver(observer, "profile-do-change");

    let profileDir = Services.dirsvc.get("ProfD", Components.interfaces.nsIFile).path;
    OS.Constants.Path.profileDir = profileDir;

    let localProfileDir = Services.dirsvc.get("ProfLD", Components.interfaces.nsIFile).path;
    OS.Constants.Path.localProfileDir = localProfileDir;
  };
  Services.obs.addObserver(observer, "profile-do-change", false);
}

/**
 * Return a shallow clone of the enumerable properties of an object.
 *
 * We use this whenever normalizing options requires making (shallow)
 * changes to an option object. The copy ensures that we do not modify
 * a client-provided object by accident.
 */
let clone = function clone(object) {
  let result = {};
  for (let k in object) {
    result[k] = object[k];
  }
  return result;
};

/**
 * A shared constant used to normalize a set of options to nothing.
 */
const noOptions = {};


let worker = new PromiseWorker(
  "resource://gre/modules/osfile/osfile_async_worker.js",
  DEBUG?LOG:null);
let Scheduler = {
  post: function post(...args) {
    let promise = worker.post.apply(worker, args);
    return promise.then(
      null,
      function onError(error) {
        // Decode any serialized error
        if (error instanceof PromiseWorker.WorkerError) {
          throw OS.File.Error.fromMsg(error.data);
        } else {
          throw error;
        }
      }
    );
  }
};

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
    if (this._fdmsg) {
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
    if (!this._fdmsg) {
      return Promise.reject(OSError.closed("accessing file"));
    }
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
  readTo: function readTo(buffer, options) {
    // If |buffer| is a typed array and there is no |bytes| options, we
    // need to extract the |byteLength| now, as it will be lost by
    // communication
    if (isTypedArray(buffer) && (!options || !"bytes" in options)) {
      options = clone(options || noOptions);
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
  write: function write(buffer, options) {
    // If |buffer| is a typed array and there is no |bytes| options,
    // we need to extract the |byteLength| now, as it will be lost
    // by communication
    if (isTypedArray(buffer) && (!options || !"bytes" in options)) {
      options = clone(options || noOptions);
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
   * @return {promise}
   * @resolves {Uint8Array} An array containing the bytes read.
   */
  read: function read(nbytes) {
    let promise = Scheduler.post("File_prototype_read",
      [this._fdmsg,
       nbytes]);
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
 *
 * @resolves {Uint8Array} A buffer holding the bytes
 * read from the file.
 */
File.read = function read(path, bytes) {
  let promise = Scheduler.post("read",
    [Type.path.toMsg(path), bytes], path);
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
 * Important note: In the current implementation, option |tmpPath|
 * is required. This requirement should disappear as part of bug 793660.
 *
 * @param {string} path The path of the file to modify.
 * @param {Typed Array | C pointer} buffer A buffer containing the bytes to write.
 * @param {*=} options Optionally, an object determining the behavior
 * of this function. This object may contain the following fields:
 * - {number} bytes The number of bytes to write. If unspecified,
 * |buffer.byteLength|. Required if |buffer| is a C pointer.
 * - {string} tmpPath The path at which to write the temporary file.
 * - {bool} noOverwrite - If set, this function will fail if a file already
 * exists at |path|. The |tmpPath| is not overwritten if |path| exist.
 *
 * @return {promise}
 * @resolves {number} The number of bytes actually written.
 */
File.writeAtomic = function writeAtomic(path, buffer, options) {
  // Copy |options| to avoid modifying the original object
  options = clone(options || noOptions);
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

/**
 * Information on a file, as returned by OS.File.stat or
 * OS.File.prototype.stat
 *
 * @constructor
 */
File.Info = function Info(value) {
  return value;
};
if (OS.Constants.Win) {
  File.Info.prototype = Object.create(OS.Shared.Win.AbstractInfo.prototype);
} else if (OS.Constants.libc) {
  File.Info.prototype = Object.create(OS.Shared.Unix.AbstractInfo.prototype);
} else {
  throw new Error("I am neither under Windows nor under a Posix system");
}

File.Info.fromMsg = function fromMsg(value) {
  return new File.Info(value);
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
      LOG("DirectoryIterator._next", "closed");
      return this._itmsg;
    }
    let self = this;
    let promise = Scheduler.post("DirectoryIterator_prototype_next", [iterator]);
    promise = promise.then(
      DirectoryIterator.Entry.fromMsg,
      function onReject(reason) {
        // If the exception is |StopIteration| (which we may determine only
        // from its message...) we need to stop the iteration.
        if (!(reason instanceof WorkerErrorEvent && reason.message == "uncaught exception: [object StopIteration]")) {
          // Any exception other than StopIteration should be propagated as such
          throw reason;
        }
        self.close();
        throw StopIteration;
      });
    return promise;
  },
  /**
   * Close the iterator
   */
  close: function close() {
    if (this._isClosed) {
      return;
    }
    this._isClosed = true;
    let self = this;
    this._itmsg.then(
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
