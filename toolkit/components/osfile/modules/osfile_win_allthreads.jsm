/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This module defines the thread-agnostic components of the Win version
 * of OS.File. It depends on the thread-agnostic cross-platform components
 * of OS.File.
 *
 * It serves the following purposes:
 * - open kernel32;
 * - define OS.Shared.Win.Error;
 * - define a few constants and types that need to be defined on all platforms.
 *
 * This module can be:
 * - opened from the main thread as a jsm module;
 * - opened from a chrome worker through require().
 */

"use strict";

var SharedAll;
if (typeof Components != "undefined") {
  let Cu = Components.utils;
  // Module is opened as a jsm module
  Cu.import("resource://gre/modules/ctypes.jsm", this);

  SharedAll = {};
  Cu.import("resource://gre/modules/osfile/osfile_shared_allthreads.jsm", SharedAll);
  this.exports = {};
} else if (typeof module != "undefined" && typeof require != "undefined") {
  // Module is loaded with require()
  SharedAll = require("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
} else {
  throw new Error("Please open this module with Component.utils.import or with require()");
}

var LOG = SharedAll.LOG.bind(SharedAll, "Win", "allthreads");
var Const = SharedAll.Constants.Win;

// Open libc
var libc = new SharedAll.Library("libc", "kernel32.dll");
exports.libc = libc;

// Define declareFFI
var declareFFI = SharedAll.declareFFI.bind(null, libc);
exports.declareFFI = declareFFI;

var Scope = {};

// Define Error
libc.declareLazy(Scope, "FormatMessage",
                 "FormatMessageW", ctypes.winapi_abi,
                 /*return*/ ctypes.uint32_t,
                 /*flags*/  ctypes.uint32_t,
                 /*source*/ ctypes.voidptr_t,
                 /*msgid*/  ctypes.uint32_t,
                 /*langid*/ ctypes.uint32_t,
                 /*buf*/    ctypes.char16_t.ptr,
                 /*size*/   ctypes.uint32_t,
                 /*Arguments*/ctypes.voidptr_t);

/**
 * A File-related error.
 *
 * To obtain a human-readable error message, use method |toString|.
 * To determine the cause of the error, use the various |becauseX|
 * getters. To determine the operation that failed, use field
 * |operation|.
 *
 * Additionally, this implementation offers a field
 * |winLastError|, which holds the OS-specific error
 * constant. If you need this level of detail, you may match the value
 * of this field against the error constants of |OS.Constants.Win|.
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
var OSError = function OSError(operation = "unknown operation",
                               lastError = ctypes.winLastError, path = "") {
  operation = operation;
  SharedAll.OSError.call(this, operation, path);
  this.winLastError = lastError;
};
OSError.prototype = Object.create(SharedAll.OSError.prototype);
OSError.prototype.toString = function toString() {
  let buf = new (ctypes.ArrayType(ctypes.char16_t, 1024))();
  let result = Scope.FormatMessage(
    Const.FORMAT_MESSAGE_FROM_SYSTEM |
    Const.FORMAT_MESSAGE_IGNORE_INSERTS,
    null,
    /* The error number */ this.winLastError,
    /* Default language */ 0,
    /* Output buffer*/     buf,
    /* Minimum size of buffer */ 1024,
    /* Format args*/       null
  );
  if (!result) {
    buf = "additional error " +
      ctypes.winLastError +
      " while fetching system error message";
  }
  return "Win error " + this.winLastError + " during operation "
    + this.operation + (this.path? " on file " + this.path : "") +
    " (" + buf.readString() + ")";
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
    return this.winLastError == Const.ERROR_FILE_EXISTS ||
      this.winLastError == Const.ERROR_ALREADY_EXISTS;
  }
});
/**
 * |true| if the error was raised because a file or directory
 * does not exist, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseNoSuchFile", {
  get: function becauseNoSuchFile() {
    return this.winLastError == Const.ERROR_FILE_NOT_FOUND ||
      this.winLastError == Const.ERROR_PATH_NOT_FOUND;
  }
});
/**
 * |true| if the error was raised because a directory is not empty
 * does not exist, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseNotEmpty", {
  get: function becauseNotEmpty() {
    return this.winLastError == Const.ERROR_DIR_NOT_EMPTY;
  }
});
/**
 * |true| if the error was raised because a file or directory
 * is closed, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseClosed", {
  get: function becauseClosed() {
    return this.winLastError == Const.ERROR_INVALID_HANDLE;
  }
});
/**
 * |true| if the error was raised because permission is denied to
 * access a file or directory, |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseAccessDenied", {
  get: function becauseAccessDenied() {
    return this.winLastError == Const.ERROR_ACCESS_DENIED;
  }
});
/**
 * |true| if the error was raised because some invalid argument was passed,
 * |false| otherwise.
 */
