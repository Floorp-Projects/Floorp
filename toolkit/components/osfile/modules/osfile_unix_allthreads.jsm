/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module defines the thread-agnostic components of the Unix version
 * of OS.File. It depends on the thread-agnostic cross-platform components
 * of OS.File.
 *
 * It serves the following purposes:
 * - open libc;
 * - define OS.Unix.Error;
 * - define a few constants and types that need to be defined on all platforms.
 *
 * This module can be:
 * - opened from the main thread as a jsm module;
 * - opened from a chrome worker through require().
 */

"use strict";

let SharedAll;
if (typeof Components != "undefined") {
  let Cu = Components.utils;
  // Module is opened as a jsm module
  Cu.import("resource://gre/modules/ctypes.jsm", this);

  SharedAll = {};
  Cu.import("resource://gre/modules/osfile/osfile_shared_allthreads.jsm", SharedAll);
  this.exports = {};
} else if (typeof "module" != "undefined" && typeof "require" != "undefined") {
  // Module is loaded with require()
  SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
} else {
  throw new Error("Please open this module with Component.utils.import or with require()");
}

let LOG = SharedAll.LOG.bind(SharedAll, "Unix", "allthreads");
let Const = SharedAll.Constants.libc;

// Open libc
let libc;
let libc_candidates =  [ "libc.so",
                         "libSystem.B.dylib",
                         "a.out" ];
for (let i = 0; i < libc_candidates.length; ++i) {
  try {
    libc = ctypes.open(libc_candidates[i]);
    break;
  } catch (x) {
    LOG("Could not open libc ", libc_candidates[i]);
  }
}

if (!libc) {
  // Note: If you change the string here, please adapt tests accordingly
  throw new Error("Could not open system library: no libc");
}
exports.libc = libc;

// Define declareFFI
let declareFFI = SharedAll.declareFFI.bind(null, libc);
exports.declareFFI = declareFFI;

// Define Error
let strerror = libc.declare("strerror",
  ctypes.default_abi,
  /*return*/ ctypes.char.ptr,
  /*errnum*/ ctypes.int);

/**
 * A File-related error.
 *
 * To obtain a human-readable error message, use method |toString|.
 * To determine the cause of the error, use the various |becauseX|
 * getters. To determine the operation that failed, use field
 * |operation|.
 *
 * Additionally, this implementation offers a field
 * |unixErrno|, which holds the OS-specific error
 * constant. If you need this level of detail, you may match the value
 * of this field against the error constants of |OS.Constants.libc|.
 *
 * @param {string=} operation The operation that failed. If unspecified,
 * the name of the calling function is taken to be the operation that
 * failed.
 * @param {number=} lastError The OS-specific constant detailing the
 * reason of the error. If unspecified, this is fetched from the system
 * status.
 *
 * @constructor
 * @extends {OS.Shared.Error}
 */
let OSError = function OSError(operation, errno) {
  operation = operation || "unknown operation";
  SharedAll.OSError.call(this, operation);
  this.unixErrno = errno || ctypes.errno;
};
OSError.prototype = Object.create(SharedAll.OSError.prototype);
OSError.prototype.toString = function toString() {
  return "Unix error " + this.unixErrno +
    " during operation " + this.operation +
    " (" + strerror(this.unixErrno).readString() + ")";
};

/**
 * |true| if the error was raised because a file or directory
 * already exists, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseExists", {
  get: function becauseExists() {
    return this.unixErrno == Const.EEXIST;
  }
});
/**
 * |true| if the error was raised because a file or directory
 * does not exist, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseNoSuchFile", {
  get: function becauseNoSuchFile() {
    return this.unixErrno == Const.ENOENT;
  }
});

/**
 * |true| if the error was raised because a directory is not empty
 * does not exist, |false| otherwise.
 */
 Object.defineProperty(OSError.prototype, "becauseNotEmpty", {
   get: function becauseNotEmpty() {
     return this.unixErrno == Const.ENOTEMPTY;
   }
 });
/**
 * |true| if the error was raised because a file or directory
 * is closed, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseClosed", {
  get: function becauseClosed() {
    return this.unixErrno == Const.EBADF;
  }
});
/**
 * |true| if the error was raised because permission is denied to
 * access a file or directory, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseAccessDenied", {
  get: function becauseAccessDenied() {
    return this.unixErrno == Const.EACCES;
  }
});

/**
 * Serialize an instance of OSError to something that can be
 * transmitted across threads (not necessarily a string).
 */
OSError.toMsg = function toMsg(error) {
  return {
    operation: error.operation,
    unixErrno: error.unixErrno
  };
};

