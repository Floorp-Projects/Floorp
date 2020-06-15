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

/* eslint-env node */

"use strict";

var SharedAll;
if (typeof Components != "undefined") {
  // Module is opened as a jsm module
  ChromeUtils.import("resource://gre/modules/ctypes.jsm", this);

  SharedAll = {};
  ChromeUtils.import(
    "resource://gre/modules/osfile/osfile_shared_allthreads.jsm",
    SharedAll
  );
  this.exports = {};
} else if (typeof module != "undefined" && typeof require != "undefined") {
  // Module is loaded with require()
  SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
} else {
  throw new Error(
    "Please open this module with Component.utils.import or with require()"
  );
}

SharedAll.LOG.bind(SharedAll, "Unix", "allthreads");
var Const = SharedAll.Constants.libc;

// Open libc
var libc = new SharedAll.Library(
  "libc",
  "libc.so",
  "libSystem.B.dylib",
  "a.out"
);
exports.libc = libc;

// Define declareFFI
var declareFFI = SharedAll.declareFFI.bind(null, libc);
exports.declareFFI = declareFFI;

// Define lazy binding
var LazyBindings = {};
libc.declareLazy(
  LazyBindings,
  "strerror",
  "strerror",
  ctypes.default_abi,
  /* return*/ ctypes.char.ptr,
  /* errnum*/ ctypes.int
);

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
 * @param {string=} path The file path that manipulated. If unspecified,
 * assign the empty string.
 *
 * @constructor
 * @extends {OS.Shared.Error}
 */
var OSError = function OSError(
  operation = "unknown operation",
  errno = ctypes.errno,
  path = ""
) {
  SharedAll.OSError.call(this, operation, path);
  this.unixErrno = errno;
};
OSError.prototype = Object.create(SharedAll.OSError.prototype);
OSError.prototype.toString = function toString() {
  return (
    "Unix error " +
    this.unixErrno +
    " during operation " +
    this.operation +
    (this.path ? " on file " + this.path : "") +
    " (" +
    LazyBindings.strerror(this.unixErrno).readStringReplaceMalformed() +
    ")"
  );
};
OSError.prototype.toMsg = function toMsg() {
  return OSError.toMsg(this);
};

/**
 * |true| if the error was raised because a file or directory
 * already exists, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseExists", {
  get: function becauseExists() {
    return this.unixErrno == Const.EEXIST;
  },
});
/**
 * |true| if the error was raised because a file or directory
 * does not exist, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseNoSuchFile", {
  get: function becauseNoSuchFile() {
    return this.unixErrno == Const.ENOENT;
  },
});

/**
 * |true| if the error was raised because a directory is not empty
 * does not exist, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseNotEmpty", {
  get: function becauseNotEmpty() {
    return this.unixErrno == Const.ENOTEMPTY;
  },
});
/**
 * |true| if the error was raised because a file or directory
 * is closed, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseClosed", {
  get: function becauseClosed() {
    return this.unixErrno == Const.EBADF;
  },
});
/**
 * |true| if the error was raised because permission is denied to
 * access a file or directory, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseAccessDenied", {
  get: function becauseAccessDenied() {
    return this.unixErrno == Const.EACCES;
  },
});
/**
 * |true| if the error was raised because some invalid argument was passed,
 * |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseInvalidArgument", {
  get: function becauseInvalidArgument() {
    return this.unixErrno == Const.EINVAL;
  },
});

/**
 * Serialize an instance of OSError to something that can be
 * transmitted across threads (not necessarily a string).
 */
OSError.toMsg = function toMsg(error) {
  return {
    exn: "OS.File.Error",
    fileName: error.moduleName,
    lineNumber: error.lineNumber,
    stack: error.moduleStack,
    operation: error.operation,
    unixErrno: error.unixErrno,
    path: error.path,
  };
};

/**
 * Deserialize a message back to an instance of OSError
 */
OSError.fromMsg = function fromMsg(msg) {
  let error = new OSError(msg.operation, msg.unixErrno, msg.path);
  error.stack = msg.stack;
  error.fileName = msg.fileName;
  error.lineNumber = msg.lineNumber;
  return error;
};
exports.Error = OSError;

/**
 * Code shared by implementations of File.Info on Unix
 *
 * @constructor
 */
var AbstractInfo = function AbstractInfo(
  path,
  isDir,
  isSymLink,
  size,
  lastAccessDate,
  lastModificationDate,
  unixLastStatusChangeDate,
  unixOwner,
  unixGroup,
  unixMode
) {
  this._path = path;
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
   * The path of the file, used for error-reporting.
   *
   * @type {string}
   */
  get path() {
    return this._path;
  },
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
  },
};
exports.AbstractInfo = AbstractInfo;

/**
 * Code shared by implementations of File.DirectoryIterator.Entry on Unix
 *
 * @constructor
 */
var AbstractEntry = function AbstractEntry(isDir, isSymLink, name, path) {
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
  },
};
exports.AbstractEntry = AbstractEntry;

// Special constants that need to be defined on all platforms

exports.POS_START = Const.SEEK_SET;
exports.POS_CURRENT = Const.SEEK_CUR;
exports.POS_END = Const.SEEK_END;

// Special types that need to be defined for communication
// between threads
var Type = Object.create(SharedAll.Type);
exports.Type = Type;

/**
 * Native paths
 *
 * Under Unix, expressed as C strings
 */
Type.path = Type.cstring.withName("[in] path");
Type.out_path = Type.out_cstring.withName("[out] path");

// Special constructors that need to be defined on all threads
OSError.closed = function closed(operation, path) {
  return new OSError(operation, Const.EBADF, path);
};

OSError.exists = function exists(operation, path) {
  return new OSError(operation, Const.EEXIST, path);
};

OSError.noSuchFile = function noSuchFile(operation, path) {
  return new OSError(operation, Const.ENOENT, path);
};

OSError.invalidArgument = function invalidArgument(operation) {
  return new OSError(operation, Const.EINVAL);
};

var EXPORTED_SYMBOLS = [
  "declareFFI",
  "libc",
  "Error",
  "AbstractInfo",
  "AbstractEntry",
  "Type",
  "POS_START",
  "POS_CURRENT",
  "POS_END",
];

// ////////// Boilerplate
if (typeof Components != "undefined") {
  this.EXPORTED_SYMBOLS = EXPORTED_SYMBOLS;
  for (let symbol of EXPORTED_SYMBOLS) {
    this[symbol] = exports[symbol];
  }
}
