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
 * - opened from a chrome worker through importScripts.
 */

if (typeof Components != "undefined") {
  // Module is opened as a jsm module
  var EXPORTED_SYMBOLS = ["OS"];
  Components.utils.import("resource://gre/modules/ctypes.jsm");
  Components.utils.import("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
} else {
  // File is included from a chrome worker
  importScripts("resource://gre/modules/osfile/osfile_shared_allthreads.jsm");
}

(function(exports) {
  "use strict";
  if (!exports.OS || !exports.OS.Shared) {
    throw new Error("osfile_unix_allthreads.jsm must be loaded after osfile_shared_allthreads.jsm");
  }
  if (exports.OS.Shared.Unix) {
    // Avoid double inclusion
    return;
  }
  exports.OS.Shared.Unix = {};

  let LOG = OS.Shared.LOG.bind(OS.Shared, "Unix", "allthreads");

  // Open libc
  let libc;
  let libc_candidates =  [ "libsystem.B.dylib",
                           "libc.so.6",
                           "libc.so" ];
  for (let i = 0; i < libc_candidates.length; ++i) {
    try {
      libc = ctypes.open(libc_candidates[i]);
      break;
    } catch (x) {
      LOG("Could not open libc "+libc_candidates[i]);
    }
  }
  if (!libc) {
    throw new Error("Could not open any libc.");
  }
  exports.OS.Shared.Unix.libc = libc;

  // Define declareFFI
  let declareFFI = OS.Shared.declareFFI.bind(null, libc);
  exports.OS.Shared.Unix.declareFFI = declareFFI;

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
    exports.OS.Shared.Error.call(this, operation);
    this.unixErrno = errno || ctypes.errno;
  };
  OSError.prototype = new exports.OS.Shared.Error();
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
      return this.unixErrno == OS.Constants.libc.EEXISTS;
    }
  });
  /**
   * |true| if the error was raised because a file or directory
   * does not exist, |false| otherwise.
   */
  Object.defineProperty(OSError.prototype, "becauseNoSuchFile", {
    get: function becauseNoSuchFile() {
      return this.unixErrno == OS.Constants.libc.ENOENT;
    }
  });

  exports.OS.Shared.Unix.Error = OSError;
})(this);
