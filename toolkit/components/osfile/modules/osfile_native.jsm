/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Native (xpcom) implementation of key OS.File functions
 */

"use strict";

var EXPORTED_SYMBOLS = ["read", "writeAtomic"];

var { Constants } = ChromeUtils.import(
  "resource://gre/modules/osfile/osfile_shared_allthreads.jsm"
);

var SysAll;
if (Constants.Win) {
  SysAll = ChromeUtils.import(
    "resource://gre/modules/osfile/osfile_win_allthreads.jsm"
  );
} else if (Constants.libc) {
  SysAll = ChromeUtils.import(
    "resource://gre/modules/osfile/osfile_unix_allthreads.jsm"
  );
} else {
  throw new Error("I am neither under Windows nor under a Posix system");
}
var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

/**
 * The native service holding the implementation of the functions.
 */
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "Internals",
  "@mozilla.org/toolkit/osfile/native-internals;1",
  "nsINativeOSFileInternalsService"
);

/**
 * Native implementation of OS.File.read
 *
 * This implementation does not handle option |compression|.
 */
var read = function(path, options = {}) {
  // Sanity check on types of options
  if ("encoding" in options && typeof options.encoding != "string") {
    return Promise.reject(new TypeError("Invalid type for option encoding"));
  }
  if ("compression" in options && typeof options.compression != "string") {
    return Promise.reject(new TypeError("Invalid type for option compression"));
  }
  if ("bytes" in options && typeof options.bytes != "number") {
    return Promise.reject(new TypeError("Invalid type for option bytes"));
  }

  return new Promise((resolve, reject) => {
    lazy.Internals.read(
      path,
      options,
      function onSuccess(success) {
        success.QueryInterface(Ci.nsINativeOSFileResult);
        if ("outExecutionDuration" in options) {
          options.outExecutionDuration =
            success.executionDurationMS + (options.outExecutionDuration || 0);
        }
        resolve(success.result);
      },
      function onError(operation, oserror) {
        reject(new SysAll.Error(operation, oserror, path));
      }
    );
  });
};

/**
 * Native implementation of OS.File.writeAtomic.
 * This should not be called when |buffer| is a view with some non-zero byte offset.
 * Does not handle option |compression|.
 */
var writeAtomic = function(path, buffer, options = {}) {
  // Sanity check on types of options - we check only the encoding, since
  // the others are checked inside Internals.writeAtomic.
  if ("encoding" in options && typeof options.encoding !== "string") {
    return Promise.reject(new TypeError("Invalid type for option encoding"));
  }

  if (typeof buffer == "string") {
    // Normalize buffer to a C buffer by encoding it
    let encoding = options.encoding || "utf-8";
    buffer = new TextEncoder(encoding).encode(buffer);
  }

  if (ArrayBuffer.isView(buffer)) {
    // We need to throw an error if it's a buffer with some byte offset.
    if ("byteOffset" in buffer && buffer.byteOffset > 0) {
      return Promise.reject(
        new Error("Invalid non-zero value of Typed Array byte offset")
      );
    }
    buffer = buffer.buffer;
  }

  return new Promise((resolve, reject) => {
    lazy.Internals.writeAtomic(
      path,
      buffer,
      options,
      function onSuccess(success) {
        success.QueryInterface(Ci.nsINativeOSFileResult);
        if ("outExecutionDuration" in options) {
          options.outExecutionDuration =
            success.executionDurationMS + (options.outExecutionDuration || 0);
        }
        resolve(success.result);
      },
      function onError(operation, oserror) {
        reject(new SysAll.Error(operation, oserror, path));
      }
    );
  });
};