Object.defineProperty(OSError.prototype, "becauseInvalidArgument", {
  get: function becauseInvalidArgument() {
    return this.winLastError == Const.ERROR_NOT_SUPPORTED ||
           this.winLastError == Const.ERROR_BAD_ARGUMENTS;
  }
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
    winLastError: error.winLastError,
    path: error.path
  };
};

/**
 * Deserialize a message back to an instance of OSError
 */
OSError.fromMsg = function fromMsg(msg) {
  let error = new OSError(msg.operation, msg.winLastError, msg.path);
  error.stack = msg.stack;
  error.fileName = msg.fileName;
  error.lineNumber = msg.lineNumber;
  return error;
};
exports.Error = OSError;

/**
 * Code shared by implementation of File.Info on Windows
 *
 * @constructor
 */
var AbstractInfo = function AbstractInfo(path, isDir, isSymLink, size,
                                         winBirthDate,
                                         lastAccessDate, lastWriteDate,
                                         winAttributes) {
  this._path = path;
  this._isDir = isDir;
  this._isSymLink = isSymLink;
  this._size = size;
  this._winBirthDate = winBirthDate;
  this._lastAccessDate = lastAccessDate;
  this._lastModificationDate = lastWriteDate;
  this._winAttributes = winAttributes;
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
   * |true| if this file is a symbolic link, |false| otherwise
   */
  get isSymLink() {
    return this._isSymLink;
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
  // Deprecated
  get creationDate() {
    return this._winBirthDate;
  },
  /**
   * The date of creation of this file.
   *
   * @type {Date}
   */
  get winBirthDate() {
    return this._winBirthDate;
  },
  /**
   * The date of last access to this file.
   *
   * Note that the definition of last access may depend on the underlying
   * operating system and file system.
   *
   * @type {Date}
   */
  get lastAccessDate() {
    return this._lastAccessDate;
  },
  /**
   * The date of last modification of this file.
   *
   * Note that the definition of last access may depend on the underlying
   * operating system and file system.
   *
   * @type {Date}
   */
  get lastModificationDate() {
    return this._lastModificationDate;
  },
  /**
   * The Object with following boolean properties of this file.
   * {readOnly, system, hidden}
   *
   * @type {object}
   */
  get winAttributes() {
    return this._winAttributes;
  }
};
exports.AbstractInfo = AbstractInfo;

/**
 * Code shared by implementation of File.DirectoryIterator.Entry on Windows
 *
 * @constructor
 */
var AbstractEntry = function AbstractEntry(isDir, isSymLink, name,
                                           winCreationDate, winLastWriteDate,
                                           winLastAccessDate, path) {
  this._isDir = isDir;
  this._isSymLink = isSymLink;
  this._name = name;
  this._winCreationDate = winCreationDate;
  this._winLastWriteDate = winLastWriteDate;
  this._winLastAccessDate = winLastAccessDate;
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
   * |true| if the entry is a symbolic link, |false| otherwise
   */
  get isSymLink() {
    return this._isSymLink;
  },
  /**
   * The name of the entry.
   * @type {string}
   */
  get name() {
    return this._name;
  },
  /**
   * The creation time of this file.
   * @type {Date}
   */
  get winCreationDate() {
    return this._winCreationDate;
  },
  /**
   * The last modification time of this file.
   * @type {Date}
   */
  get winLastWriteDate() {
    return this._winLastWriteDate;
  },
  /**
   * The last access time of this file.
   * @type {Date}
   */
  get winLastAccessDate() {
    return this._winLastAccessDate;
  },
  /**
   * The full path of the entry
   * @type {string}
   */
  get path() {
    return this._path;
  }
};
exports.AbstractEntry = AbstractEntry;

// Special constants that need to be defined on all platforms

exports.POS_START = Const.FILE_BEGIN;
exports.POS_CURRENT = Const.FILE_CURRENT;
exports.POS_END = Const.FILE_END;

// Special types that need to be defined for communication
// between threads
var Type = Object.create(SharedAll.Type);
exports.Type = Type;

/**
 * Native paths
 *
 * Under Windows, expressed as wide strings
 */
Type.path = Type.wstring.withName("[in] path");
Type.out_path = Type.out_wstring.withName("[out] path");

// Special constructors that need to be defined on all threads
OSError.closed = function closed(operation, path) {
  return new OSError(operation, Const.ERROR_INVALID_HANDLE, path);
};

OSError.exists = function exists(operation, path) {
  return new OSError(operation, Const.ERROR_FILE_EXISTS, path);
};

OSError.noSuchFile = function noSuchFile(operation, path) {
  return new OSError(operation, Const.ERROR_FILE_NOT_FOUND, path);
};

OSError.invalidArgument = function invalidArgument(operation) {
  return new OSError(operation, Const.ERROR_NOT_SUPPORTED);
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
  "POS_END"
];

//////////// Boilerplate
if (typeof Components != "undefined") {
  this.EXPORTED_SYMBOLS = EXPORTED_SYMBOLS;
  for (let symbol of EXPORTED_SYMBOLS) {
    this[symbol] = exports[symbol];
  }
}