/**
 * Deserialize a message back to an instance of OSError
 */
OSError.fromMsg = function fromMsg(msg) {
  return new OSError(msg.operation, msg.unixErrno);
};
exports.Error = OSError;

/**
 * Code shared by implementations of File.Info on Unix
 *
 * @constructor
*/
let AbstractInfo = function AbstractInfo(isDir, isSymLink, size, lastAccessDate,
                                         lastModificationDate, unixLastStatusChangeDate,
                                         unixOwner, unixGroup, unixMode) {
  this._isDir = isDir;
  this._isSymlLink = isSymLink;
  this._size = size;
  this._lastAccessDate = lastAccessDate;
  this._lastModificationDate = lastModificationDate;
  this._unixLastStatusChangeDate = unixLastStatusChangeDate;
  this._unixOwner = unixOwner;
  this._unixGroup = unixGroup;
  this._unixMode = unixMode;
};

AbstractInfo.prototype = {
  /**
   * |true| if this file is a directory, |false| otherwise
   */
  get isDir() {
    return this._isDir;
  },
  /**
   * |true| if this file is a symbolink link, |false| otherwise
   */
  get isSymLink() {
    return this._isSymlLink;
  },
  /**
   * The size of the file, in bytes.
   *
   * Note that the result may be |NaN| if the size of the file cannot be
   * represented in JavaScript.
   *
   * @type {number}
   */
  get size() {
    return this._size;
  },
  /**
   * The date of last access to this file.
   *
   * Note that the definition of last access may depend on the
   * underlying operating system and file system.
   *
   * @type {Date}
   */
  get lastAccessDate() {
    return this._lastAccessDate;
  },
  /**
   * Return the date of last modification of this file.
   */
  get lastModificationDate() {
    return this._lastModificationDate;
  },
  /**
   * Return the date at which the status of this file was last modified
   * (this is the date of the latest write/renaming/mode change/...
   * of the file)
   */
  get unixLastStatusChangeDate() {
    return this._unixLastStatusChangeDate;
  },
  /*
   * Return the Unix owner of this file
   */
  get unixOwner() {
    return this._unixOwner;
  },
  /*
   * Return the Unix group of this file
   */
  get unixGroup() {
    return this._unixGroup;
  },
  /*
   * Return the Unix group of this file
   */
  get unixMode() {
    return this._unixMode;
  }
};
exports.AbstractInfo = AbstractInfo;

/**
 * Code shared by implementations of File.DirectoryIterator.Entry on Unix
 *
 * @constructor
*/
let AbstractEntry = function AbstractEntry(isDir, isSymLink, name, path) {
  this._isDir = isDir;
  this._isSymlLink = isSymLink;
  this._name = name;
  this._path = path;
};

AbstractEntry.prototype = {
  /**
   * |true| if the entry is a directory, |false| otherwise
   */
  get isDir() {
    return this._isDir;
  },
  /**
   * |true| if the entry is a directory, |false| otherwise
   */
  get isSymLink() {
    return this._isSymlLink;
  },
  /**
   * The name of the entry
   * @type {string}
   */
  get name() {
    return this._name;
  },
  /**
   * The full path to the entry
   */
  get path() {
    return this._path;
  }
};
exports.AbstractEntry = AbstractEntry;

// Special constants that need to be defined on all platforms

exports.POS_START = Const.SEEK_SET;
exports.POS_CURRENT = Const.SEEK_CUR;
exports.POS_END = Const.SEEK_END;

// Special types that need to be defined for communication
// between threads
let Type = Object.create(SharedAll.Type);
exports.Type = Type;

/**
 * Native paths
 *
 * Under Unix, expressed as C strings
 */
Type.path = Type.cstring.withName("[in] path");
Type.out_path = Type.out_cstring.withName("[out] path");

// Special constructors that need to be defined on all threads
OSError.closed = function closed(operation) {
  return new OSError(operation, Const.EBADF);
};

OSError.exists = function exists(operation) {
  return new OSError(operation, Const.EEXIST);
};

OSError.noSuchFile = function noSuchFile(operation) {
  return new OSError(operation, Const.ENOENT);
};

let EXPORTED_SYMBOLS = [
  "declareFFI",
  "libc",
  "Error",
  "AbstractInfo",
  "AbstractEntry",
  "Type",
  "POS_START",
  "POS_CURRENT",
  "POS_END"
];

//////////// Boilerplate
if (typeof Components != "undefined") {
  this.EXPORTED_SYMBOLS = EXPORTED_SYMBOLS;
  for (let symbol of EXPORTED_SYMBOLS) {
    this[symbol] = exports[symbol];
  }
}
